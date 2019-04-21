#include "BUSSide.h"

#define TRUE  255
#define FALSE 0

//#define DEBUGTAP
//#define DEBUGIR

// For 3.3v AVR boards. Cuts clock in half. Also see cmd in setup()
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))

// Once you have found the JTAG pins you can define
// the following to allow for the boundary scan and
// irenum functions to be run. 
//
// ************** VERY IMPORTANT! *****************
// Define the values as the index for the pins[]
// array of the found jtag pin.
// ************************************************                     
#define TCK                      0
#define TMS                      1
#define TDO                      2
#define TDI                      3
#define TRST                     4

// Pattern used for scan() and loopback() tests
#define PATTERN_LEN              64
// Use something random when trying find JTAG lines:
static char mypattern[PATTERN_LEN] = "011001110100110110100001011100100101100111010011010101011010101";
// Use something more determinate when trying to find
// length of the DR register:
//static char mypattern[PATTERN_LEN] = "1000000000000000000000000000000000";

// Max. number of JTAG enabled chips (MAX_DEV_NR) and length
// of the DR register together define the number of
// iterations to run for scan_idcode():
#define MAX_DEV_NR               8
#define IDCODE_LEN               32  

// Target specific, check your documentation or guess 
#define SCAN_LEN                 1890 // used for IR enum. bigger the better
#define IR_LEN                   5
// IR registers must be IR_LEN wide:
#define IR_IDCODE                "01100" // always 011
#define IR_SAMPLE                "10100" // always 101
#define IR_PRELOAD               IR_SAMPLE

/*
 * END USER DEFINITIONS
 */



// TAP TMS states we care to use. NOTE: MSB sent first
// Meaning ALL TAP and IR codes have their leftmost
// bit sent first. This might be the reverse of what
// documentation for your target(s) show.
#define TAP_RESET                "11111"       // looping 1 will return 
                                               // IDCODE if reg available
#define TAP_SHIFTDR              "111110100"
#define TAP_SHIFTIR              "1111101100" // -11111> Reset -0> Idle -1> SelectDR
                                              // -1> SelectIR -0> CaptureIR -0> ShiftIR

// Ignore TCK, TMS use in loopback check:
#define IGNOREPIN                0xFFFF 
// Flags configured by UI:
static boolean VERBOSE                  = FALSE;
static boolean DELAY                    = FALSE;
static long    DELAYUS                  = 5; //5000; // 5 MillisecondsHTAGboolean PULLUP                   = TRUE; 

/*
 * Set the JTAG TAP state machine
 */
static void tap_state(char tap_state[], int tck, int tms) 
{
#ifdef DEBUGTAP
  Serial.print("tap_state: tms set to: ");
#endif
  while (*tap_state) { // exit when string \0 terminator encountered
    ESP.wdtFeed();
    if (DELAY) delayMicroseconds(50);
    digitalWrite(pins[tck], LOW);          
    digitalWrite(pins[tms], *tap_state - '0'); // conv from ascii pattern
#ifdef DEBUGTAP
    Serial.print(*tap_state - '0',DEC);
#endif
    digitalWrite(pins[tck], HIGH); // rising edge shifts in TMS
    *tap_state++;
  }        
#ifdef DEBUGTAP
  Serial.println();
#endif
}

static int JTAG_clock(int tck, int tms, int tdi, int tdo, int tms_state, int tdi_state)
{
  int tdo_state;

  ESP.wdtFeed();
  if (DELAY) delayMicroseconds(50);
  digitalWrite(pins[tck], LOW);
  digitalWrite(pins[tms], tms_state);
  digitalWrite(pins[tdi], tdi_state);
  tdo_state = digitalRead(pins[tdo]);
  digitalWrite(pins[tck], HIGH);
  return tdo_state;
}

static uint32_t JTAG_read32(int tck, int tms, int tdi, int tdo)
{
  uint32_t r;

  r = 0;
  for (int i = 0; i < 32; i++) {
    ESP.wdtFeed();
    r |= (JTAG_clock(tck, tms, tdi, tdo, 0, 0) << i);
  }
  return r;
}

static int JTAG_ndevices(int tck, int tms, int tdi, int tdo)
{
  int nbDevices;
  int i;
  
  // go to reset state
  for(i=0; i<5; i++) {
    ESP.wdtFeed();
    JTAG_clock(tck, tms, tdi, tdo, 1, 0);
  }

  // go to Shift-IR
  JTAG_clock(tck, tms, tdi, tdo, 0, 0);
  JTAG_clock(tck, tms, tdi, tdo, 1, 0);
  JTAG_clock(tck, tms, tdi, tdo, 1, 0);
  JTAG_clock(tck, tms, tdi, tdo, 0, 0);
  JTAG_clock(tck, tms, tdi, tdo, 0, 0);

  // Send plenty of ones into the IR registers
  // That makes sure all devices are in BYPASS!
  for(i=0; i<999; i++) {
    ESP.wdtFeed();
    JTAG_clock(tck, tms, tdi, tdo, 0, 1);
  }

  JTAG_clock(tck, tms, tdi, tdo, 1, 1);  // last bit needs to have TMS active, to exit shift-IR

  // we are in Exit1-IR, go to Shift-DR
  JTAG_clock(tck, tms, tdi, tdo, 1, 0);
  JTAG_clock(tck, tms, tdi, tdo, 1, 0);
  JTAG_clock(tck, tms, tdi, tdo, 0, 0);
  JTAG_clock(tck, tms, tdi, tdo, 0, 0);

  // Send plenty of zeros into the DR registers to flush them
  for(i=0; i<1000; i++) {
    ESP.wdtFeed();
    JTAG_clock(tck, tms, tdi, tdo, 0, 0);
  }

  // now send ones until we receive one back
  for(i=0; i<1000; i++) {
    ESP.wdtFeed();
    if(JTAG_clock(tck, tms, tdi, tdo, 0, 1)) break;
  }

  nbDevices = i;
//  Serial.printf("There are %d device(s) in the JTAG chain\n", nbDevices);
  return nbDevices;
}

static void JTAG_scan_chain(int tck, int tms, int tdi, int tdo, int ndevices)
{
  // go to reset state (that loads IDCODE into IR of all the devices)
  for(int i=0; i<5; i++) JTAG_clock(tck, tms, tdi, tdo, 1, 0);

  // go to Shift-DR
  JTAG_clock(tck, tms, tdi, tdo, 0, 0);
  JTAG_clock(tck, tms, tdi, tdo, 1, 0);
  JTAG_clock(tck, tms, tdi, tdo, 0, 0);
  JTAG_clock(tck, tms, tdi, tdo, 0, 0);

  // and read the IDCODES
  for (int i=0; i < ndevices; i++)
  {
    printf("IDCODE for device %d is %08X\n", i+1, JTAG_read32(tck, tms, tdi, tdo));
  }
}

static void pulse_tms(int tck, int tms, int s_tms)
{
  if (tck == IGNOREPIN) return;
  digitalWrite(pins[tck], LOW);
  digitalWrite(pins[tms], s_tms); 
  digitalWrite(pins[tck], HIGH);
}
static void pulse_tdi(int tck, int tdi, int s_tdi)
{
  if (DELAY) delayMicroseconds(50);
  if (tck != IGNOREPIN) digitalWrite(pins[tck], LOW);
  digitalWrite(pins[tdi], s_tdi); 
  if (tck != IGNOREPIN) digitalWrite(pins[tck], HIGH);
}

byte
pulse_tdo(int tck, int tdo)
{
  byte tdo_read;
  if (DELAY) delayMicroseconds(50);
  digitalWrite(pins[tck], LOW); // read in TDO on falling edge
  tdo_read = digitalRead(pins[tdo]);
  digitalWrite(pins[tck], HIGH);
  return tdo_read;
}

/*
 * Initialize all pins to a default state
 * default with no arguments: all pins as INPUTs
 */
void
init_pins(int tck = IGNOREPIN, int tms = IGNOREPIN, int tdi = IGNOREPIN, int ntrst = IGNOREPIN) 
{ 
  ESP.wdtFeed();
  // default all to INPUT state
  for (int i = 0; i < pinslen; i++) {
    if (pins[i] == D0)
      pinMode(pins[i], INPUT);
    else
      pinMode(pins[i], INPUT); //  Don't use PULLUP
    // internal pullups default to logic 1:
    //if (PULLUP) digitalWrite(pins[i], HIGH); 
  }
  // TCK = output
  if (tck != IGNOREPIN) pinMode(pins[tck], OUTPUT);
  // TMS = output
  if (tms != IGNOREPIN) pinMode(pins[tms], OUTPUT);
  // tdi = output
  if (tdi != IGNOREPIN) pinMode(pins[tdi], OUTPUT);
  // ntrst = output, fixed to 1
  if (ntrst != IGNOREPIN) {
    pinMode(pins[ntrst], OUTPUT);
    digitalWrite(pins[ntrst], HIGH);
  }
}


/*
 * send pattern[] to TDI and check for output on TDO
 * This is used for both loopback, and Shift-IR testing, i.e.
 * the pattern may show up with some delay.
 * return: 0 = no match
 *       1 = match 
 *       2 or greater = no pattern found but line appears active
 *
 * if retval == 1, *reglen returns the length of the register
 */
static int check_data(char pattern[], int iterations, int tck, int tdi, int tdo, int *reg_len)
{
  int i;
        int w          = 0;
  int plen       = strlen(pattern);
  char tdo_read;
  char tdo_prev;
  int nr_toggle  = 0; // count how often tdo toggled
  /* we store the last plen (<=PATTERN_LEN) bits,
   *  rcv[0] contains the oldest bit */
  char rcv[PATTERN_LEN];
  
  tdo_prev = '0' + (digitalRead(pins[tdo]) == HIGH);

  for(i = 0; i < iterations; i++) {
    ESP.wdtFeed();
       
    /* output pattern and incr write index */
    pulse_tdi(tck, tdi, pattern[w++] - '0');
    if (!pattern[w])
      w = 0;

    /* read from TDO and put it into rcv[] */
    tdo_read  =  '0' + (digitalRead(pins[tdo]) == HIGH);

    nr_toggle += (tdo_read != tdo_prev);
    tdo_prev  =  tdo_read;

    if (i < plen)
      rcv[i] = tdo_read;
    else 
    {
      memmove(rcv, rcv + 1, plen - 1);
      rcv[plen-1] = tdo_read;
    }
        
    /* check if we got the pattern in rcv[] */
    if (i >= (plen - 1) ) {
      if (!memcmp(pattern, rcv, plen)) {
        *reg_len = i + 1 - plen;
        return 1;
      }
    }
  } /* for(i=0; ... ) */
  
  *reg_len = 0;
  return nr_toggle > 1 ? nr_toggle : 0;
}

static void print_pins(int tck, int tms, int tdo, int tdi, int ntrst)
{
  if (VERBOSE) {
    if (ntrst != IGNOREPIN) {
      Serial.print(" ntrst:");
      Serial.print(pinnames[ntrst]);
    }
    Serial.print(" tck:");
    Serial.print(pinnames[tck]);
    Serial.print(" tms:");
    Serial.print(pinnames[tms]);
    Serial.print(" tdo:");
    Serial.print(pinnames[tdo]);
    if (tdi != IGNOREPIN) {
      Serial.print(" tdi:");
      Serial.print(pinnames[tdi]);
    }
  }
}

/*
 * Shift JTAG TAP to ShiftIR state. Send pattern to TDI and check
 * for output on TDO
 */
static int scan(int *tck_pin, int *tms_pin, int *tdi_pin, int *tdo_pin, int *ntrst_pin)
{
  int tck, tms, tdo, tdi, ntrst;
  int checkdataret = 0;
  int len;
  int reg_len;
  int retVal = 0;

//  Serial.println(pattern);
  for(ntrst=0;ntrst<pinslen;ntrst++) {
    ESP.wdtFeed();
    for(tck=0;tck<pinslen;tck++) {
      ESP.wdtFeed();
      if(tck == ntrst) continue;
      for(tms=0;tms<pinslen;tms++) {
        ESP.wdtFeed();
        if(tms == ntrst) continue;
        if(tms == tck  ) continue;
        for(tdo=0;tdo<pinslen;tdo++) {
          ESP.wdtFeed();
          if(tdo == ntrst) continue;
          if(tdo == tck  ) continue;
          if(tdo == tms  ) continue;
          for(tdi=0;tdi<pinslen;tdi++) {
            ESP.wdtFeed();
            if(tdi == ntrst) continue;
            if(tdi == tck  ) continue;
            if(tdi == tms  ) continue;
            if(tdi == tdo  ) continue;
            if(VERBOSE) {
              print_pins(tck, tms, tdo, tdi, ntrst);
              Serial.print("    ");
            }
            init_pins(tck, tms, tdi, ntrst);
            tap_state(TAP_SHIFTIR, tck, tms);
            checkdataret = check_data(mypattern, (2*PATTERN_LEN), tck, tdi, tdo, &reg_len); 
            if(checkdataret == 1) {
//              Serial.print("--- Found JTAG connection\n");
//              print_pins(tck, tms, tdo, tdi, ntrst);
              *tck_pin = tck;
              *tms_pin = tms;
              *tdo_pin = tdo;
              *tdi_pin = tdi;
              *ntrst_pin = ntrst;
              retVal++;
//              Serial.print(" IR length: ");
//              Serial.println(reg_len, DEC);
            }
            else if(checkdataret > 1) {
#if 0
              Serial.print("active ");
              print_pins(tck, tms, tdo, tdi, ntrst);
              Serial.print("  bits toggled:");
              Serial.println(checkdataret);
#endif
            }
            else if(VERBOSE) Serial.println();                      
          } /* for(tdi=0; ... ) */
        } /* for(tdo=0; ... ) */
      } /* for(tms=0; ... ) */
    } /* for(tck=0; ... ) */
  } /* for(ntrst=0; ... ) */
  return retVal;
}
/*
 * Check for pins that pass pattern[] between tdi and tdo
 * regardless of JTAG TAP state (tms, tck ignored).
 *
 * TDO, TDI pairs that match indicate possible shorts between
 * pins. Pins that do not match but are active might indicate
 * that the patch cable used is not shielded well enough. Run
 * the test again without the cable connected between controller
 * and target. Run with the verbose flag to examine closely.
 */
static void loopback_check()
{
  int tdo, tdi;
  int checkdataret = 0;
  int reg_len;

  for(tdo=0;tdo<pinslen;tdo++) {
    for(tdi=0;tdi<pinslen;tdi++) {
      ESP.wdtFeed();
      if(tdi == tdo) continue;
  
      if(VERBOSE) {
        Serial.print(" tdo:");
        Serial.print(pinnames[tdo]);
        Serial.print(" tdi:");
        Serial.print(pinnames[tdi]);
        Serial.print("    ");
      }
      init_pins(IGNOREPIN/*tck*/, IGNOREPIN/*tck*/, pins[tdi], IGNOREPIN /*ntrst*/);
      checkdataret = check_data(mypattern, (2*PATTERN_LEN), IGNOREPIN, tdi, tdo, &reg_len);
      if(checkdataret == 1) {
        if (VERBOSE) {
          Serial.print("FOUND! ");
          Serial.print(" tdo:");
          Serial.print(pinnames[tdo]);
          Serial.print(" tdi:");
          Serial.print(pinnames[tdi]);
          Serial.print(" reglen:");
          Serial.println(reg_len);
        }
      }
      else if(checkdataret > 1) {
        if (VERBOSE) {
          Serial.print("active ");
          Serial.print(" tdo:");
          Serial.print(pinnames[tdo]);
          Serial.print(" tdi:");
          Serial.print(pinnames[tdi]);
          Serial.print("  bits toggled:");
          Serial.println(checkdataret);
        }
      }
      else if(VERBOSE) Serial.println();
    }
  }
}

#if 0
static void
list_pin_names()
{
  int pin;
  Serial.print("The configured pins are:\r\n");
  for(pin=0;pin<pinslen;pin++) {
    Serial.print(pinnames[pin]);
    Serial.print(" ");
  }
  Serial.println();
}
#endif

/*
 * Scan TDO for IDCODE. Handle MAX_DEV_NR many devices.
 * We feed zeros into TDI and wait for the first 32 of them to come out at TDO (after n * 32 bit).
 * As IEEE 1149.1 requires bit 0 of an IDCODE to be a "1", we check this bit.
 * We record the first bit from the idcodes into bit0.
 * (oppposite to the old code).
 * If we get an IDCODE of all ones, we assume that the pins are wrong.
 */
static void scan_idcode()
{
  int tck, tms, tdo, tdi, ntrst;
  int i, j;
  int nr; /* number of devices */
  int tdo_read;
  uint32_t idcodes[MAX_DEV_NR];

  char idcodestr[] = "                ";
  int idcode_i = 31; // TODO: artifact that might need to be configurable
  uint32_t idcode;
  for(ntrst=0;ntrst<pinslen;ntrst++) {
    ESP.wdtFeed();
    for(tck=0;tck<pinslen;tck++) {
      ESP.wdtFeed();
      if(tck == ntrst) continue;
      for(tms=0;tms<pinslen;tms++) {
        ESP.wdtFeed();
        if(tms == ntrst) continue;
        if(tms == tck  ) continue;
        for(tdo=0;tdo<pinslen;tdo++) {
          ESP.wdtFeed();
          if(tdo == ntrst) continue;
          if(tdo == tck  ) continue;
          if(tdo == tms  ) continue;
          for(tdi=0;tdi<pinslen;tdi++) {
            ESP.wdtFeed();
            if(tdi == ntrst) continue;
            if(tdi == tck  ) continue;
            if(tdi == tms  ) continue;
            if(tdi == tdo  ) continue;
            if(VERBOSE) {
              print_pins(tck, tms, tdo, tdi, ntrst);
              Serial.print("    ");
            }
            init_pins(tck, tms, tdi, ntrst);

            /* we hope that IDCODE is the default DR after reset */
            tap_state(TAP_RESET, tck, tms);
            tap_state(TAP_SHIFTDR, tck, tms);
            
            /* j is the number of bits we pulse into TDI and read from TDO */
            for(i = 0; i < MAX_DEV_NR; i++) {
              idcodes[i] = 0;
              for(j = 0; j < IDCODE_LEN;j++) {
                ESP.wdtFeed();
                /* we send '0' in */
                pulse_tdi(tck, tdi, 0);
                tdo_read = digitalRead(pins[tdo]);
                if (tdo_read)
                  idcodes[i] |= ( (uint32_t) 1 ) << j;
  
                if (VERBOSE)
                  Serial.print(tdo_read,DEC);
              } /* for(j=0; ... ) */
              if (VERBOSE) {
                Serial.print(" ");
                Serial.println(idcodes[i],HEX);
              }
              /* save time: break at the first idcode with bit0 != 1 */
              if (!(idcodes[i] & 1) || idcodes[i] == 0xffffffff)
                break;
            } /* for(i=0; ...) */
  
            if (i > 0) {
              if (VERBOSE) {
                print_pins(tck,tms,tdo,tdi,ntrst);
                Serial.print("  devices: ");
                Serial.println(i,DEC);
                for(j = 0; j < i; j++) {
                  Serial.print("  0x");
                  Serial.println(idcodes[j],HEX);
                }
              }
            } /* if (i > 0) */
          } /* for(tdo=0; ... ) */
        } /* for(tdi=0; ...) */
      } /* for(tms=0; ...) */
    } /* for(tck=0; ...) */
  } /* for(trst=0; ...) */
}

static void shift_bypass()
{
  int tdi, tdo, tck;
  int checkdataret;
  int reg_len;

  for(tck=0;tck<pinslen;tck++) {
    ESP.wdtFeed();
    for(tdi=0;tdi<pinslen;tdi++) {
      ESP.wdtFeed();
      if(tdi == tck) continue;
      for(tdo=0;tdo<pinslen;tdo++) {
        ESP.wdtFeed();
        if(tdo == tck) continue;
        if(tdo == tdi) continue;
        if(VERBOSE) {
          Serial.print(" tck:");
          Serial.print(pinnames[tck]);
          Serial.print(" tdi:");
          Serial.print(pinnames[tdi]);
          Serial.print(" tdo:");
          Serial.print(pinnames[tdo]);
          Serial.print("    ");
        }

        init_pins(tck, IGNOREPIN/*tms*/,tdi, IGNOREPIN /*ntrst*/);
        // if bypass is default on start, no need to init TAP state
        checkdataret = check_data(mypattern, (2*PATTERN_LEN), tck, tdi, tdo, &reg_len);
        if(checkdataret == 1) {
          if (VERBOSE) {
            Serial.print("FOUND! ");
            Serial.print(" tck:");
            Serial.print(pinnames[tck]);
            Serial.print(" tdo:");
            Serial.print(pinnames[tdo]);
            Serial.print(" tdi:");
            Serial.println(pinnames[tdi]);
          }
        }
        else if(checkdataret > 1) {
          if (VERBOSE) {
            Serial.print("active ");
            Serial.print(" tck:");
            Serial.print(pinnames[tck]);
            Serial.print(" tdo:");
            Serial.print(pinnames[tdo]);
            Serial.print(" tdi:");
            Serial.print(pinnames[tdi]);
            Serial.print("  bits toggled:");
            Serial.println(checkdataret);
          }
        }
        else if(VERBOSE) Serial.println();
      }
    }
  }
}
/* ir_state()
 * Set TAP to Reset then ShiftIR. 
 * Shift in state[] as IR value.
 * Switch to ShiftDR state and end.
 */
static void ir_state(char state[], int tck, int tms, int tdi) 
{
#ifdef DEBUGIR
  Serial.println("ir_state: set TAP to ShiftIR:");
#endif
  tap_state(TAP_SHIFTIR, tck, tms);
#ifdef DEBUGIR
  Serial.print("ir_state: pulse_tdi to: ");
#endif
  for (int i=0; i < IR_LEN; i++) {
    ESP.wdtFeed();
    if (DELAY) delayMicroseconds(50);
    // TAP/TMS changes to Exit IR state (1) must be executed
    // at same time that the last TDI bit is sent:
    if (i == IR_LEN-1) {
      digitalWrite(pins[tms], HIGH); // ExitIR
#ifdef DEBUGIR
      Serial.print(" (will be in ExitIR after next bit) ");
#endif
    }
    pulse_tdi(tck, tdi, *state-'0');
#ifdef DEBUGIR
    Serial.print(*state-'0', DEC);
#endif
    // TMS already set to 0 "shiftir" state to shift in bit to IR
    *state++;
  }
#ifdef DEBUGIR
  Serial.println("\r\nir_state: Change TAP from ExitIR to ShiftDR:");
#endif
  // a reset would cause IDCODE instruction to be selected again
  tap_state("1100", tck, tms); // -1> UpdateIR -1> SelectDR -0> CaptureDR -0> ShiftDR
}
static void
sample(int iterations, int tck, int tms, int tdi, int tdo, int ntrst=IGNOREPIN)
{
  init_pins(tck, tms ,tdi, ntrst);  

  // send instruction and go to ShiftDR
  ir_state(IR_SAMPLE, tck, tms, tdi);

  // Tell TAP to go to shiftout of selected data register (DR)
  // is determined by the instruction we sent, in our case 
  // SAMPLE/boundary scan
  for (int i = 0; i < iterations; i++) {
    ESP.wdtFeed();
    // no need to set TMS. It's set to the '0' state to 
    // force a Shift DR by the TAP
 //   Serial.print(pulse_tdo(tck, tdo),DEC);
 //   if (i % 32  == 31 ) Serial.print(" ");
 //   if (i % 128 == 127) Serial.println();
  }
}

char ir_buf[IR_LEN+1];
static void
brute_ir(int iterations, int tck, int tms, int tdi, int tdo, int ntrst=IGNOREPIN)
{
  init_pins(tck, tms ,tdi, ntrst);  
  int iractive;
  byte tdo_read;
  byte prevread;
  for (uint32_t ir = 0; ir < (1UL << IR_LEN); ir++) { 
    ESP.wdtFeed();
    iractive=0;
    // send instruction and go to ShiftDR (ir_state() does this already)
    // convert ir to string.
    for (int i = 0; i < IR_LEN; i++) 
      ir_buf[i]=bitRead(ir, i)+'0';
    ir_buf[IR_LEN]=0;// terminate
    ir_state(ir_buf, tck, tms, tdi);
    // we are now in TAP_SHIFTDR state

    prevread = pulse_tdo(tck, tdo);
    for (int i = 0; i < iterations-1; i++) {
      ESP.wdtFeed();
      // no need to set TMS. It's set to the '0' state to force a Shift DR by the TAP
      tdo_read = pulse_tdo(tck, tdo);
      if (tdo_read != prevread) iractive++;
      
      if (iractive && VERBOSE) {
        Serial.print(prevread,DEC);
        if (i%16 == 15) Serial.print(" ");
        if (i%128 == 127) Serial.println();
      }
      prevread = tdo_read;
    }
    if (iractive && VERBOSE) {
      Serial.print(prevread,DEC);
      Serial.print("  Ir ");
      Serial.print(ir_buf);
      Serial.print("  bits changed ");
      Serial.println(iractive, DEC);
    }
  }
}

#if 0
static void
set_pattern()
{
  int i;
  char c;

  Serial.print("Enter new pattern of 1's or 0's (terminate with new line or '.'):\r\n"
               "> ");
  i = 0;
  while(1) {
    c = Serial.read();
    switch(c) {
    case '0':
    case '1':
      if(i < (PATTERN_LEN - 1) ) {
        pattern[i++] = c;
        Serial.print(c);
      }
      break;
    case '\n':
    case '\r':
    case '.': // bah. for the arduino serial console which does not pass us \n
      pattern[i] = 0;
      Serial.println();
      Serial.print("new pattern set [");
      Serial.print(pattern);
      Serial.println("]");
      return;
    }
  }
}
#endif

struct bs_frame_s*
JTAG_scan(struct bs_request_s *request)
{
  struct bs_frame_s *reply;
  int tck, tms, tdo, tdi, ntrst;
  int rv;
  uint32_t *reply_data;
  
  reply = (struct bs_frame_s *)malloc(BS_HEADER_SIZE + 5*4);
  if (reply == NULL)
    return NULL;
  reply->bs_payload_length = 6*4;
  reply_data = (uint32_t *)&reply->bs_payload[0];
  
  loopback_check();
  reply->bs_command = BS_REPLY_JTAG_DISCOVER_PINOUT;
  rv = scan(&tck, &tms, &tdi, &tdo, &ntrst);
  reply_data[0] = rv;
  if (rv > 0) {
   reply_data[1] = tck + 1;
   reply_data[2] = tms + 1;
   reply_data[3] = tdi + 1;
   reply_data[4] = tdo + 1;
   reply_data[5] = ntrst + 1;
  }
  return reply;
}

void
JTAG_reset(int ntrst)
{
  digitalWrite(pins[ntrst], HIGH);
  delay_us(50);
  digitalWrite(pins[ntrst], LOW);
  delay_us(50);
  digitalWrite(pins[ntrst], HIGH);
  delay_us(50);
}
