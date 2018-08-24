#include "BUSSide.h"
#include <SPI.h>

#define CS_GPIO             D8

int
SPI_read_id(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t spispeed;
  uint8_t *data;
  
  spispeed = request->bs_request_args[0];

  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x9f);
  reply->bs_reply_data[0] = SPI.transfer(0x00);
  reply->bs_reply_data[1] = SPI.transfer(0x00);
  reply->bs_reply_data[2] = SPI.transfer(0x00);
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();
  return 0;
}

int
send_SPI_command(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t readsize, skipsize, spispeed;
  uint32_t cmdsize;
  uint8_t *cmd;
  uint8_t *data;
  
  cmdsize = request->bs_request_args[0];
  spispeed = request->bs_request_args[1];
  cmd = (uint8_t *)&request->bs_request_args[2];
  data = (uint8_t *)&reply->bs_reply_data[0];

  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  for (int i = 0 ; i < cmdsize; i++) {
    data[i] = SPI.transfer(cmd[i]);
    ESP.wdtFeed();
  }
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();
  reply->bs_reply_length = cmdsize;
  return 0;
}

int
read_SPI_flash(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t readsize, skipsize, spispeed;
  uint8_t *data;
  
  readsize = request->bs_request_args[0];
  skipsize = request->bs_request_args[1];
  spispeed = request->bs_request_args[2];
  if (readsize > sizeof(reply->bs_reply_data))
    return -1;
  data = (uint8_t *)&reply->bs_reply_data[0];
    
  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x03);
  (void)SPI.transfer((skipsize & 0xff0000) >> 16);
  (void)SPI.transfer((skipsize & 0x00ff00) >>  8);
  (void)SPI.transfer((skipsize & 0x0000ff));
  for (int i = 0 ; i < readsize; i++) {
    data[i] = SPI.transfer(0x00);
    ESP.wdtFeed();
  }
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();
  reply->bs_reply_length = readsize;
  return 0;
}

