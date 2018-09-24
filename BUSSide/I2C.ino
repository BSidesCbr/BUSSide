#include "BUSSide.h"
#include <Wire.h>

struct bs_frame_s*
read_I2C_eeprom(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t *request_args;
  uint32_t readsize, count, skipsize;
  uint8_t slaveAddress;
  uint8_t *reply_data;
  uint32_t i;
  int sdaPin, sclPin;
  int addressLength;
  
  request_args = (uint32_t *)&request->bs_payload[0];
  slaveAddress = request_args[0];
  readsize = request_args[1];
  skipsize = request_args[2];
  sdaPin = request_args[3] - 1;
  sclPin = request_args[4] - 1;
  addressLength = request_args[5];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + readsize);
  if (reply == NULL)
    return NULL;

  reply->bs_command = BS_REPLY_I2C_FLASH_DUMP;
  reply_data = (uint8_t *)&reply->bs_payload[0];
  
  Wire.begin(gpioIndex[sdaPin], gpioIndex[sclPin]);  
  
  Wire.beginTransmission(slaveAddress);
  switch (addressLength) {
    case 2:
      Wire.write((skipsize & 0xff00) >> 8); // send the high byte of the EEPROM memory address
    case 1:
      Wire.write((skipsize & 0x00ff)); // send the low byte
      break;
    default:
      free(reply);
      return NULL;
  }
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
    }
  }
  reply->bs_payload_length = request_args[1];
  return reply;
}


int
write_byte_I2C_eeprom(uint8_t slaveAddress, uint32_t skipsize, int addressLength, uint32_t val)
{
    Wire.beginTransmission(slaveAddress);
    switch (addressLength) {
      case 2:
        Wire.write((skipsize & 0xff00) >> 8); // send the high byte of the EEPROM memory address
      case 1:
        Wire.write((skipsize & 0x00ff)); // send the low byte
        break;
      default:
        Wire.endTransmission();
        return -1;
    }
    Wire.write(val);
    Wire.endTransmission();
    for (int i = 0; i < 100; i++) {
      Wire.beginTransmission(slaveAddress);
      if (Wire.endTransmission() == 0) {
        return 0;
      }
      delay(10);
    }
}

struct bs_frame_s*
write_I2C_eeprom(struct bs_request_s *request)
{
  uint32_t *request_args;
  struct bs_frame_s *reply;
  uint32_t writesize, skipsize;
  uint8_t slaveAddress;
  uint8_t *request_data;
  int sdaPin, sclPin;
  int addressLength;
  
  request_args = (uint32_t *)&request->bs_payload[0];
  slaveAddress = request_args[0];
  writesize = request_args[1];
  skipsize = request_args[2];
  sdaPin = request_args[3] - 1;
  sclPin = request_args[4] - 1;
  addressLength = request_args[5];
  request_data = (uint8_t *)&request_args[6];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE);
  if (reply == NULL)
    return NULL;

  reply->bs_command = BS_REPLY_I2C_FLASH;

  Wire.begin(gpioIndex[sdaPin], gpioIndex[sclPin]);  
  while (writesize > 0) {
    int retry, rv;
    
    rv = write_byte_I2C_eeprom(slaveAddress, skipsize, addressLength, request_data[skipsize]);
    retry = 0;
    while (rv && retry < 5) {
      rv = write_byte_I2C_eeprom(slaveAddress, skipsize, addressLength, request_data[skipsize]); 
      delay(50);   
      retry++;
    }
    skipsize++;
    writesize--;
    ESP.wdtFeed();
  }
  reply->bs_payload_length = request_args[1];
  return reply;
}

static void
I2C_active_scan1(struct bs_request_s *request, struct bs_reply_s *reply, int sdaPin, int sclPin)
{
  uint32_t *reply_data;
  int numberOfSlaves;
  
  Wire.begin(gpioIndex[sdaPin], gpioIndex[sclPin]);  
  numberOfSlaves = 0;
  for (int slaveAddress = 0; slaveAddress < 128; slaveAddress++) {
    int rv1, rv2;

    ESP.wdtFeed();
    Wire.beginTransmission(slaveAddress);
    if (Wire.endTransmission() == 0) {
      int n;
      int gotitalready;
      
#define BYTESTOREAD 8
      n = Wire.requestFrom(slaveAddress, BYTESTOREAD);
      if (n != BYTESTOREAD)
        continue;
      ESP.wdtFeed();
      numberOfSlaves++;
    }
  }
  if (numberOfSlaves > 0) {
    int index;
    
    for (int i = 0; i < 10; i++)  {
      int slaveCount;

      slaveCount = 0;
      for (int slaveAddress = 0; slaveAddress < 128; slaveAddress++) {
        ESP.wdtFeed();
        Wire.beginTransmission(slaveAddress);
        if (Wire.endTransmission() == 0) { // if (no errors)
          slaveCount++;
        }
      }
      if (slaveCount != numberOfSlaves) {
        return;
      }
    } 
    index = reply->bs_payload_length / 8;
    reply_data = (uint32_t *)&reply->bs_payload[0];
    reply_data[2*index + 0] = sdaPin + 1;
    reply_data[2*index + 1] = sclPin + 1;
    reply->bs_payload_length += 4*2;
  }
}

struct bs_frame_s*
I2C_active_scan(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t *request_args;
  
  reply = (struct bs_reply_s *)malloc(BS_HEADER_SIZE + 4*2*50);
  if (reply == NULL)
    return NULL;

  reply->bs_payload_length = 0;
    
  for (int sda_pin=1; sda_pin < N_GPIO; sda_pin++) {
    ESP.wdtFeed();
    for(int scl_pin = 1; scl_pin < N_GPIO; scl_pin++) {
      ESP.wdtFeed();
      if (sda_pin == scl_pin)
        continue;
      I2C_active_scan1(request, reply, sda_pin, scl_pin);
    }
  }
  return reply;
}

struct bs_frame_s*
discover_I2C_slaves(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t *request_args, *reply_data;
  int sdaPin, sclPin;
  uint32_t count;
  
  request_args = (uint32_t *)&request->bs_payload[0];
  sdaPin = request_args[0] - 1;
  sclPin = request_args[1] - 1;

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + 128*4);
  if (reply == NULL)
    return NULL;

  reply_data = (uint32_t *)&reply->bs_payload[0];
  
  Wire.begin(gpioIndex[sdaPin], gpioIndex[sclPin]);  
  reply->bs_command = BS_REPLY_I2C_DISCOVER_SLAVES; 
  reply->bs_payload_length = 0;
  count = 0;
  for (int slaveAddress = 0; slaveAddress < 128; slaveAddress++) {
    ESP.wdtFeed();
    Wire.beginTransmission(slaveAddress);
    if (Wire.endTransmission() == 0) { // if (no errors)
      reply_data[count++] = slaveAddress;
      reply->bs_payload_length += 4;
    }
  }
  return reply;
}
