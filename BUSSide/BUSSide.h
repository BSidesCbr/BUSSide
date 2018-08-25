#ifndef BUSSIDE_H
#define BUSSIDE_H

#define FREQ 160
#define N_GPIO 9

unsigned long crc_update(unsigned long crc, byte data);
unsigned long crc_mem(char *s, int n);
void delay_us(int us);

int send_SPI_command(struct bs_request_s *request, struct bs_reply_s *reply);
int data_discovery(struct bs_request_s *request, struct bs_reply_s *reply);
int UART_all_line_settings(struct bs_request_s *request, struct bs_reply_s *reply);
int UART_discover_tx(struct bs_request_s *request, struct bs_reply_s *reply);
int discover_I2C_slaves(struct bs_request_s *request, struct bs_reply_s *reply);
int I2C_active_scan(struct bs_request_s *request, struct bs_reply_s *reply);
int read_I2C_eeprom(struct bs_request_s *request, struct bs_reply_s *reply);
int read_SPI_flash(struct bs_request_s *request, struct bs_reply_s *reply);
int JTAG_scan(struct bs_request_s *request, struct bs_reply_s *reply);
int SPI_read_id(struct bs_request_s *request, struct bs_reply_s *reply);
int UART_passthrough(struct bs_request_s *request, struct bs_reply_s *reply);

extern byte pins[];
extern char *pinnames[];
extern const byte pinslen;
extern uint32_t usTicks;

static inline int32_t
asm_ccount(void)
{
  int32_t r;

  asm volatile("rsr %0, ccount" : "=r"(r));
  return r;
}

#define BS_ECHO                         0
#define BS_REPLY_ECHO                   -1
#define BS_SPI_FLASH_DUMP               1
#define BS_REPLY_SPI_FLASH_DUMP         2
#define BS_SPI_SEND                     3
#define BS_REPLY_SPI_SEND               4
#define BS_I2C_DISCOVER_SLAVES          5
#define BS_REPLY_I2C_DISCOVER_SLAVES    6
#define BS_I2C_SEND                     7
#define BS_REPLY_I2C_SEND               8
#define BS_I2C_FLASH_DUMP               9
#define BS_REPLY_I2C_FLASH_DUMP         10
#define BS_UART_DISCOVER_RX             11
#define BS_REPLY_UART_LINE_SETTINGS     12
#define BS_JTAG_DISCOVER_PINOUT         13
#define BS_REPLY_JTAG_DISCOVER_PINOUT   14
#define BS_DATA_DISCOVERY               15
#define BS_REPLY_DATA_DISCOVERY         16
#define BS_SPI_READID                   17
#define BS_REPLY_SPI_READID             18
#define BS_UART_PASSTHROUGH             19
#define BS_REPLY_UART_PASSTHROUGH       20
#define BS_UART_DISCOVER_TX             21
#define BS_REPLY_UART_DISCOVER_TX       22

#define BS_REQUEST_SIZE (4 + 4 + 4 + 4*256 + 4)
#define BS_REPLY_SIZE (BS_REQUEST_SIZE)

struct bs_request_s {
  uint32_t bs_command;
  uint32_t bs_command_length;
  uint32_t bs_sequence_number;
  uint32_t bs_request_args[256];
  uint32_t bs_checksum;
};

struct bs_reply_s {
  uint32_t bs_command;
  uint32_t bs_reply_length;
  uint32_t bs_sequence_number;
  uint32_t bs_reply_data[256];
  uint32_t bs_checksum;
};

#endif
