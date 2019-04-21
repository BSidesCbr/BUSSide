#include "BUSSide.h"
#include <SPI.h>

#define CS_GPIO             D8

int write_enable(uint32_t spispeed);
int write_disable(uint32_t spispeed);

static uint8_t spi_transfer_byte(int spispeed, int gpio_CS, int gpio_CLK, int gpio_MOSI, int gpio_MISO, uint8_t data)
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

static int spi_bb_send_fast_command(int spispeed, int cs, int clk, int mosi, int miso, uint8_t *out, uint32_t wrsize, uint8_t *in, int rdsize)
{
  pinMode(gpioIndex[cs], OUTPUT);
  pinMode(gpioIndex[clk], OUTPUT);
  pinMode(gpioIndex[mosi], OUTPUT);
  pinMode(gpioIndex[miso], INPUT);
  digitalWrite(gpioIndex[clk], LOW);
  digitalWrite(gpioIndex[mosi], LOW);
  digitalWrite(gpioIndex[cs], HIGH);
  digitalWrite(gpioIndex[cs], LOW);

  for (int i = 0; i < wrsize; i++) {
    (void)spi_transfer_byte(spispeed, gpioIndex[cs], gpioIndex[clk], gpioIndex[mosi], gpioIndex[miso], out[i]);
  }

  for (int i = 0; i < rdsize; i++) {
    in[i] = spi_transfer_byte(spispeed, gpioIndex[cs], gpioIndex[clk], gpioIndex[mosi], gpioIndex[miso], 0x00);
  }
  
  digitalWrite(gpioIndex[cs], HIGH);
  pinMode(gpioIndex[cs], INPUT);
  pinMode(gpioIndex[clk], INPUT);
  pinMode(gpioIndex[mosi], INPUT);
  return 0;
}

static int spi_hw_bb_send_fast_command(int spispeed, int cs, int clk, int mosi, int miso, uint8_t *out, uint32_t wrsize, uint8_t *in, int rdsize)
{
  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  
  for (int i = 0; i < wrsize; i++) {
    (void)SPI.transfer(out[i]);
  }

  for (int i = 0; i < rdsize; i++) {
    in[i] = SPI.transfer(0x00);
  }

  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();

  return 0;
}
static int spi_bb_send_command(int spispeed, int cs, int clk, int mosi, int miso, uint8_t *in, uint8_t *out, int n)
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

struct bs_frame_s*
spi_read_id_bb(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t *request_args;
  uint32_t *reply_data;
  uint32_t cs, clk, mosi, miso;
  int spispeed;
  uint8_t cmd[4] = { 0x9f, 0x00, 000, 0x00 }; 
  uint8_t v[4];

  request_args = (uint32_t *)&request->bs_payload[0];
  spispeed = request_args[0];
  cs = request_args[1] - 1;
  clk = request_args[2] - 1;
  mosi = request_args[3] - 1;
  miso = request_args[4] - 1;

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + 3*4);
  if (reply == NULL)
    return NULL;

  reply->bs_payload_length = 3*4;
  
  spi_bb_send_command(spispeed, cs, clk, mosi, miso, cmd, v, 4);

  reply_data = (uint32_t *)&reply->bs_payload[0];
  reply_data[0] = v[1];
  reply_data[1] = v[2];
  reply_data[2] = v[3];
  return reply;
}

struct bs_frame_s*
spi_command_finder(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t *request_args, *reply_data;
  uint32_t spispeed;
  uint32_t cs, clk, mosi, miso;
  uint32_t count;
  
  request_args = (uint32_t *)&request->bs_payload[0];
  spispeed = request_args[0];
  cs = request_args[1] - 1;
  clk = request_args[2] - 1;
  mosi = request_args[3] - 1;
  miso = request_args[4] - 1;

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + 50*6);
  if (reply == NULL)
    return NULL;

  reply_data = (uint32_t *)&reply->bs_payload[0];  
  count = 0;
  for (int i = 0; i < 256; i++) {
    uint8_t cmd[6] = { i, 0x00, 0x00, 0x00, 0x00, 0x00 }; 
    uint8_t v[6] = { 0 };

    spi_bb_send_command(spispeed, cs, clk, mosi, miso, cmd, v, 5);
    
    if (v[1] == 0 && v[2] == 0 && v[3] == 0 && v[4] == 0)
      continue;
    if (v[1] == 0xff && v[2] == 0xff && v[3] == 0xff && v[4] == 0xff)
      continue;

    if (count < 40) {
      reply_data[count*6 + 0] = i;
      reply_data[count*6 + 1] = v[1];
      reply_data[count*6 + 2] = v[2];
      reply_data[count*6 + 3] = v[3];
      reply_data[count*6 + 4] = v[4];
      reply_data[count*6 + 5] = v[5];
      count++;
    }
  }
  reply->bs_payload_length = count*4*6;
  return reply;
}

struct bs_frame_s*
spi_discover(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t *request_args, *reply_data;
  uint32_t spispeed;
  uint32_t index;
  
  request_args = (uint32_t *)&request->bs_payload[0];
  spispeed = request_args[0];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + 4*4*50);
  if (reply == NULL)
    return NULL;

  reply_data = (uint32_t *)&reply->bs_payload[0];
  index = 0;
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
          if (index < 50) {
            reply_data[4*index + 0] = cs + 1;
            reply_data[4*index + 1] = clk + 1;
            reply_data[4*index + 2] = mosi + 1;
            reply_data[4*index + 3] = miso + 1;
            index++;
          }
        }
      }
    }
  }
  reply->bs_payload_length = index*4*4;
  return reply;
}

struct bs_frame_s*
SPI_read_id(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t *request_args, *reply_data;
  uint32_t spispeed;
  uint8_t *data;

  request_args = (uint32_t *)&request->bs_payload[0];
  spispeed = request_args[0];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + 4*3);
  if (reply == NULL)
    return NULL;

  reply_data = (uint32_t *)&reply->bs_payload[0];
  
  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x9f);
  reply_data[0] = SPI.transfer(0x00);
  reply_data[1] = SPI.transfer(0x00);
  reply_data[2] = SPI.transfer(0x00);
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();
  reply->bs_payload_length = 4*3;
  return reply;
}

struct bs_frame_s*
hw_send_SPI_command(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t *request_args;
  uint32_t readsize, skipsize, spispeed;
  uint32_t cmdsize;
  uint8_t *cmd;
  uint8_t *data;

  request_args = (uint32_t *)&request->bs_payload[0];
  cmdsize = request_args[0];
  spispeed = request_args[1];
  cmd = (uint8_t *)&request_args[2];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + cmdsize);
  if (reply == NULL)
    return NULL;
    
  data = (uint8_t *)&reply->bs_payload[0];

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
  reply->bs_payload_length = cmdsize;
  return reply;
}

struct bs_frame_s*
send_SPI_fast_command(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t cmdsize;
  uint8_t *cmd;
  uint8_t *data;
  uint32_t spispeed;
  uint32_t wrsize, rdsize;
  uint32_t cs, clk, mosi, miso;
  uint32_t *request_args;

  request_args = (uint32_t *)&request->bs_payload[0];

  spispeed = request_args[0];
  cs = request_args[1] - 1;
  clk = request_args[2] - 1;
  mosi = request_args[3] - 1;
  miso = request_args[4] - 1;
  wrsize = request_args[5];
  rdsize = request_args[6];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + rdsize);
  if (reply == NULL)
    return NULL;

  data = (uint8_t *)&reply->bs_payload[0];
  cmd = (uint8_t *)&request_args[7];
  
  spi_hw_bb_send_fast_command(spispeed, cs, clk, mosi, miso, cmd, wrsize, data, rdsize);
  reply->bs_payload_length = rdsize;
  return reply;
}

struct bs_frame_s*
send_SPI_command(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t cmdsize;
  uint8_t *cmd;
  uint8_t *data;
  uint32_t spispeed;
  uint32_t cs, clk, mosi, miso;
  uint32_t *reply_data;
  uint32_t *request_args;

  request_args = (uint32_t *)&request->bs_payload[0];

  spispeed = request_args[0];
  cs = request_args[1] - 1;
  clk = request_args[2] - 1;
  mosi = request_args[3] - 1;
  miso = request_args[4] - 1;
  cmdsize = request_args[5];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + cmdsize);
  if (reply == NULL)
    return NULL;

  reply_data = (uint32_t *)&reply->bs_payload[0];
  
  cmd = (uint8_t *)alloca(cmdsize);
  data = (uint8_t *)alloca(cmdsize);
  for (int i = 0; i < cmdsize; i++) { 
    cmd[i] = request_args[6 + i];
  }
  spi_bb_send_command(spispeed, cs, clk, mosi, miso, cmd, data, cmdsize);
  for (int i = 0; i < cmdsize; i++) {
    reply_data[i] = data[i];
  }
  reply->bs_payload_length = cmdsize*4;
  return reply;
}

struct bs_frame_s*
read_SPI_flash(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t readsize, skipsize, spispeed;
  uint8_t *data;
  uint32_t *request_args;

  request_args = (uint32_t *)&request->bs_payload[0];
  
  readsize = request_args[0];
  skipsize = request_args[1];
  spispeed = request_args[2];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + readsize);
  if (reply == NULL)
    return NULL;
  data = (uint8_t *)&reply->bs_payload[0];
    
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
  reply->bs_payload_length = readsize;
  return reply;
}

struct bs_frame_s*
erase_sector_SPI_flash(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t skipsize, spispeed;
  uint8_t *data;
  uint32_t *request_args;

  request_args = (uint32_t *)&request->bs_payload[0];
  
  spispeed = request_args[0];
  skipsize = request_args[1];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE);
  if (reply == NULL)
    return NULL;
    
  write_enable(spispeed);
  
  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x20); // sector erase
  (void)SPI.transfer((skipsize & 0xff0000) >> 16);
  (void)SPI.transfer((skipsize & 0x00ff00) >>  8);
  (void)SPI.transfer((skipsize & 0x0000ff));
  delay(25); // Tse is 25 milliseconds
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();

  write_disable(spispeed);
  reply->bs_payload_length = 0;
  return reply;
}

int
write_disable(uint32_t spispeed)
{
  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x04); // disable write status register  
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();
  return 0;
}

int
write_enable(uint32_t spispeed)
{
  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x06); // WREN enable write status register  
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();
  return 0;

}

struct bs_frame_s*
disable_write_protection(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint8_t s1, s2;
  uint32_t spispeed;
  uint32_t *request_args;

  request_args = (uint32_t *)&request->bs_payload[0];
  spispeed = request_args[0];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE);
  if (reply == NULL)
    return NULL;
    
  pinMode(CS_GPIO, OUTPUT);
  
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x05); // read status register 1
  s1 = SPI.transfer(0x00);
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();

  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x35); // read status register 2
  s2 = SPI.transfer(0x00);
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();

  s1 &= ~(1 << 2); // clear BP0
  s1 &= ~(1 << 3); // clear BP1
  s1 &= ~(1 << 4); // clear BP2
  
  write_enable(spispeed);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);  
  (void)SPI.transfer(0x01); // write status register
  (void)SPI.transfer(s1);
  (void)SPI.transfer(s2);
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();
  delay(15);
  write_disable(spispeed);

  reply->bs_payload_length = 0;
  return reply;
}


struct bs_frame_s*
enable_write_protection(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint8_t s1, s2;
  uint32_t spispeed;
  uint32_t *request_args;

  request_args = (uint32_t *)&request->bs_payload[0];
  spispeed = request_args[0];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE);
  if (reply == NULL)
    return NULL;
    
  pinMode(CS_GPIO, OUTPUT);
  
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x05); // read status register 1
  s1 = SPI.transfer(0x00);
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();

  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x35); // read status register 2
  s2 = SPI.transfer(0x00);
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();

  s1 |= (1 << 2); // set BP0
  s1 |= (1 << 3); // set BP1
  s1 |= (1 << 4); // set BP2
  
  write_enable(spispeed);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);  
  (void)SPI.transfer(0x01); // write status register
  (void)SPI.transfer(s1);
  (void)SPI.transfer(s2);
  digitalWrite(CS_GPIO, HIGH);
  SPI.endTransaction();
  SPI.end();
  delay(15);
  write_disable(spispeed);

  reply->bs_payload_length = 0;
  return reply;
}

struct bs_frame_s*
write_SPI_flash(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t *request_args;
  uint32_t writesize, skipsize, spispeed;
  uint8_t *data;

  request_args = (uint32_t *)&request->bs_payload[0];
  writesize = request_args[0];
  skipsize = request_args[1];
  spispeed = request_args[2];
  if (writesize != 256)
    return NULL;
  data = (uint8_t *)&request_args[3];

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE);
  if (reply == NULL)
    return NULL;
    
  write_enable(spispeed);
  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(spispeed, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_GPIO, LOW);
  (void)SPI.transfer(0x02); // write program page
  (void)SPI.transfer((skipsize & 0xff0000) >> 16);
  (void)SPI.transfer((skipsize & 0x00ff00) >>  8);
  (void)SPI.transfer((skipsize & 0x0000ff));
  for (int i = 0 ; i < 256; i++, writesize--) {
    SPI.transfer(data[i]);
    ESP.wdtFeed();
  }
  digitalWrite(CS_GPIO, HIGH);
  delay(3);
  SPI.endTransaction();
  SPI.end();
  write_disable(spispeed);
  reply->bs_payload_length = 0;
  return reply;
}

struct bs_frame_s*
read_SPI_flash_bitbang(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t *request_args;
  uint32_t readsize, skipsize, spispeed, cs, clk, mosi, miso;
  uint8_t *data;

  request_args = (uint32_t *)&request->bs_payload[0];
  readsize = request_args[0];
  skipsize = request_args[1];
  spispeed = request_args[2];
  cs = request_args[3] - 1;
  clk = request_args[4] - 1;
  mosi = request_args[5] - 1;
  miso = request_args[6] - 1;

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + readsize);
  if (reply == NULL)
    return NULL;
  data = (uint8_t *)&reply->bs_payload[0];

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
  reply->bs_payload_length = readsize;
  return reply;
}
