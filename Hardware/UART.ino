#include <pins_arduino.h>
#include "BUSSide.h"

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

static unsigned int
findNumberOfUartSpeeds(void)
{
  unsigned int i;

  for (i = 0; uartInfo[i].baudRate; i++);
  return i;
}

static int
waitForIdle(int pin)
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

static int
buildwidths(int pin, int *widths, int nwidths)
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

static unsigned int
findminwidth(int *widths, int nwidths)
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


static float
autobaud(int pin, int *widths, int nwidths)
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

static int
tryFrameSize(int framesize, int stopbits, int *widths, int nwidths)
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

static int
calcBaud(int pin, int *widths, int nwidths)
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

  dtostrf(min1, 4, 2, fstr);
  sprintf(s, "\r\n\r\nBITWIDTH: %s\r\nBAUDRATE measured=%i nearest=%i\r\n", fstr, (int)(1000000.0/min1), uartInfo[baudIndex].baudRate);
  Serial.print(s);
  return baudIndex;
}

static int
calcParity(int frameSize, int stopBits, int *widths, int nwidths)
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
        Serial.printf("stop error\n");
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

#define NWIDTHS 100

static int
UART_line_settings_direct(int pin)
{
  int widths[NWIDTHS];
  char s[100];
  unsigned long timeStart;
  int n;
  int ret;

  pin = gpioIndex[pin];
  if (pin == D0)
    pinMode(pin, INPUT);
  else
    pinMode(pin, INPUT_PULLUP);

  ret = buildwidths(pin, widths, NWIDTHS);
  if (ret) {
    Serial.printf("--- Couldn't sample %i\n", ret);
    return -6;
  }
  
  uartSpeedIndex = calcBaud(pin, widths, NWIDTHS);
  if (uartSpeedIndex < 0) {
    Serial.println("Timeout. Not a UART.\n");
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
    Serial.println("UART framing not detected.\nNot a UART.");
    return -1;
  } else
    sprintf(s, "FRAMESIZE %i", frameSize);
  Serial.println(s);
  if (tryFrameSize(frameSize, 2, widths, NWIDTHS))
    stopBits = 2;
  else
    stopBits = 1;
  sprintf(s, "STOPBITS: %i", stopBits);
  Serial.println(s);

  parity = calcParity(frameSize, stopBits, widths, NWIDTHS);
  if (parity == -2) {
    Serial.println("Parity Timeout. Not a UART.\n");
    return -1;
  } else if (parity < 0) {
    dataBits = frameSize - stopBits - 1;
    sprintf(s, "PARITY: NONE");
  } else {
    dataBits = frameSize - stopBits - 1 - 1;
    if (parity == 0)
      sprintf(s, "PARITY: EVEN");
    else
      sprintf(s, "PARITY: ODD");
  }
  Serial.println(s);
  
  sprintf(s, "DATABITS: %i", dataBits);
  Serial.println(s);
  return 0;
}

static void
UART_all_line_settings_direct()
{
  int u = 0;

  for (int i = 0; i < N_GPIO; i++) {
    if (gpio[i] > 100) {
      u++;
      Serial.printf("--- Trying UART on GPIO %d\n", i);
      Serial.flush();
      for (int attempt = 0; attempt < 50; attempt++) {
        int ret;
        
         ret = UART_line_settings_direct(i);
         if (ret == 0) {
          Serial.printf("--- Detected UART line settings.\n");
          u++;
          break;
        } else {
          Serial.printf("--- Didn't detect UART. Retrying to be sure.\n");
          if (ret == -6) { // Timeout
            break;
          }
        }
      }
    }
  }
  if (u == 0) {
    Serial.printf("--- Couldn't detect any UART. If you think this is an error, try again.");
    Serial.printf("--- You might need to pause for some seconds after booting the device under test.\n");
  }
}

void
UART_all_line_settings()
{
  Serial.write('.');
  Serial.flush();
  UART_all_line_settings_direct();
  Serial.write(';');
}

void
data_discovery()
{
  int32_t startTime;

  Serial.print(".");
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
  Serial.printf("");
  for (int i = 0; i < N_GPIO; i++) {
    Serial.printf("--- GPIO %s had %i signal changes\n", gpioName[i], gpio[i]);
  }
  Serial.print(";");
  Serial.flush();
}
