#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "driver/adc.h"
#include "soc/sens_reg.h"
#include <Wire.h>
#include <Adafruit_SSD1306.h>

//declarations
#define OLED_SDA 21
#define OLED_SCL 22
//#define OLED_RST 16
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

uint16_t i,j;
bool cnt = false, test_running = false;

hw_timer_t * timerF = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR offtime() {
  cnt = true;
}

#define ADC_pin 33
static const adc1_channel_t ADC_channel = ADC1_CHANNEL_5;

#define DAC_PIN 25


//common variables
String test_type,com_type;
float output;
uint16_t point=0;
const uint16_t send_size = 256;
int16_t input_vec[send_size]={0},output_vec[send_size]={0};

//fir variables
const uint16_t fir_max_size = 510;
uint16_t fir_filter_len,fir_fs = 10000;
float fir_filter[fir_max_size];
float inp_fir[fir_max_size];
uint16_t adc_reading = 0;
float output_fir;

//iir variables
const uint16_t iir_max_size = 20;
uint16_t iir_filter_len,iir_fs = 10000;
double output_iir;
double num_coeff[iir_max_size]={0},den_coeff[iir_max_size]={0};
double inp_iir[iir_max_size]={0},out_iir[iir_max_size]={0};

//conv variables
const uint16_t conv_max_size = 256;
uint16_t conv_size,conv_fs = 10000;
uint8_t dual_read_status = 0;
int16_t conv_in1[2*conv_max_size]={0},conv_in2[2*conv_max_size]={0};
int32_t conv_out[2*conv_max_size] = {0};

//fft variables
struct complex {
    float real, imag;
};
const uint16_t fft_max_size = 256;
uint16_t fft_size,fft_fs = 10000;
float fft_in[fft_max_size],samples_abs[fft_max_size]={0};
float temp_real,temp_imag;
int steps,lower,upper,skip;
struct complex twiddle[fft_max_size>>1];
struct complex samples[fft_max_size]={};
int bit_reverse[fft_max_size];

//goertzel variables
uint16_t goertz_fs = 10000;
uint16_t freq_arr[] = {697, 770, 852, 941, 1209, 1336, 1447, 1633};
uint16_t freq_bin[8];
float goertz_out[8]={0};
float wo,coeff,s_n,s_n1,s_n2;
uint8_t decode_sum,decode_arr[8],decode_arr1[8];
uint8_t row,col;
bool decode_blank,decode_display;
char keymap[4][4]=
{{'1', '2', '3', 'A'},
 {'4', '5', '6', 'B'},
 {'7', '8', '9', 'C'},
 {'*', '0', '#', 'D'}};

//sig_gen variables
uint16_t sig_gen_type,sig_gen_skip,sig_gen_timer,sig_gen_fs = 10000;
uint8_t sig_gen_array[256];

void send_data(String data_type){
  if (data_type=="FDATA"){
    for(int i=point-1;i>=0;i--){
      Serial.println(input_vec[i]);
      Serial.println(output_vec[i]);
    }
    for(int i=send_size-1;i>=point;i--){
      Serial.println(input_vec[i]);
      Serial.println(output_vec[i]);
    }
  }
  if (data_type == "ODATA"){
    if (test_type == "CONV"){
      for(i=0;i<conv_size;i++){
        Serial.println(conv_in1[i]);
        Serial.println(conv_in2[i]);        
      }
      for(i=0;i<2*conv_size-1;i++)
        Serial.println(conv_out[i]);
    }

    if (test_type == "FFT"){
      for(i=0;i<fft_size;i++)
        Serial.println(samples_abs[i],5);
    }
  }
}

void set_func(String data_type){
  timerAlarmDisable(timerF);
  String temp_data;
  if(data_type=="FIR"){
    temp_data = Serial.readStringUntil('\n');
    fir_filter_len = temp_data.toInt();
    temp_data = Serial.readStringUntil('\n');
    fir_fs = temp_data.toInt();
    for(i=0;i<fir_filter_len;i++){
      temp_data = Serial.readStringUntil('\n');
      fir_filter[i]=temp_data.toFloat();
    }  
    timerAlarmWrite(timerF, (int)(40000000/fir_fs) , true);
    test_type = "FIR";
  }
    if(data_type=="IIR"){
    temp_data = Serial.readStringUntil('\n');
    iir_filter_len = temp_data.toInt();
    temp_data = Serial.readStringUntil('\n');
    iir_fs = temp_data.toInt();
    for(i=0;i<iir_filter_len;i++){
      temp_data = Serial.readStringUntil('\n');
      num_coeff[i]=temp_data.toDouble();
      temp_data = Serial.readStringUntil('\n');
      den_coeff[i]=temp_data.toDouble();
    }   
    timerAlarmWrite(timerF, (int)(40000000/iir_fs) , true);
    test_type = "IIR";
  }

   if (data_type =="CONV"){
    temp_data = Serial.readStringUntil('\n');
    conv_size = temp_data.toInt();
    temp_data = Serial.readStringUntil('\n');
    conv_fs = temp_data.toInt();
    temp_data = Serial.readStringUntil('\n');
    dual_read_status = temp_data.toInt();
    timerAlarmWrite(timerF, (int)(40000000/conv_fs) , true);
    test_type = "CONV";
   }
   
   if (data_type =="FFT"){
    temp_data = Serial.readStringUntil('\n');
    fft_size = temp_data.toInt();
    temp_data = Serial.readStringUntil('\n');
    fft_fs = temp_data.toInt();
    timerAlarmWrite(timerF, (int)(40000000/fft_fs) , true);
    test_type = "FFT";
    bitReverse(bit_reverse,fft_size);
    twid(twiddle,fft_size);
   }
   if (data_type =="GOERTZ"){
    goertz_fs = 10000;
    for(int j=0;j<8;j++)
      freq_bin[j] = round(float(freq_arr[j])/goertz_fs*256);
          
    timerAlarmWrite(timerF, (int)(40000000/goertz_fs) , true);
    test_type = "GOERTZ";
   }
   if (data_type == "SIG_GEN"){
    temp_data = Serial.readStringUntil('\n');
    sig_gen_type = temp_data.toInt();
    temp_data = Serial.readStringUntil('\n');
    sig_gen_fs = temp_data.toInt();  
    sig_gen_array_set();
    test_type = "SIG_GEN";
   }
  display.clearDisplay();
  display.setCursor(20, 10);
  display.setTextSize(2);
  display.println(test_type);
  display.display();
  test_running = true;
  timerAlarmEnable(timerF);
}
void setup() {
  Serial.begin(115200);
  pinMode(25,OUTPUT);
  adc_init();
  oled_init();
  dac_init();
  timerF = timerBegin(2, 2, true);      //40 Mhz
  timerAttachInterrupt(timerF, &offtime, true);
  // Fire Interrupt every 1m ticks, so 1s
  timerAlarmWrite(timerF, 40000000/10000, true);
}

void loop() {
  if(test_running){
    if(cnt){
      if(test_type=="SIG_GEN"){
        SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, sig_gen_array[point], RTC_IO_PDAC1_DAC_S); 
        point+=sig_gen_skip;
      }
      else{
        gpio_set_level(GPIO_NUM_25,1);
        if(test_type=="FIR"){
          adc_read();
          input_vec[point]=adc_reading;
          run_fir(); 
          output_vec[point]=int(output);
        }
        else if(test_type=="IIR"){
          adc_read();
          input_vec[point]=adc_reading;
          run_iir();
          output_vec[point]=int(output);
        }
        else if(test_type=="CONV"){
          adc_read();
          conv_in1[point] = adc_reading - 2048;
          conv_in2[point] = adc_reading - 2048;
        }
  
        else if (test_type == "FFT"){
          adc_read();
          fft_in[point] = float(adc_reading-2048)*3.3/4096;
        }
  
        else if (test_type=="GOERTZ"){
          adc_read();
          input_vec[point]=int(adc_reading/100);
        }
        
        point++;
  
        if((test_type=="CONV") & (point == conv_size)){
          conv_data();
          point = 0;
        }
        else if ((test_type=="FFT") & (point == fft_size)){
          fft_data();
          point = 0;
        }
        else if ((test_type=="GOERTZ") & (point == 256)){
          goertz_decode();
        }
      }

      if(point >= 256)
        point = 0;

      cnt = false;
    }
  }
  if(Serial.available()>0){
    gpio_set_level(GPIO_NUM_25,0);
    com_type = Serial.readStringUntil('\n');
    if((com_type=="ODATA")||(com_type=="FDATA"))
      send_data(com_type);
      
    else
      set_func(com_type);
  }
}
