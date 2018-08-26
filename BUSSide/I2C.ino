#include "BUSSide.h"
#include <Wire.h>

int
read_I2C_eeprom(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t readsize, count, skipsize;
  uint8_t slaveAddress;
  uint8_t *reply_data;
  uint32_t i;
  int sdaPin, sclPin;
  
  reply->bs_command = BS_REPLY_I2C_FLASH_DUMP;
  reply_data = (uint8_t *)&reply->bs_reply_data[0];
  slaveAddress = request->bs_request_args[0];
  readsize = request->bs_request_args[1];
  skipsize = request->bs_request_args[2];
  if (readsize > sizeof(reply->bs_reply_data))
    return -1;
  sdaPin = request->bs_request_args[3] - 1;
  sclPin = request->bs_request_args[4] - 1;

  Wire.begin(gpioIndex[sdaPin], gpioIndex[sclPin]);  
  
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

static void
I2C_active_scan1(struct bs_request_s *request, struct bs_reply_s *reply, int sdaPin, int sclPin)
{
  Wire.begin(gpioIndex[sdaPin], gpioIndex[sclPin]);  
  for (int slaveAddress = 0; slaveAddress < 128; slaveAddress++) {
    int rv1, rv2;

    ESP.wdtFeed();
    Wire.beginTransmission(slaveAddress);
    if (Wire.endTransmission() == 0) {
      int index;

      index = reply->bs_reply_length;
      reply->bs_reply_data[2*index + 0] = sdaPin + 1;
      reply->bs_reply_data[2*index + 1] = sclPin + 1;
      reply->bs_reply_length++;
    }
  }
}

int
I2C_active_scan(struct bs_request_s *request, struct bs_reply_s *reply)
{
  reply->bs_reply_length = 0;
  for (int sda_pin=1; sda_pin < N_GPIO; sda_pin++) {
    ESP.wdtFeed();
    for(int scl_pin = 1; scl_pin < N_GPIO; scl_pin++) {
      ESP.wdtFeed();
      if (sda_pin == scl_pin)
        continue;
      I2C_active_scan1(request, reply, sda_pin, scl_pin);
    }
  }
  return 0;
}

int
discover_I2C_slaves(struct bs_request_s *request, struct bs_reply_s *reply)
{
  int sdaPin, sclPin;

  sdaPin = request->bs_request_args[0] - 1;
  sclPin = request->bs_request_args[1] - 1;
  Wire.begin(gpioIndex[sdaPin], gpioIndex[sclPin]);  
  reply->bs_command = BS_REPLY_I2C_DISCOVER_SLAVES; 
  reply->bs_reply_length = 0;
  for (int slaveAddress = 0; slaveAddress < 128; slaveAddress++) {
    ESP.wdtFeed();
    Wire.beginTransmission(slaveAddress);
    if (Wire.endTransmission() == 0) { // if (no errors)
      reply->bs_reply_data[reply->bs_reply_length++] = slaveAddress;
    }
  }
  return 0;
}


