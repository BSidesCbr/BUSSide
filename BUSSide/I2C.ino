#include "BUSSide.h"
#include <Wire.h>

int
read_I2C_eeprom(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t readsize, count, skipsize;
  uint8_t slaveAddress;
  uint8_t *reply_data;
  uint32_t i;
  
  reply->bs_command = BS_REPLY_I2C_FLASH_DUMP;
  reply_data = (uint8_t *)&reply->bs_reply_data[0];
  slaveAddress = request->bs_request_args[0];
  readsize = request->bs_request_args[1];
  skipsize = request->bs_request_args[2];
  if (readsize > sizeof(reply->bs_reply_data))
    return -1;

  Wire.begin(); 
  
  Wire.beginTransmission(slaveAddress);
  Wire.write((skipsize & 0xff00) >> 8); // send the high byte of the EEPROM memory address
  Wire.write((skipsize & 0x00ff)); // send the low byte
  Wire.endTransmission(); 

  i = 0;
  while (readsize > 0) {
    uint32_t gotRead;
    
    gotRead = Wire.requestFrom(slaveAddress, readsize > 8 ? 8 : readsize);
    readsize -= gotRead;
    count = 0;
    while (count < gotRead) {
      if (Wire.available()) {
        uint8_t data;
      
        data = Wire.read();
        reply_data[i++] = data;
        count++;
      }
      ESP.wdtFeed();
    }
  }
  reply->bs_reply_length = request->bs_request_args[0];
  return 0;
}

/*
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
*/

int
discover_I2C_slaves(struct bs_request_s *request, struct bs_reply_s *reply)
{
  Wire.begin(); 
  reply->bs_command = BS_REPLY_I2C_DISCOVER_SLAVES; 
  reply->bs_reply_length = 0;
  for (int slaveAddress = 0; slaveAddress < 128; slaveAddress++) {
    Wire.beginTransmission(slaveAddress);
    if (Wire.endTransmission() == 0) // if (no errors)
      reply->bs_reply_data[reply->bs_reply_length++] = slaveAddress;
  }
  return 0;
}


