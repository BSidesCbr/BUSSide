#include "BUSSide.h"
#include <SPI.h>

#define CS_GPIO             D8

void
read_SPI_flash()
{
  uint32_t readsize, skipsize;
  unsigned long crc = ~0L;
  
  Serial.write('.');
  Serial.flush();
  readsize = read_size_u32();
  skipsize = read_size_u32();
  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x03);
  (void)SPI.transfer((skipsize & 0xff0000) >> 16);
  (void)SPI.transfer((skipsize & 0x00ff00) >>  8);
  (void)SPI.transfer((skipsize & 0x0000ff));
  for (int i = 0 ; i < readsize; i++) {
    uint8_t data;
    
    data = SPI.transfer(0x00);
    Serial.write(data);
    crc = crc_update(crc, data);
    ESP.wdtFeed();
  }
  crc = ~crc;
  write_size_u32(crc);
  Serial.flush();
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();
}

