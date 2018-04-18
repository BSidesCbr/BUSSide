#include "BUSSide.h"
#include <Wire.h>

void
read_I2C_eeprom()
{
  uint32_t readsize, count;
  uint8_t slaveAddress;

  Serial.write('.');
  Serial.flush();
  slaveAddress = read_u8();
  readsize = read_size_u32();
  Wire.begin();  
  while (readsize > 0) {
    uint32_t gotRead;
    
    gotRead = Wire.requestFrom(slaveAddress, readsize > 8 ? 8 : readsize);
    readsize -= gotRead;
    count = 0;
    while (count < gotRead) {
      if (Wire.available()) {
        uint8_t data;
      
        data = Wire.read();
        Serial.write(data);
        count++;
      }
      ESP.wdtFeed();
    }
  }
}


void
I2C_active_scan1(int sdaPin, int sclPin)
{
  Wire.begin(pins[sdaPin], pins[sdaPin]);  
  for (int slaveAddress = 0; slaveAddress < 128; slaveAddress++) {
    Wire.beginTransmission(slaveAddress);
    if (Wire.endTransmission() == 0) // if (no errors)
      Serial.printf("Slave address found: 0x%x (sda=%s scl=%s)\n", slaveAddress, pinnames[sdaPin], pinnames[sclPin]);
  }
}

void
I2C_active_scan()
{
  Serial.write('.');
  for (int sda_pin=0; sda_pin < pinslen; sda_pin++) {
    ESP.wdtFeed();
    for(int scl_pin = 0; scl_pin < pinslen; scl_pin++) {
      ESP.wdtFeed();
      if(sda_pin == scl_pin) continue;
      I2C_active_scan1(sda_pin, scl_pin);
    }
  }
  Serial.println(";");
}

void
discover_I2C_slaves()
{
  Serial.write('.');
  Wire.begin();  
  for (int slaveAddress = 0; slaveAddress < 128; slaveAddress++) {
    Wire.beginTransmission(slaveAddress);
    if (Wire.endTransmission() == 0) // if (no errors)
      Serial.printf("--- Slave address found: 0x%x\n", slaveAddress);
  }
  Serial.println(";");
}


