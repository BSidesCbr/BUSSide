#ifndef BUSSIDE_H
#define BUSSIDE_H

#define FREQ 160
#define N_GPIO 9

struct bs_frame_s {
  uint32_t bs_command;
  uint32_t bs_payload_length;
  uint32_t bs_sequence_number;
  uint32_t bs_checksum;
  uint32_t bs_payload[0];
};


#define BS_HEADER_SIZE (4*4)

#define bs_request_s bs_frame_s
#define bs_reply_s bs_frame_s

unsigned long crc_update(unsigned long crc, byte data);
unsigned long crc_mem(char *s, int n);
void delay_us(int us);

struct bs_frame_s *disable_write_protection(struct bs_request_s *request);
struct bs_frame_s *enable_write_protection(struct bs_request_s *request);
struct bs_frame_s *write_SPI_flash(struct bs_request_s *request);
struct bs_frame_s *spi_command_finder(struct bs_request_s *request);
struct bs_frame_s *send_SPI_command(struct bs_request_s *request);
struct bs_frame_s *send_SPI_fast_command(struct bs_request_s *request);
struct bs_frame_s *data_discovery(struct bs_request_s *request);
struct bs_frame_s *UART_all_line_settings(struct bs_request_s *request);
struct bs_frame_s *UART_discover_tx(struct bs_request_s *request);
struct bs_frame_s *discover_I2C_slaves(struct bs_request_s *request);
struct bs_frame_s *I2C_active_scan(struct bs_request_s *request);
struct bs_frame_s *read_I2C_eeprom(struct bs_request_s *request);
struct bs_frame_s *write_I2C_eeprom(struct bs_request_s *request);
struct bs_frame_s *read_SPI_flash(struct bs_request_s *request);
struct bs_frame_s *JTAG_scan(struct bs_request_s *request);
struct bs_frame_s *SPI_read_id(struct bs_request_s *request);
struct bs_frame_s *UART_passthrough(struct bs_request_s *request);
struct bs_frame_s *erase_sector_SPI_flash(struct bs_request_s *request);
struct bs_frame_s *spi_discover(struct bs_request_s *request);
struct bs_frame_s *spi_read_id_bb(struct bs_request_s *request);
struct bs_frame_s *read_SPI_flash_bitbang(struct bs_request_s *request);

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
#define BS_I2C_DISCOVER                 23
#define BS_REPLY_I2C_DISCOVER           24
#define BS_I2C_FLASH                    25
#define BS_REPLY_I2C_FLASH              26
#define BS_SPI_ERASE_SECTOR             27
#define BS_REPLY_SPI_ERASE_SECTOR       28
#define BS_SPI_DISCOVER_PINOUT          29
#define BS_REPLY_SPI_DISCOVER_PINOUT    30
#define BS_SPI_BB_READID                31
#define BS_REPLY_BB_READID              32
#define BS_SPI_BB_SPI_FLASH_DUMP        33
#define BS_REPY_SPI_BB_SPI_FLASH_DUMP   34
#define BS_SPI_COMMAND_FINDER           35
#define BS_REPLY_SPI_COMMAND_FINDER     36
#define BS_SPI_FLASH                    37
#define BS_REPLY_SPI_FLASH              38
#define BS_SPI_DISABLE_WP               39
#define BS_REPLY_SPI_DISABLE_WP         40
#define BS_SPI_ENABLE_WP                41
#define BS_REPLY_SPI_ENABLE_WP          42
#define BS_SPI_FAST_SEND                43
#define BS_REPLY_SPI_FAST_SEND          44


#endif
