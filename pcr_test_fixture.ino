/*
 ******************************************************************************************
 *** PCR Test Fixture Software                                                          ***
 *** Version:     0.1                                                                   ***
 *** Date:        11-09-2025                                                            ***
 *** Author:      Ethan Wagner (ewagner@ou.edu)                                         ***
 *** Board:       Teensy 4.0                                                            ***
 *** Repository:  https://github.com/EthanWagner7/NWSCapstone25                         ***
 *** Description: This is the software that controls the Post Charge Regulator (PCR)    ***
 ***                test fixture designed at OU in Fall of 2025. For more details and   ***
 ***                version information, see the repository linked above.               ***
 ******************************************************************************************
 */

#include <SPI.h>
#include <Keypad.h>

/*
 ***********************
 *** Pin Definitions ***
 ******************************************************************************************
 * Note: For clarity within the software, signals may not have the same name as they do   *
 *        in the schematic. All renamed signals have their schematic names listed in      *
 *        parentheses beside them. Refer to these descriptions to verify functionality.   *
 ******************************************************************************************
 * PRI[n]:          These pins control the nth PRI bit. Signals are sent to the differential 
 *                    line driver.
 * PCR_Trigger:     (TRIGIN) This is the PCR trigger, which is buffered, then sent to the PCR.
 * PCR_Reset:       This signal resets the fault on the PCR.
 * Fault_In:        (FLTOUT) This signal comes from the PCR and gives its fault status.
 * DC:              (D/C) Data/Command select line foe the display.
 * CS:              SPI Chip Select line for the display.
 * SDI:             SPI Serial Data In  line for the display.
 * SDO:             SPI Serial Data Out line for the display.
 * SCK:             SPI Clock line for the display.
 * Current_Monitor: (Current MCU) This is the buffered output of the PCR current monitor pin.
 * Col[n]:          The nth column of switches on the keypad, starting from the left side.
 * Row[n]:          The nth row    of switches on the keypad, starting from the top.
 * RF_Start:        (RF START 3.3V) Input for the RF Start trigger from the transmitter.
 */

#define PRI0             2
#define PRI1             3
#define PRI2             4
#define PCR_Trigger      5
#define PCR_Reset        6
#define Fault_In         7
#define DC               9
#define CS              10
#define SDI             11
#define SDO             12
#define SCK             13
#define Current_Monitor 14
#define Col3            15 
#define Col2            16 
#define Col1            17 
#define Col0            18 
#define Row3            19 
#define Row2            20 
#define Row1            21 
#define Row0            22 
#define RF_Start        23

/*
 *****************************
 *** Variable Declarations ***
 *****************************
 * bleed_off_current:       The value of the bleed-off current in mA calculated from the current monitor
 * bleed_time:              The time in micros before the next pulse start trigger that the PCR trigger 
 *                            is sent.
 * charge_trigger_time:     The time after pulse start that the charge trigger is sent.
 *                            = pulse_interval - 740;
 * current_monitor_raw:     The ADC measurement of the current monitor pin. It is an integer value
 *                            between 0 and ((2^10) - 1) that is proportional to the voltage.
 * current_monitor_voltage: current_monitor_raw converted into a voltage.
 *                            = (current_monitor_raw / ((2^10) - 1)) * 3.3;
 *                            this is because 3.3V is the maximum readable voltage.
 * discharge_trigger_time:  The time after pulse start that the discharge trigger is sent.
 *                            = charge_trigger_time + 720 + discharge_trigger_delta;
 * discharge_trigger_delta: The variance in time between the charge and discharge triggers.
 *                            (See Modulator Timing Relationships Diagram for more details)
 * PCR_trigger_time:        The time after pulse start that the PCR trigger is sent.
 *                            = pulse_interval - bleed_time;
 * PRF:                     The Pulse Repitition frequency.
 *                            = 1/pulse_interval;
 * pulse_interval:          The time between the falling edges of the RF start trigger.
 * rows:                    The number of rows     in the keypad.
 * cols:                    The number of colmumns in the keypad.
 * keymap:                  4x4 array that gives the layout of the keys on the keypad.
 * row_pins:                Array that gives the pin connections of the rows    on the keypad.
 * col_pins:                Array that gives the pin connections of the columns on the keypad.
 */

float        bleed_off_current;
float        bleed_time;
float        charge_trigger_time;
unsigned int current_monitor_raw;
float        current_monitor_voltage;
float        discharge_trigger_time;
float        discharge_trigger_delta;
char         key;
int          operating_mode;
float        PCR_trigger_time;
float        PRF;
int          PRF_menu;
float        pulse_interval;
int          start_menu;

const byte   rows = 4;
const byte   cols = 4;

char keymap[rows][cols] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

byte row_pins[rows] = { Row0, Row1, Row2, Row3 };
byte col_pins[cols] = { Col0, Col1, Col2, Col3 };

/*
 ***************
 *** setup() ***
 ***************
 *
 **********
 * Inputs *      
 **********
 * None
 ***********
 * Outputs *
 ***********
 * None
 ***************
 * Description *
 ***************
 * This function is called once (automatically) at program start for initialization.
 */

void setup() {
  pinMode(PRI0, OUTPUT); 
  pinMode(PRI1, OUTPUT);
  pinMode(PRI2, OUTPUT);

  pinMode(PCR_Trigger, OUTPUT);
  pinMode(PCR_Reset, OUTPUT);
  pinMode(Fault_In, INPUT);

  pinMode(Current_Monitor, INPUT);

  pinMode(RF_Start, INPUT);

  /* Initializing keypad */
  Keypad keypad = Keypad(makeKeymap(keymap), row_pins, col_pins, rows, cols); 
  /* 10ms non-blocking debounce (specified in keypad datasheet) */
  keypad.setDebounceTime(10); 

  /* Set start_menu to 0 to disable operating mode prompt*/
  start_menu     = 1; 
  operating_mode = 0;

} /* setup() */

/*
 ********************
 *** outputPRIs() ***
 ********************
 *
 **********
 * Inputs *      
 **********
 * PRF: The Pulse Repetition Frequency that the system is operating at.
 ***********
 * Outputs *
 ***********
 * None
 ***************
 * Description *
 ***************
 * This function takes the PRF, determines the appropriate state of the PRI bits, and writes them.
 */

void outputPRIs(float PRF) {

  if (PRF <= 327.09) {
    digitalWriteFast(PRI0, HIGH);
    digitalWriteFast(PRI1, HIGH);
    digitalWriteFast(PRI2, HIGH);
  } /* if (PRF <= 327.09) */

  else if ((372.09 < PRF) && (PRF <= 446.52)) {
    digitalWriteFast(PRI0, LOW);
    digitalWriteFast(PRI1, HIGH);
    digitalWriteFast(PRI2, HIGH);
  } /* else if ((372.09 < PRF) && (PRF <= 446.52)) */

  else if ((446.52 < PRF) && (PRF <= 536.04)) {
    digitalWriteFast(PRI0, HIGH);
    digitalWriteFast(PRI1, LOW);
    digitalWriteFast(PRI2, HIGH);
  } /* else if ((446.52 < PRF) && (PRF <= 536.04)) */

  else if ((536.04 < PRF) && (PRF <= 643.29)) {
    digitalWriteFast(PRI0, LOW);
    digitalWriteFast(PRI1, LOW);
    digitalWriteFast(PRI2, HIGH);
  } /* else if ((536.04 < PRF) && (PRF <= 643.29)) */

  else if ((643.29 < PRF) && (PRF <= 771.90)) {
    digitalWriteFast(PRI0, HIGH);
    digitalWriteFast(PRI1, HIGH);
    digitalWriteFast(PRI2, LOW);
  } /* else if ((643.29 < PRF) && (PRF <= 771.90)) */

  else if ((771.90 < PRF) && (PRF <= 926.35)) {
    digitalWriteFast(PRI0, LOW);
    digitalWriteFast(PRI1, HIGH);
    digitalWriteFast(PRI2, LOW);
  } /* else if ((771.90 < PRF) && (PRF <= 926.35)) */

  else if ((926.35 < PRF) && (PRF <= 1111.72)) {
    digitalWriteFast(PRI0, HIGH);
    digitalWriteFast(PRI1, LOW);
    digitalWriteFast(PRI2, LOW);
  } /* else if ((926.35 < PRF) && (PRF <= 1111.72)) */

  else if (1111.72 < PRF) {
    digitalWriteFast(PRI0, LOW);
    digitalWriteFast(PRI1, LOW);
    digitalWriteFast(PRI2, LOW);
  } /* else if (1111.72 < PRF) */

} /* outputPRIs() */

struct NumberInput {
  const int max             = 16;
  char      buffer[max + 1] = {0};
  int       length          = 0;

  void clear() {
    length = 0;
    buffer[0] = '\0';
  }

  void backspace() {
    if (length > 0) {
      buffer[length - 1] = '\0'
    } /* if (length > 0) */
  }

  void addDigit(char digit) {
    if (length < max) {
      buffer[length + 1] = digit;
      buffer[length]     = '\0'
    }
  }

  void addDecimal() {
    int has_decimal = 0;
    for (i = 0; i < length; i++) {
      if (buffer[i] == '.'){
        has_decimal = 1;
      }
    }
    if (len < max && !has_decimal) {

    }
  }
} /* NumberInput */


/*
 **************
 *** loop() ***
 **************
 *
 **********
 * Inputs *      
 **********
 * None
 ***********
 * Outputs *
 ***********
 * None
 ***************
 * Description *
 ***************
 * This function loops forever after setup() is called.
 *  It is similar to a while(1) loop within the main()
 *  function of a typical C program.
 */
 
void loop() {
  if (start_menu) { 
    /* Output Start Menu */
  } /* if (start_menu) */
  while (start_menu) {
    key = keypad.getkey();
    
    if (key == '1') {      /* Select transmitter dependent mode */
      operating_mode = 1;
      start_menu     = 0;
    } /* if (key == '1') */

    else if (key == '2') { /* Select High Voltage Module control mode */
      operating_mode = 0;
      start_menu     = 0;
    } /* else if (key == '2') */
  } /* while (start_menu) */

  /* Read PRF */
  
  /* Read Pulse Length (Long or Short) */

} /* loop() */