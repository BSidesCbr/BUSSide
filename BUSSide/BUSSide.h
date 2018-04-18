#ifndef BUSSIDE_H
#define BUSSIDE_H

#define FREQ 160
#define N_GPIO 9

unsigned long crc_update(unsigned long crc, byte data);
unsigned long crc_mem(char *s, int n);
void data_discovery();
void UART_all_line_settings();
void delay_us(int us);
void discover_I2C_slaves();
void I2C_active_scan();
void read_I2C_eeprom();
void read_SPI_flash();
void JTAG_scan();

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


#endif
