#include "BUSSide.h"
#include <SPI.h>

#define CS_GPIO             D8

static uint8_t
spi_transfer_byte(int spispeed, int gpio_CS, int gpio_CLK, int gpio_MOSI, int gpio_MISO, uint8_t data)
{
  uint8_t x = 0;

  ESP.wdtFeed();
  digitalWrite(gpio_CLK, LOW);
  
  for (int i = 0x80; i; i >>= 1) {
    if (data & i) {
      digitalWrite(gpio_MOSI, HIGH);
    } else {
      digitalWrite(gpio_MOSI, LOW);
    }
    delay_us(50);
    
    digitalWrite(gpio_CLK, HIGH);
    delay_us(50);  
      
    if(digitalRead(gpio_MISO) == HIGH) {
      x |= i;
    }
    digitalWrite(gpio_CLK, LOW);
  }
  return x;
}

static int
spi_bb_send_command(int spispeed, int cs, int clk, int mosi, int miso, uint8_t *in, uint8_t *out, int n)
{
  pinMode(gpioIndex[cs], OUTPUT);
  pinMode(gpioIndex[clk], OUTPUT);
  pinMode(gpioIndex[mosi], OUTPUT);
  pinMode(gpioIndex[miso], INPUT);
  digitalWrite(gpioIndex[clk], LOW);
  digitalWrite(gpioIndex[mosi], LOW);
  digitalWrite(gpioIndex[cs], HIGH);
  digitalWrite(gpioIndex[cs], LOW);

  for (int i = 0; i < n; i++) {
    out[i] = spi_transfer_byte(spispeed, gpioIndex[cs], gpioIndex[clk], gpioIndex[mosi], gpioIndex[miso], in[i]);
  }
  
  digitalWrite(gpioIndex[cs], HIGH);
  pinMode(gpioIndex[cs], INPUT);
  pinMode(gpioIndex[clk], INPUT);
  pinMode(gpioIndex[mosi], INPUT);
  return 0;
}

int
spi_read_id_bb(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t cs, clk, mosi, miso;
  int spispeed;
  uint8_t cmd[4] = { 0x9f, 0x00, 000, 0x00 }; 
  uint8_t v[4];

  spispeed = request->bs_request_args[0];
  cs = request->bs_request_args[1] - 1;
  clk = request->bs_request_args[2] - 1;
  mosi = request->bs_request_args[3] - 1;
  miso = request->bs_request_args[4] - 1;
  spi_bb_send_command(spispeed, cs, clk, mosi, miso, cmd, v, 4);
  reply->bs_reply_data[0] = v[1];
  reply->bs_reply_data[1] = v[2];
  reply->bs_reply_data[2] = v[3];
  return 0;
}

int
spi_command_finder(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t spispeed;
  uint32_t cs, clk, mosi, miso;
  
  spispeed = request->bs_request_args[0];
  cs = request->bs_request_args[1] - 1;
  clk = request->bs_request_args[2] - 1;
  mosi = request->bs_request_args[3] - 1;
  miso = request->bs_request_args[4] - 1;
  
  reply->bs_reply_length = 0;
  for (int i = 0; i < 256; i++) {
    uint8_t cmd[6] = { i, 0x00, 0x00, 0x00, 0x00, 0x00 }; 
    uint8_t v[6] = { 0 };
    uint32_t index;

    spi_bb_send_command(spispeed, cs, clk, mosi, miso, cmd, v, 5);
    
    if (v[1] == 0 && v[2] == 0 && v[3] == 0 && v[4] == 0)
      continue;
    if (v[1] == 0xff && v[2] == 0xff && v[3] == 0xff && v[4] == 0xff)
      continue;

    index = reply->bs_reply_length;
    if (index < 40) {
      reply->bs_reply_data[index*6 + 0] = i;
      reply->bs_reply_data[index*6 + 1] = v[1];
      reply->bs_reply_data[index*6 + 2] = v[2];
      reply->bs_reply_data[index*6 + 3] = v[3];
      reply->bs_reply_data[index*6 + 4] = v[4];
      reply->bs_reply_data[index*6 + 5] = v[5];
      reply->bs_reply_length++;
    }
  }
  return 0;
}
int
spi_discover(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t spispeed;
  
  spispeed = request->bs_request_args[0];
  reply->bs_reply_length = 0;
  for (int cs = 0; cs < N_GPIO; cs++) {
    for (int clk = 0; clk < N_GPIO; clk++) {
      if (clk == cs)
        continue;
      for (int mosi = 0; mosi < N_GPIO; mosi++) {       
        if (mosi == clk)
          continue;
        if (mosi == cs)
          continue;
        for (int miso = 0; miso < N_GPIO; miso++) {
          uint32_t index;
          uint8_t cmd[4] = { 0x9f, 0x00, 000, 0x00 }; 
          uint8_t v[4];
                 
          if (miso == mosi)
            continue;
          if (miso == clk)
            continue;
          if (miso == cs)
            continue;

          spi_bb_send_command(spispeed, cs, clk, mosi, miso, cmd, v, 4);
  
          if (v[1] == 0 && v[2] == 0 && v[3] == 0)
            continue;
          if (v[1] == 0xff && v[2] == 0xff && v[3] == 0xff)
            continue;
         
          index = reply->bs_reply_length;
          if (index < 50) {
            reply->bs_reply_data[4*index + 0] = cs + 1;
            reply->bs_reply_data[4*index + 1] = clk + 1;
            reply->bs_reply_data[4*index + 2] = mosi + 1;
            reply->bs_reply_data[4*index + 3] = miso + 1;
            reply->bs_reply_length++;
          }
        }
      }
    }
  }
  return 0;
}

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
hw_send_SPI_command(struct bs_request_s *request, struct bs_reply_s *reply)
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
send_SPI_command(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t cmdsize;
  uint8_t *cmd;
  uint8_t *data;
  uint32_t spispeed;
  uint32_t cs, clk, mosi, miso;
  
  spispeed = request->bs_request_args[0];
  cs = request->bs_request_args[1] - 1;
  clk = request->bs_request_args[2] - 1;
  mosi = request->bs_request_args[3] - 1;
  miso = request->bs_request_args[4] - 1;
  cmdsize = request->bs_request_args[5];

  if (cmdsize < 100) {
    cmd = (uint8_t *)alloca(cmdsize);
    data = (uint8_t *)alloca(cmdsize);
    for (int i = 0; i < cmdsize; i++) { 
      cmd[i] = request->bs_request_args[6 + i];
    }
    spi_bb_send_command(spispeed, cs, clk, mosi, miso, cmd, data, cmdsize);
    for (int i = 0; i < cmdsize; i++) {
      reply->bs_reply_data[i] = data[i];
    }
  }
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

int
erase_sector_SPI_flash(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t skipsize, spispeed;
  uint8_t *data;
  
  skipsize = request->bs_request_args[1];
  spispeed = request->bs_request_args[2];

  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x06); // WREN enable write status register
  (void)SPI.transfer(0x20); // sector erase
  (void)SPI.transfer((skipsize & 0xff0000) >> 16);
  (void)SPI.transfer((skipsize & 0x00ff00) >>  8);
  (void)SPI.transfer((skipsize & 0x0000ff));
  delay(25); // Tse is 25 milliseconds
  (void)SPI.transfer(0x04); // WRDI disable write status register
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();
  return 0;
}

int
write_SPI_flash(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t writesize, skipsize, spispeed;
  uint8_t *data;
  
  writesize = request->bs_request_args[0];
  skipsize = request->bs_request_args[1];
  spispeed = request->bs_request_args[2];
  if (writesize > sizeof(reply->bs_reply_data))
    return -1;
  data = (uint8_t *)&reply->bs_reply_data[0];
    
  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x06); // WREN enable write status register
  digitalWrite(CS_GPIO, HIGH);
  delay(25);
  digitalWrite(CS_GPIO, LOW);
  for (int i = 0 ; i < writesize; i++) {
    (void)SPI.transfer(0x02); // write program byte
    (void)SPI.transfer((skipsize & 0xff0000) >> 16);
    (void)SPI.transfer((skipsize & 0x00ff00) >>  8);
    (void)SPI.transfer((skipsize & 0x0000ff));
    SPI.transfer(data[i++]);
    ESP.wdtFeed();
  }
  (void)SPI.transfer(0x04); // WRDI disable write status register
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();
  reply->bs_reply_length = writesize;
  return 0;
}

int
read_SPI_flash_bitbang(struct bs_request_s *request, struct bs_reply_s *reply)
{
  uint32_t readsize, skipsize, spispeed, cs, clk, mosi, miso;
  uint8_t *data;
  
  readsize = request->bs_request_args[0];
  skipsize = request->bs_request_args[1];
  spispeed = request->bs_request_args[2];
  cs = request->bs_request_args[3] - 1;
  clk = request->bs_request_args[4] - 1;
  mosi = request->bs_request_args[5] - 1;
  miso = request->bs_request_args[6] - 1;
  
  if (readsize > sizeof(reply->bs_reply_data))
    return -1;
  data = (uint8_t *)&reply->bs_reply_data[0];

  pinMode(gpioIndex[cs], OUTPUT);
  pinMode(gpioIndex[clk], OUTPUT);
  pinMode(gpioIndex[mosi], OUTPUT);
  pinMode(gpioIndex[miso], INPUT);  
  digitalWrite(gpioIndex[cs], LOW);
  (void)spi_transfer_byte(spispeed, gpioIndex[cs], gpioIndex[clk], gpioIndex[mosi], gpioIndex[miso], 0x03);
  (void)spi_transfer_byte(spispeed, gpioIndex[cs], gpioIndex[clk], gpioIndex[mosi], gpioIndex[miso], (skipsize & 0xff0000) >> 16);
  (void)spi_transfer_byte(spispeed, gpioIndex[cs], gpioIndex[clk], gpioIndex[mosi], gpioIndex[miso], (skipsize & 0x00ff00) >>  8);
  (void)spi_transfer_byte(spispeed, gpioIndex[cs], gpioIndex[clk], gpioIndex[mosi], gpioIndex[miso], (skipsize & 0x0000ff));
  for (int i = 0 ; i < readsize; i++) {
    data[i] = spi_transfer_byte(spispeed, gpioIndex[cs], gpioIndex[clk], gpioIndex[mosi], gpioIndex[miso], 0x00);
    ESP.wdtFeed();
  }
  digitalWrite(gpioIndex[cs], HIGH);
  pinMode(gpioIndex[cs], INPUT);
  pinMode(gpioIndex[clk], INPUT);
  pinMode(gpioIndex[mosi], INPUT);
  reply->bs_reply_length = readsize;
  return 0;
}
