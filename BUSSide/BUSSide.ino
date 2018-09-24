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
send_reply(struct bs_request_s *request, struct bs_reply_s *reply)
{
    reply->bs_sequence_number = request->bs_sequence_number;
    reply->bs_checksum = 0;
    reply->bs_checksum = crc_mem((char *)reply, BS_HEADER_SIZE + reply->bs_payload_length);
    Serial.write((uint8_t *)reply, BS_HEADER_SIZE + reply->bs_payload_length);
    Serial.flush();
}

static void
FlushIncoming()
{
  while (Serial.available() > 0) {
    (void)Serial.read();
  }
}

static void
Sync()
{
  while (1) {
    int rv;
    int ch;
    
    rv = Serial.readBytes((uint8_t *)&ch, 1);
    if (rv <= 0)
      continue;
    if (ch == 0xfe) {
got1:
      rv = Serial.readBytes((uint8_t *)&ch, 1);
      if (rv <= 0)
        continue;
      if (ch == 0xca)
        return;
      else if (ch == 0xfe)
        goto got1;
    }
  }
}

void
loop()
{
    struct bs_frame_s header;
    struct bs_frame_s *request, *reply;
    int rv;

    Serial.setTimeout(1000);
    Sync();
    rv = Serial.readBytes((char *)&header, BS_HEADER_SIZE);
    if (rv <= 0) {
      FlushIncoming();
      return;
    }
    if (header.bs_payload_length > 65356) {
      FlushIncoming();
      return;
    }
    request = (struct bs_frame_s *)alloca(BS_HEADER_SIZE + header.bs_payload_length);
    memcpy(request, &header, BS_HEADER_SIZE);
    if (request->bs_payload_length > 0) {
      Serial.setTimeout(1000);
      rv = Serial.readBytes((char *)&request->bs_payload, request->bs_payload_length);
      if (rv <= 0) {
        FlushIncoming();
        return;
      }
    }
    
    request->bs_checksum = 0;
    if (crc_mem((char *)request, BS_HEADER_SIZE + request->bs_payload_length) != header.bs_checksum) {
      return;
    }
    if (request->bs_sequence_number <= sequence_number)
      return;
    sequence_number = request->bs_sequence_number;

    reply = NULL;
   
    switch (request->bs_command) {
    case BS_SPI_SEND:
      reply = send_SPI_command(request);
      break;
      
    case BS_DATA_DISCOVERY:
      reply = data_discovery(request);
      break;

    case BS_UART_DISCOVER_RX:
      reply = data_discovery(request);
      if (reply != NULL) {
        free(reply);
        reply = UART_all_line_settings(request);
      }
      break;

    case BS_UART_DISCOVER_TX:
      reply = UART_discover_tx(request);
      break;
      
    case BS_UART_PASSTHROUGH:
      reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE);
      if (reply != NULL) {
        send_reply(request, reply);
        free(reply);
        (void)UART_passthrough(request);
        // no return
      }
      break;
      
    case BS_I2C_DISCOVER_SLAVES:
      reply = discover_I2C_slaves(request);
      break;

    case BS_I2C_FLASH_DUMP:
      reply = read_I2C_eeprom(request);
      break;

    case BS_I2C_FLASH:
      reply = write_I2C_eeprom(request);
      break;
      
    case BS_I2C_DISCOVER:
      reply = I2C_active_scan(request);
      break;
      
    case BS_SPI_FLASH_DUMP:
      reply = read_SPI_flash(request);
      break;  

    case BS_SPI_READID:
      reply = SPI_read_id(request);
      break;
      
    case BS_JTAG_DISCOVER_PINOUT:
      reply = JTAG_scan(request);
      break;

    case BS_ECHO:
      reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + request->bs_payload_length);
      if (reply != NULL) {
        memcpy(reply, request, BS_HEADER_SIZE + request->bs_payload_length);
        reply->bs_command = BS_REPLY_ECHO;
      }
      break;

    case BS_SPI_ERASE_SECTOR:
      reply = erase_sector_SPI_flash(request);
      break;
    
    case BS_SPI_DISCOVER_PINOUT:
      reply = spi_discover(request);
      break;
      
    case BS_SPI_BB_READID:
      reply = spi_read_id_bb(request);
      break;
      
    case BS_SPI_BB_SPI_FLASH_DUMP:
      reply = read_SPI_flash_bitbang(request);
      break;

    case BS_SPI_COMMAND_FINDER:
      reply = spi_command_finder(request);
      break;

    case BS_SPI_FLASH:
      reply = write_SPI_flash(request);
      break;

    case BS_SPI_DISABLE_WP:
      reply = disable_write_protection(request);
      break;

    case BS_SPI_ENABLE_WP:
      reply  = enable_write_protection(request);
      break;
      
    default:
      reply = NULL;
      break;
    }

    reset_gpios();
    send_reply(request, reply);
    free(reply);    
}
