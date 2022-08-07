void dac_init()
{
  pinMode(25, ANALOG);
  CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);
  CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
  SET_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC | RTC_IO_PDAC1_DAC_XPD_FORCE);
}

void adc_init()
{
  pinMode(ADC_pin,INPUT);
  adcAttachPin(ADC_pin); 
  analogSetAttenuation(ADC_11db); 
  CLEAR_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_START_SAR_M);
  SET_PERI_REG_BITS(SENS_SAR_MEAS_START1_REG, SENS_SAR1_EN_PAD, (1 << ADC_channel), SENS_SAR1_EN_PAD_S);
}

void adc_read()
{
   CLEAR_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_START_SAR_M);
   SET_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG,SENS_MEAS1_START_SAR_M);
   while(GET_PERI_REG_MASK( SENS_SAR_MEAS_START1_REG,SENS_MEAS1_DONE_SAR)==0); 
   adc_reading=GET_PERI_REG_BITS2(SENS_SAR_MEAS_START1_REG,SENS_MEAS1_DATA_SAR, SENS_MEAS1_DATA_SAR_S);
}

void oled_init()
{
  //reset OLED display via software
//  pinMode(OLED_RST, OUTPUT);
//  digitalWrite(OLED_RST, LOW);
//  delay(20);
//  digitalWrite(OLED_RST, HIGH);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.clearDisplay();
  display.setCursor(0, 5);
  display.print("Digital");
  display.setCursor(0, 40);
  display.print("Filters");
  display.display();
  display.setTextSize(2);
  delay(1000);
  display.clearDisplay();
}
