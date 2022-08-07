void sig_gen_array_set()
{
    sig_gen_skip = (int)((sig_gen_fs-1)/1000) +1;
    sig_gen_timer = sig_gen_fs * 256/sig_gen_skip;
    if(sig_gen_type==0)
      for(i=0;i<256;i++)
        sig_gen_array[i]= (int)(sin(2*PI*i/256)*127) + 128;
    else if(sig_gen_type==1)
      for(i=0;i<128;i++){
        sig_gen_array[i]=0*255;
        sig_gen_array[128+i]=1*255;
      }
    else if(sig_gen_type==2)
      for(i=0;i<128;i++){
        sig_gen_array[i]=2*i;
        sig_gen_array[128+i]=(128-i)*2 -1;
      }
    else if(sig_gen_type==3)
      for(i=0;i<256;i++)
        sig_gen_array[i]=i;
    else if (sig_gen_type==3)
      for(i=0;i<256;i++)
        sig_gen_array[i]=random(0,255);
        
    timerAlarmWrite(timerF, (int)(40000000/sig_gen_timer) , true);
}

void run_fir(){
  output_fir=0;
  inp_fir[0]=adc_reading;
  for(i=fir_filter_len-1;i>0;i--){
    output_fir+=fir_filter[i]*inp_fir[i];
    inp_fir[i]=inp_fir[i-1];
  }
  output_fir+=fir_filter[0]*inp_fir[0];
  output = output_fir;
}

void run_iir() {
  output_iir=0;
  inp_iir[0]=adc_reading;
  for (i = iir_filter_len; i > 0; i--) {
    output_iir += num_coeff[i] * inp_iir[i] - den_coeff[i] * out_iir[i];
    out_iir[i] = out_iir[i - 1];
    inp_iir[i] = inp_iir[i - 1];
  }
  output_iir += num_coeff[0] * inp_iir[0];
  out_iir[1] = output_iir;
  output=output_iir;
}

void conv_data() {
    for(i=0;i<conv_size;i++){
      conv_in1[conv_size+i]=0;
      conv_in2[conv_size+i]=0;

      conv_out[i]=0;
      conv_out[conv_size+i]=0;
  }

  for(i=0;i<2*conv_size-1;i++){
    conv_out[i]=0;
    for(j=0;j<conv_size;j++){
      if((i-j+1)>0)
        conv_out[i]+=conv_in1[j]*conv_in2[i-j];
    }
  }
}

void fft_data()
{
  copy_array(samples,fft_in,fft_size);
  fft(samples,fft_size,twiddle);
  absolute(samples,fft_size,samples_abs);
}

void fft(struct complex *Y, int M, struct complex *w)
{
    steps=1;
    do
    {
      steps*=2;

      for(i=0;i<M/steps;i++)
      {
          for(j=0;j<steps/2;j++)
          {
              lower = i*steps+j+steps/2;
              upper = i*steps+j;
              skip=M*j/steps;

              temp_real=Y[lower].real*w[skip].real - Y[lower].imag*w[skip].imag;
              temp_imag=Y[lower].real*w[skip].imag + Y[lower].imag*w[skip].real;
              
              Y[lower].real= Y[upper].real - temp_real;
              Y[lower].imag= Y[upper].imag - temp_imag;
              Y[upper].real= Y[upper].real + temp_real;
              Y[upper].imag= Y[upper].imag + temp_imag;
          }
      }
    }
    while(steps!=M);
}

void absolute(struct complex *inp, int M, float *out)
{
    int i;
    for(i=0;i<M;i++)
        out[i]=(inp[i].real*inp[i].real + inp[i].imag*inp[i].imag);
}

void bitReverse(int *X, int M)
{
    X[0]=0;
    int i,j;
    for(i=M>>1;i>=1;i=i/2){
        for(j=0;j<(M/i)>>1;j++){
            X[j+(M/(2*i))]=X[j]+i;
        }
    }
}

void twid(struct complex *Y,int M)
{
    for(i=0;i<M>>1;i++){
        Y[i].real = cos(2*PI*i/M);
        Y[i].imag = sin(2*PI*i/M);
    }
}

void copy_array(struct complex *out,float *inp, int M)
{
    for(i=0;i<M/2;i++){
        out[i].real=inp[bit_reverse[i]];
        out[i+M/2].real=inp[bit_reverse[i]+1];
        out[i].imag=0;
        out[i+M/2].imag=0;
    }
}
void goertz_decode()
{
  for(int j=0;j<8;j++){
    wo = 2*PI*freq_bin[j]/256;
    coeff = 2*cos(wo);
    s_n1=0; s_n2=0;
    
    for(int k=0; k<256; k++){
      s_n = float(input_vec[k])/256 + coeff*s_n1 - s_n2;
      s_n2=s_n1;
      s_n1=s_n;
    }
    goertz_out[j]= s_n2*s_n2 + s_n1*s_n1 - coeff*s_n1*s_n2;
    decode_arr[j] = ( goertz_out[j] > 10 ) ? 1 : 0;
  }
  decode_sum = 0;
  for(int j=0;j<8;j++)
    decode_sum += decode_arr[j];

  decode_blank = 0;
  if(decode_sum ==0)
    decode_blank = 1;
  else if (decode_sum == 2)
    if((decode_arr[0])||(decode_arr[1])||(decode_arr[2])||(decode_arr[3]))
      if ((decode_arr[4])||(decode_arr[5])||(decode_arr[6])||(decode_arr[7])){
        decode_display=1;
        for(i=0;i<8;i++)
          decode_arr1[i]=decode_arr[i];
      }

  if((decode_blank)&(decode_display)){
    decode_display=0;
    for(i=0;i<4;i++){
      if(decode_arr1[i]==1)
        row=i;
      if(decode_arr1[4+i]==1)
        col=i;    
    }
  display.clearDisplay();
  display.setCursor(0,5);
  display.setTextSize(2);
  display.print("GOERTZEL");
  display.setCursor(30,30);
  display.setTextSize(4);
  display.print(keymap[row][col]);  
  display.display();
  }
}
