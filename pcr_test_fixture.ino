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
#include <string.h>
#include <IntervalTimer.h>

/*
 ***********************
 *** Pin Definitions ***
 ******************************************************************************************
 * Note: For clarity within the software, signals may not have the same name as they do   *
 *        in the schematic. All renamed signals have their schematic names listed in      *
 *        parentheses beside them. Refer to these descriptions to verify functionality.   *
 ******************************************************************************************
 * Charge_Trigger:    (Charge Trig 3.3) This is the charge trigger, which is buffered, sent to 
 *                      the line driver, then sent to the high voltage module.
 * Discharge_Trigger: (Discharge Trig 3.3) This is the discharge trigger, which is buffered, sent to 
 *                      the line driver, then sent to the high voltage module.
 * PRI[n]:            These pins control the nth PRI bit. Signals are sent to the differential 
 *                      line driver.
 * PCR_Trigger:       (TRIGIN) This is the PCR trigger, which is buffered, then sent to the PCR.
 * PCR_Reset:         This signal resets the fault on the PCR.
 * Fault_In:          (FLTOUT) This signal comes from the PCR and gives its fault status.
 * NC:                Pin not connected.
 * DC:                (D/C) Data/Command select line foe the display.
 * CS:                SPI Chip Select line for the display.
 * SDI:               SPI Serial Data In  line for the display.
 * SDO:               SPI Serial Data Out line for the display.
 * SCK:               SPI Clock line for the display.
 * Current_Monitor:   (Current MCU) This is the buffered output of the PCR current monitor pin.
 * Col[n]:            The nth column of switches on the keypad, starting from the left side.
 * Row[n]:            The nth row    of switches on the keypad, starting from the top.
 * RF_Start:          (RF START 3.3V) Input for the RF Start trigger from the transmitter.
 */

#define Charge_Trigger    0
#define Discharge_Trigger 1
#define PRI0              2
#define PRI1              3
#define PRI2              4
#define PCR_Trigger       5
#define PCR_Reset         6
#define Fault_In          7
#define NC                8
#define DC                9 
#define CS               10
#define SDI              11
#define SDO              12
#define SCK              13
#define Current_Monitor  14
#define Col3             15 
#define Col2             16 
#define Col1             17 
#define Col0             18 
#define Row3             19 
#define Row2             20 
#define Row1             21 
#define Row0             22 
#define RF_Start         23

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
int          charge_trigger_width
unsigned int current_monitor_raw;
float        current_monitor_voltage;
float        discharge_trigger_time;
int          discharge_trigger_width;
float        discharge_trigger_delta;
char         key;
int          menu_position;
int          operating_mode;
float        PCR_trigger_time;
int          PCR_trigger_width;
float        PRF;
int          PRF_menu;
float        pulse_interval;
int          start_menu;
uint_32t     start_time;

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

IntervalTimer charge_timer;
IntervalTimer discharge_timer;
IntervalTimer PCR_timer;
IntervalTimer pulse_timer;

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
  menu_position  = 1;
  
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

/*
 *********************
 *** keyToString() ***
 *********************
 *
 **********
 * Inputs *      
 **********
 *
 * key: This is the char that is returned from the getKey() function of the Keypad class
 *
 ***********
 * Outputs *
 ***********
 *
 * String version of key.
 *
 ***************
 * Description *
 ***************
 *
 * The function retruns a string that represents the keypad function of the key that was pressed.
 *   For digits, this is just the string version of the digit. For function buttons, this is the 
 *   name of the funtion that cooresponds with that key.
 */

const char* keyToString(char key) {
  switch (key) {
    case '#': return "Reset";
    case '*': return ".";
    case 'A': return "Enter";
    case 'B': return "Back";
    case 'C': return "Clear";
    case 'D': return "Stop";
    case '1': return "1";
    case '2': return "2";
    case '3': return "3";
    case '4': return "4";
    case '5': return "5";
    case '6': return "6";
    case '7': return "7";
    case '8': return "8";
    case '9': return "9";
    case '0': return "0";
    default:  return "";
  }/* switch (key) */
}/* keyToString(char key) */

/*
 *************************
 *** concatenateKeys() ***
 *************************
 *
 **********
 * Inputs *      
 **********
 *
 * key_string: This is the return value of the getKey() function of the keypad class after
 *               it has been passed through the keyToString() function.
 *
 * word: This is the word that is currently being typed on the keypad. For example, 
 *        "321." if the user were trying to input "321.24". If nothing has been typed,  
 *        pass the empty string, "", to the function.
 *
 ***********
 * Outputs *
 ***********
 *
 * combine: This is the concatenation of word and key_string. For example, 
 *            if word = "321" and key_string = ".", then combine "321.".
 *
 ***************
 * Description *
 ***************
 *
 * This function takes a string and appends another string on to the end of it.
 *
 */

char* concatenateKeys(char* key_string, char* word) {
  char combine[16];
  strcpy(combine, word);
  strcat(combine, key_string);
  return combine;
} /* concatenateKeys(char* key_string, char* word) */


void startTest() {
  start_time = micros();
  charge_timer.begin(chargeTrigger, charge_trigger_time);
  discharge_timer.begin(dischargeTrigger, discharge_trigger_time);
  PCR_timer.begin(PCRTrigger, PCR_trigger_time);
} /* startTest() */

void chargeTrigger() {
  digitalWriteFast(Charge_Trigger, Low);
  pulse_timer.begin(endPulse, charge_trigger_width);
  charge_timer.end();
}

void dischargeTrigger() {
  digitalWriteFast(Discharge_Trigger, Low);
  pulse_timer.begin(endPulse, discharge_trigger_width);
  discharge_timer.end();
}

void PCRTrigger() {
  digitalWriteFast(PCR_Trigger, Low);
  pulse_timer.begin(endPulse, PCR_trigger_width);
  PCR_timer.end();
}

void endPulse() {
  digitalWriteFast(Charge_Trigger, HIGH);
  digitalWriteFast(Discharge_Trigger, HIGH);
  digitalWriteFast(PCR_Trigger, HIGH);
  pulse_timer.end();
}

/*
 ************
 *** Menu ***
 ************
 *
 *************
 * Variables *
 *************
 *
 * current: This stores the current menu for each Menu object on an individual basis.
 *
 *********
 * Types *
 *********
 *
 * ID: Each ID subtype represents a different menu, for example Start is the start menu.
 *
 *************
 * Functions *
 *************
 * 
 * enter(): This function switches the menu to the next menu in the sequence.
 *            For example, if menu.enter() is called and the current menu is PRF,
 *            the enter function switches the current menu to PulseMode.
 *
 * back():  This function switches the menu to the previous menu in the sequence.
 *            For example, if menu.back() is called and the current menu is PRF,
 *            the back function switches the current menu to Start.
 *
 * title(): This function returns the title of the current menu. 
 *            For example, if menu.title() is called and the current menu is PRF,
 *            the title function returns the string "PRF Input".
 *
 * id():    This function returns the ID of the current menu.
 *            For example, if menu.id() is called and the current menu is PRF,
 *            the id function returns the ID Menu::ID::PRF.
 *            Notice that the return type of the function is the ID data type.
 */

class Menu {
public:
  enum class ID : uint8_t {Options, Start, PRF, PulseMode, Test};

  Menu() : current(ID::Start) {} //Initialize current screen

  void enter() {
    switch(current) {
      case ID::Options:   current = ID::Start; break;
      case ID::Start:     current = ID::PRF; break;
      case ID::PRF:       current = ID::PulseMode; break;
      case ID::PulseMode: current = ID::Test; break;
      case ID::Test:      break;
    } /* switch(current) */
  } /* enter() */

  void back() {
    switch(current) {
      case ID::Options:   break;
      case ID::Start:     current = ID::Options; break;
      case ID::PRF:       current = ID::Start; break;
      case ID::PulseMode: current = ID::PRF; break;
      case ID::Test:      break;
    } /* switch(current) */
  } /* back() */

  const char* title() const {
    switch(current) {
      case ID::Options:   return "Options";
      case ID::Start:     return "Start";
      case ID::PRF:       return "PRF Input";
      case ID::PulseMode: return "Pulse Mode Input";
      case ID::Test:      return "Test Results";
    } /* switch(current) */
  } /* title() */

  ID id() const {return current;}

}/* Menu */

Menu menu;

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
  if    (menu.id() == Menu::ID::Start) { 
    /* Output Start Menu */
  } /* if (menu.id() == Menu::ID::Start) */

  while (menu.id() == Menu::ID::Start) {
    key = keyToString(keypad.getkey());
    
    if (key == "1") {      /* Select transmitter dependent mode */
      operating_mode = 1;
    } /* if (key == '1') */

    else if (key == "2") { /* Select High Voltage Module control mode */
      operating_mode = 0;
    } /* else if (key == '2') */
  } /* while (menu.id() == Menu::ID::Start) */

  if    (menu.id() == Menu::ID::PRF) {

  } /* if (menu.id() == Menu::ID::PRF) */

  while (menu.id() == Menu::ID::PRF) {

  } /* while (menu.id() == Menu::ID::PRF) */

  if    (menu.id() == Menu::ID::PulseMode) {

  } /* if (menu.id() == Menu::ID::PulseMode) */

  while (menu.id() == Menu::ID::PulseMode) {
    key = keyToString(keypad.getkey());
    
    if (key == "1") {      /* Select Short Pulse */
      pulse_mode = 0;
    } /* if (key == '1') */

    else if (key == "2") { /* Select Long Pulse */
      pulse_mode = 1;
    } /* else if (key == '2') */
  } /* while (menu.id() == Menu::ID::PulseMode) */

  if    (menu.id() == Menu::ID::Test) {
      attachInterrupt(digitalPinToInterrupt(RF_Start), startTest, FALLING);
  }/* if (menu.id() == Menu::ID::Test) */

  while (menu.id() == Menu::ID::Test) {
    discharge_trigger_time = pulse_interval - discharge_trigger_delta;
    if (pulse_mode == 0) { /* If Short Pulse */
      charge_trigger_time = pulse_interval - 740.0;
      PCR_trigger_time    = pulse_interval - 105.0;
    }/* if (pulse_mode == 0) */
    else if (pulse_mode == 1) { /* If Long Pulse */
      charge_trigger_time = pulse_interval - 1200.0;
      PCR_trigger_time    = pulse_interval - 205;
    } /* else if (pulse_mode == 1) */
  } /* while (menu.id() == Menu::ID::Test) */

} /* loop() */