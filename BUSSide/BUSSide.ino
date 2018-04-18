#include <pins_arduino.h>
#include "BUSSide.h"
//#include <ESP8266WiFi.h>

#define microsTime()  ((uint32_t)(asm_ccount() - (int32_t)usTicks)/FREQ)

void reset_gpios();

int gpio[N_GPIO];
int gpioIndex[N_GPIO] = { D0, D1, D2, D3, D4, D5, D6, D7, D8 };
char *gpioName[] = { "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8" };

byte pins[] = { D0, D1, D2, D3, D4, D5, D6, D7, D8 };
char * pinnames[] = { "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", };
const byte pinslen = sizeof(pins)/sizeof(pins[0]);  

static  prog_uint32_t crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

unsigned long crc_update(unsigned long crc, byte data)
{
    byte tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = crc_table[tbl_idx & 0x0f] ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = crc_table[tbl_idx & 0x0f] ^ (crc >> 4);
    return crc;
}

unsigned long crc_mem(char *s, int n)
{
  unsigned long crc = ~0L;
  for (int i = 0; i < n; i++)
    crc = crc_update(crc, s[i]);
  crc = ~crc;
  return crc;
}

void
delay_us(int us)
{
  int32_t startTime, endTime;

  startTime = microsTime();
  endTime = us + startTime;
  while (microsTime() < endTime);
}

uint8_t
read_u8()
{
  uint8_t s1;
  while (!Serial.available())
    ESP.wdtFeed();
  s1 = Serial.read();
  return s1;
}

uint32_t
read_size_u32()
{
  uint8_t s1, s2, s3, s4;
  uint32_t readsize;

  s4 = read_u8();
  s3 = read_u8();
  s2 = read_u8();
  s1 = read_u8();
  readsize = s1 + (s2 << 8) + (s3 << 16) + (s4 << 24);
  return readsize;
}

void
write_u8(uint8_t s)
{
  Serial.write(s);
}

void
write_size_u32(uint32 writesize)
{
  write_u8((writesize & 0xff000000) >> 24);
  write_u8((writesize & 0x00ff0000) >> 16);
  write_u8((writesize & 0x0000ff00) >>  8);
  write_u8((writesize & 0x000000ff));
}
void
printHelp()
{
  Serial.println("Commands:");
  Serial.println("\tU\tAutomatically detect UART");
  Serial.println("\ts\tDiscover I2C Slave Devices");
  Serial.println("\ti\tDump I2C EEPROM");
  Serial.println("\tr\tDump SPI Flash");
  Serial.println("\td\tDiscover active pins");
  Serial.println();
}

void
reset_gpios()
{
  int gpioIndex[N_GPIO] = { D0, D1, D2, D3, D4, D5, D6, D7, D8 };
  char *gpioName[N_GPIO] = { "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8" };

  for (int i = 0; i < N_GPIO; i++) {
    pinMode(gpioIndex[i], INPUT);
  }
}

void
setup()
{
//  WiFi.mode(WIFI_OFF);
  reset_gpios();
  Serial.begin(500000);
  while (!Serial);
  Serial.printf("Welcome to the BUSSide!\n");
  usTicks = asm_ccount();
}

static int cmdBufIndex = 0;
static char cmdBuf[20]; 

void
loop()
{
    char ch;

    if (Serial.available() <= 0)
      return;
 
    ch = Serial.read();
    if (ch == -1)
        Serial.printf("--- Serial error\n");
    if (ch != 10 && ch != 13 && ch != '.') {
        Serial.write(ch);
        Serial.flush();
        if (cmdBufIndex < 10)
          cmdBuf[cmdBufIndex++] = ch;
        return;
    }
       
    if (cmdBufIndex == 0 || cmdBuf[0] != '.')
      Serial.println();
  
    if (cmdBufIndex == 0) {
      Serial.print('>');
      Serial.flush();
      return;
    }
    cmdBufIndex = 0;
    
    switch (cmdBuf[0]) {
    case '.':
      return;
      
    case 'd':
      data_discovery();
      break;

    case 'U':
      UART_all_line_settings();
      break;

    case 's':
      discover_I2C_slaves();
      break;

    case 'i':
      read_I2C_eeprom();
      break;
          
    case 'r':
      read_SPI_flash();
      break;  

    case 'j':
      JTAG_scan();
      break;

    case '#':
      break;
      
    case 'h':
    default:
      printHelp();
      break;
    }

    reset_gpios();
    Serial.print('>');
    Serial.flush();
}
