#include <Boards.h>
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

static uint32_t crc_table[16] = {
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

static uint32_t sequence_number = 1;

void
send_reply(int rv, struct bs_request_s *request, struct bs_reply_s *reply)
{
    reply->bs_sequence_number = request->bs_sequence_number;
    if (rv == 0) {
      reply->bs_checksum = crc_mem((char *)reply, BS_REPLY_SIZE - 4);
      Serial.write((char *)reply, BS_REPLY_SIZE);
    }
}

void
loop()
{
    struct bs_request_s request;
    struct bs_reply_s reply;
    int rv;

    rv = Serial.readBytes((char *)&request, BS_REQUEST_SIZE);
    if (rv == -1)
        return;
    while (Serial.available() > 0)
      (void)Serial.read();

    if (crc_mem((char *)&request, BS_REQUEST_SIZE - 4) != request.bs_checksum) {
      return;
    }
    if (request.bs_sequence_number <= sequence_number)
      return;
    sequence_number = request.bs_sequence_number;

    memset(&reply, 0, sizeof(reply));
    reply.bs_command = request.bs_command;
   
    switch (request.bs_command) {
    case BS_SPI_SEND:
      rv = send_SPI_command(&request, &reply);
      break;
      
    case BS_DATA_DISCOVERY:
      rv = data_discovery(&request, &reply);
      break;

    case BS_UART_DISCOVER_RX:
      rv = data_discovery(&request, &reply);
      if (rv != 0)
        break;
      rv = UART_all_line_settings(&request, &reply);
      break;

    case BS_UART_DISCOVER_TX:
      rv = UART_discover_tx(&request, &reply);
      break;
      
    case BS_UART_PASSTHROUGH:
      rv = 0;
      send_reply(0, &request, &reply);
      (void)UART_passthrough(&request, &reply);
      // no return
      break;
      
    case BS_I2C_DISCOVER_SLAVES:
      rv = discover_I2C_slaves(&request, &reply);
      break;

    case BS_I2C_FLASH_DUMP:
      rv = read_I2C_eeprom(&request, &reply);
      break;

    case BS_I2C_FLASH:
      rv = write_I2C_eeprom(&request, &reply);
      break;
      
    case BS_I2C_DISCOVER:
      rv = I2C_active_scan(&request, &reply);
      break;
      
    case BS_SPI_FLASH_DUMP:
      rv = read_SPI_flash(&request, &reply);
      break;  

    case BS_SPI_READID:
      rv = SPI_read_id(&request, &reply);
      break;
      
    case BS_JTAG_DISCOVER_PINOUT:
      rv = JTAG_scan(&request, &reply);
      break;

    case BS_ECHO:
      memcpy(&reply, &request, BS_REPLY_SIZE - 4);
      reply.bs_command = BS_REPLY_ECHO;
      rv = 0;
      break;
      
    default:
      rv = -1;
      break;
    }

    send_reply(rv, &request, &reply);    
    reset_gpios();
}
