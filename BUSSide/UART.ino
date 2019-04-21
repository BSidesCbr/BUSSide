#include <pins_arduino.h>
#include "BUSSide.h"
#include <SoftwareSerial.h>

//#define min(a,b) (((a)<(b))?(a):(b))

uint32_t usTicks = 0;

static int gpioVal[N_GPIO];

#define sampleTx(pin) digitalRead(pin)

struct uartInfo_s {
  int baudRate;
  float microsDelay;
} uartInfo[] = {
  { 300,    3333.3  }, // 0
  { 600,    1666.7  }, // 1
  { 1200,   833.3 }, // 2
  { 2400,   416.7 }, // 3
  { 4800,   208.3 }, // 4
  { 9600,   104.2 }, // 5
  { 19200,  52.1  }, // 6
  { 38400,  26.0  }, // 7
  { 57600,  17.4  }, // 8
  { 115200, 8.68  }, // 9
  { 0, 0 },
};

static int uartSpeedIndex;

static unsigned int findNumberOfUartSpeeds(void)
{
  unsigned int i;

  for (i = 0; uartInfo[i].baudRate; i++);
  return i;
}

static int waitForIdle(int pin)
{
  unsigned long startTime;
  unsigned long bitTime10;
  int32_t beginTime;

  beginTime = asm_ccount();
  bitTime10 = uartInfo[uartSpeedIndex].microsDelay * 10.0;
start:
  ESP.wdtFeed();
  if ((uint32_t)(asm_ccount() - beginTime)/FREQ >= 10*1000000)
    return 1;
  startTime = microsTime();
  while ((microsTime() - startTime) <  bitTime10) {
    if (sampleTx(pin) != HIGH)
      goto start;
    ;
  }
  return 0;
}

static int buildwidths(int pin, int *widths, int nwidths)
{
  int val;
  int32_t startTime;
  int curVal;

  if (waitForIdle(pin))
    return 1;
  widths[0] = 1;
  val = 1;
  startTime = asm_ccount();
  for (int i = 1; i < nwidths; i++) {
    int32_t newStartTime;

    do {
      ESP.wdtFeed();
      curVal = sampleTx(pin);
    } while (curVal == val && (uint32_t)(asm_ccount() - startTime)/FREQ < 5*1000000);
    if (curVal == val)
      return 1;
      
    newStartTime = asm_ccount();
    val = curVal;
    widths[i] = (uint32_t)(newStartTime - startTime)/FREQ;
    startTime = newStartTime;
  }
  return 0;
}

static unsigned int findminwidth(int *widths, int nwidths)
{
  int minIndex1;
  unsigned int min1;

  minIndex1 = -1;
  for (int i = 2; i < nwidths; i++) {
    if (minIndex1 < 0 || widths[i] < min1) {
      min1 = widths[i];
      minIndex1 = i;
    }
  }
  return min1;
}


static float autobaud(int pin, int *widths, int nwidths)
{
  int sum;
  int c = 0;

  sum = 0;
  for (int i = 2; (i+15) < nwidths; i += 15, c++) {
    int minwidth;
    
    minwidth = findminwidth(&widths[i], 15);
    sum += minwidth;
  }
  return (float)sum/(float)c;
}

static int tryFrameSize(int framesize, int stopbits, int *widths, int nwidths)
{
  float width_timepos = 0.0;
  float bitTime = uartInfo[uartSpeedIndex].microsDelay;
  float stopTime = bitTime*((float)framesize - (float)stopbits + 0.5);
  float frameTime = bitTime*(float)framesize;
  float w;
  int framingErrors = 0;

  w = 0.0;
  for (int i = 2; i < nwidths-1; i++) {
    if (stopTime >= w && stopTime < (w + widths[i])) {
      if ((i % 2) != widths[0]) {
        framingErrors++;
        if (framingErrors >= 1)
          return 0;
      }
      w = 0.0;
    } else {
      w += widths[i];
    }
  }
  return 1;
}

static int calcBaud(int pin, int *widths, int nwidths)
{
  char fstr[6];
  char s[100];
  int baudIndex;
  float min1;
  int minDelayIndex;
  float minDelay;

  min1 = autobaud(pin, widths, nwidths);
  minDelayIndex = -1;
  for (int i = 0; uartInfo[i].baudRate; i++) {
    double abs_delay;

    abs_delay = min1 - uartInfo[i].microsDelay;
    abs_delay *= abs_delay;
    if (minDelayIndex < 0 || abs_delay < minDelay) {
      minDelay = abs_delay;
      minDelayIndex = i;
    }
  }
  baudIndex = minDelayIndex;

//  dtostrf(min1, 4, 2, fstr);
//  sprintf(s, "\r\n\r\nBITWIDTH: %s\r\nBAUDRATE measured=%i nearest=%i\r\n", fstr, (int)(1000000.0/min1), uartInfo[baudIndex].baudRate);
//  Serial.print(s);
  return baudIndex;
}

static int calcParity(int frameSize, int stopBits, int *widths, int nwidths)
{
  float width_timepos = 0.0;
  float bitTime = uartInfo[uartSpeedIndex].microsDelay;
  float stopTime = bitTime*((float)frameSize - (float)stopBits + 0.5);
  float frameTime = bitTime*(float)frameSize;
  float w;
  int bits[20];
  float dataTime = (float)bitTime * 1.5;
  int onBits = 0;
  int odd, even;
  int bitCount;
  int framingErrors;

  framingErrors = 0;
  bitCount = 0;
  odd = 0;
  even = 0;
  w = 0.0;
  for (int i = 2; i < nwidths-1; i++) {
    if (dataTime >= w && dataTime < (w + widths[i])) {
      bits[bitCount] = i % 2;
      if (i % 2)
        onBits++;
      if (bitCount < (frameSize - stopBits)) {
         dataTime += dataTime;
      }
    }
    if (stopTime >= w && stopTime < (w + widths[i])) {
      if ((i % 2) != widths[0]) {
//        Serial.printf("stop error\n");
        framingErrors++;
        if (framingErrors >= 1)
          return 0;
      }
      if (onBits & 1)
        odd++;
      else
        even++;
      w = 0.0;
      bitCount = 0;
      dataTime = (float)bitTime * 1.5;
      onBits = 0;
    } else {
      w += widths[i];
    }
  }
//  Serial.printf("odd %i even %i\n", odd, even);
  if (odd && even && min(odd,even) >= 1)
    return -1;
  if (odd)
    return 1;
  return 0;
}

static int frameSize;
static int stopBits;
static int dataBits;
static int parity;
static float bitTime;

#define NWIDTHS 200

static int UART_line_settings_direct(struct bs_reply_s *reply, int index)
{
  int widths[NWIDTHS];
  char s[100];
  unsigned long timeStart;
  int n;
  int ret;
  int pin;
  uint32_t *reply_data;

  pin = gpioIndex[index];
  if (pin == D0)
    pinMode(pin, INPUT);
  else
    pinMode(pin, INPUT_PULLUP);

  ret = buildwidths(pin, widths, NWIDTHS);
  if (ret) {
    return -6;
  }
  uartSpeedIndex = calcBaud(pin, widths, NWIDTHS);
  if (uartSpeedIndex < 0) {
    return -1;
  }
  bitTime = uartInfo[uartSpeedIndex].microsDelay;

  frameSize = -1;
  for (int i = 7; i < 14; i++) {
    if (tryFrameSize(i, 1, widths, NWIDTHS)) {
      frameSize = i;
      break;
    }
  }
  if (frameSize < 0) {
    return -1;
  } else {
  }
  if (tryFrameSize(frameSize, 2, widths, NWIDTHS)){
    stopBits = 2;
  } else {
    stopBits = 1;
  }

  parity = calcParity(frameSize, stopBits, widths, NWIDTHS);
  if (parity == -2) {
    return -1;
  } else if (parity < 0) {
    dataBits = frameSize - stopBits - 1;
  } else {
    dataBits = frameSize - stopBits - 1 - 1;
    if (parity == 0) {
    } else {
    }
  }

  reply_data = (uint32_t *)&reply->bs_payload[0];
  reply_data[index*5 + 0] = gpio[index];
  reply_data[index*5 + 1] = dataBits;
  reply_data[index*5 + 2] = stopBits;
  reply_data[index*5 + 3] = parity;
  reply_data[index*5 + 4] = uartInfo[uartSpeedIndex].baudRate;

  return 0;
}

struct bs_frame_s*
UART_all_line_settings(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  int u = 0;

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + 4*5*N_GPIO);
  if (reply == NULL)
    return NULL;
  reply->bs_payload_length = 4*5*N_GPIO;
  memset(reply->bs_payload, 0, 4*5*N_GPIO);
  for (int i = 0; i < N_GPIO; i++) {
    if (gpio[i] > 100) {
      u++;
      for (int attempt = 0; attempt < 50; attempt++) {
        int ret;

        ESP.wdtFeed();
        ret = UART_line_settings_direct(reply, i);
        if (ret == 0) {
          u++;
          break;
        } else {
          if (ret == -6) { // Timeout
            break;
          }
        }
      }
    }
  }
  return reply;
}

struct bs_frame_s*
data_discovery(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  uint32_t *reply_data;
  int32_t startTime;

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + N_GPIO*4);
  if (reply == NULL)
    return NULL;
  reply_data = (uint32_t *)&reply->bs_payload[0];
  startTime = asm_ccount();
  for (int i = 0; i < N_GPIO; i++) {
    if (gpioIndex[i] == D0)
      pinMode(gpioIndex[i], INPUT);
    else
      pinMode(gpioIndex[i], INPUT_PULLUP);
    gpio[i] = 0;
    gpioVal[i] = sampleTx(gpioIndex[i]);
  }
  do {
    ESP.wdtFeed();
    for (int i = 0; i < N_GPIO; i++) {
      int curVal;
      
      curVal = sampleTx(gpioIndex[i]);
      if (gpioVal[i] != curVal) {
        gpioVal[i] = curVal;
        gpio[i]++;
      }
    }
  } while ((uint32_t)(asm_ccount() - startTime)/FREQ < (7.5 * 1000000));  
  reply->bs_payload_length = 4*N_GPIO;
  for (int i = 0; i < N_GPIO; i++) {
    reply_data[i] = gpio[i];
  }
  return reply;
}

struct bs_frame_s*
UART_passthrough(struct bs_request_s *request)
{
  uint32_t *request_args;
  int rxpin, txpin;
  int baudrate;

  request_args = (uint32_t *)&request->bs_payload[0];
  rxpin = request_args[0];
  txpin = request_args[1];
  baudrate = request_args[2];
  SoftwareSerial ser(gpioIndex[rxpin], gpioIndex[txpin]);
  ser.begin(baudrate);
  while (1) {
    ESP.wdtFeed();
    
    while (ser.available() > 0) {
      Serial.write(ser.read());
      yield();
    }

    while (Serial.available() > 0) {
      ser.write(Serial.read());
      yield();
    }
  }
  return NULL;
}

int
UART_testtx(SoftwareSerial *ser, int testChar)
{
  ser->write(testChar);
  for (int i = 0; i < 10000; i++) {
    ESP.wdtFeed();
    if (ser->available() > 0) {
      int ch;
      
      ch = ser->read();
      if (ch == testChar)
        return 1;
      else
        return 0;
    }
    delay_us(50);
  }
  return 0;
}

struct bs_frame_s*
UART_discover_tx(struct bs_request_s *request)
{
  uint32_t *request_args, *reply_data;
  struct bs_frame_s *reply;
  int rxpin, txpin;
  int baudrate;

  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + 4);
  if (reply == NULL)
    return NULL;
  reply->bs_payload_length = 4;
  reply_data = (uint32_t *)&reply->bs_payload[0];
  request_args = (uint32_t *)&request->bs_payload[0];
  rxpin = request_args[0] - 1;
  baudrate = request_args[1];
  for (txpin = 1; txpin < N_GPIO; txpin++) {
    int found;
    
    ESP.wdtFeed();
    
    if (rxpin == txpin)
      continue;
      
    SoftwareSerial ser(gpioIndex[rxpin], gpioIndex[txpin]);

    ser.begin(baudrate);
    while (ser.available()) {
      ser.read();
    }
    found = 1;
    for (const char *p = "BS"; *p; p++) {
      if (UART_testtx(&ser, *p) == 0) {
        found = 0;
        break;
      }
    }
    if (found) {
      reply_data[0] = txpin;
      return reply;
    }
  }
  reply_data[0] = -1;
  return reply;
}
