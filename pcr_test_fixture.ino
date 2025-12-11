/*
 ******************************************************************************************
 *** PCR Test Fixture Software                                                          ***
 *** Version:     0.4                                                                   ***
 *** Date:        12-10-2025                                                            ***
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
 * charge_flag:             This is a logical value that is 1 when it is safe to send the charge
 *                            trigger, and 0 otherwise.
 * charge_trigger_time:     The time after pulse start that the charge trigger is sent.
 *                            = pulse_interval - 740;
 * charge_trigger_width:    The width of the charge trigger in microseconds.
 * current_monitor_raw:     The ADC measurement of the current monitor pin. It is an integer value
 *                            between 0 and ((2^10) - 1) that is proportional to the voltage.
 * current_monitor_voltage: current_monitor_raw converted into a voltage.
 *                            = (current_monitor_raw / ((2^10) - 1)) * 3.3;
 *                            this is because 3.3V is the maximum readable voltage.
 * discharge_flag:          This is a logical value that is 1 when it is safe to send the discharge
 *                            trigger, and 0 otherwise.
 * discharge_trigger_time:  The time after pulse start that the discharge trigger is sent.
 *                            = charge_trigger_time + 720 + discharge_trigger_delta;
 * discharge_trigger_delta: The variance in time between the charge and discharge triggers.
 *                            (See Modulator Timing Relationships Diagram for more details)
 * discharge_trigger_width: The width of the discharge trigger in microseconds.
 * key:                     This holds the value of the key that has been pressed on the keypad.
 * old_start_time:          The start time of the previous pulse cycle.
 * operating_mode:          The mode the system is operating in, 1 for Transmitter-Dependent, and
 *                            0 for High Voltage Control.
 * PCR_flag:                This is a logical value that is 1 when it is safe to send the PCR
 *                            trigger, and 0 otherwise.
 * PCR_trigger_time:        The time after pulse start that the PCR trigger is sent.
 *                            = pulse_interval - bleed_time;
 * charge_trigger_width:    The width of the PCR trigger in microseconds.
 * PRF:                     The Pulse Repitition frequency.
 *                            = 1/pulse_interval;
 * pulse_interval:          The time between the falling edges of the RF start trigger.
 *                            = 1/PRF;
 * start_menu:              Logical value that controls whether the start menu should be displayed,
 *                            0 for off, 1 for on.
 * start_time:              The start time of the current pulse cycle in microseconds.
 */

float         bleed_off_current;
float         bleed_time;
int           charge_flag;
float         charge_trigger_time;
int           charge_trigger_width;
unsigned int  current_monitor_raw;
float         current_monitor_voltage;
int           discharge_flag;
float         discharge_trigger_time;
int           discharge_trigger_width;
float         discharge_trigger_delta;
char          key;
unsigned long old_start_time;
int           operating_mode;
int           PCR_flag;
float         PCR_trigger_time;
int           PCR_trigger_width;
float         PRF;
float         pulse_interval;
int           start_menu;
unsigned long start_time;


/*
 **************************
 * Non-Standard Variables *
 **************************
 * 
 * rows:                    The number of rows     in the keypad.
 * cols:                    The number of colmumns in the keypad.
 * keymap:                  4x4 matrix that gives the layout of the keys on the keypad.
 * row_pins:                Array that gives the pin connections of the rows    on the keypad.
 * col_pins:                Array that gives the pin connections of the columns on the keypad.
 * charge_timer:            This is a timer that manages the timing of the charge    trigger.
 * discharge_timer:         This is a timer that manages the timing of the discharge trigger.
 * PCR_timer:               This is a timer that manages the timing of the PCR       trigger.
 * menu:                    This is an object of the Menu class, which is in the code below.
 *                            Menu objects essentially serve as a state register and state transition 
 *                            logic for a set of menus.
 */

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
Menu menu;

/*
 ***************
 *** setup() ***
 ***************
 *
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
  start_time     = 0;
  old_start_time = 0;


  /* Trigger test inputs */
  PRF = 517.24;
  pulse_interval = ((1.0/PRF)*1e6); /* In microseconds */
  pulse_mode = 0;
  outputPRIs(PRF);
  triggerTimings(PRF, pulse_mode);
  charge_flag = 1;
  discharge_flag = 1;
  PCR_flag = 1;
  attachInterrupt(digitalPinToInterrupt(RF_Start), startTest, FALLING);
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
 **********
 * Output *
 **********
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
 **********
 * Output *
 **********
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
 **********
 * Output *
 **********
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

/*
 *************************
 *** timeDifference () ***
 *************************
 *
 **********
 * Inputs *      
 **********
 *
 * time1: The earlier time of the pair.
 * time2: The later   time of the pair.
 *
 **********
 * Output *
 **********
 *
 * time_difference: The difference in time between the two times.
 *
 ***************
 * Description *
 ***************
 *
 * This function takes two times and returns the difference, regardless of rollover.
 * It exists because the micros() function, which gives the system time in microseconds,
 * will roll over every 2^32 - 1 microseconds, which is about every 70 minutes. In the case that
 * the system uptime were greater than that value, simply taking the difference of a time before the 
 * rollover and a time after would yield a negative result, which could break things...
 *
 */

unsigned long timeDifference(time1, time2) {
  unsigned long time_difference;
  if (time1 > time2) {
    time_difference = (((2^32) - 1) - time1) + time2);
  }
  else {
    time_difference = time2 - time1;
  }
  return time_difference;
}

/*
 *******************
 *** startTest() ***
 *******************
 *
 ***************
 * Description *
 ***************
 *
 * This function is called by the interrupt attached to the falling edge of the RF start trigger.
 *  It starts the timers that control the charge, discharge, and PCR triggers. It also reads the start
 *  time of the pulse cycle for PRF recalculation.
 *
 */

void startTest() {
  if (charge_flag && discharge_flag && PCR_flag) {
    charge_flag = 0;
    discharge_flag = 0;
    PCR_flag = 0;
    charge_timer.begin(chargeTrigger, charge_trigger_time);
    discharge_timer.begin(dischargeTrigger, discharge_trigger_time);
    PCR_timer.begin(PCRTrigger, PCR_trigger_time);
  }
  
} /* startTest() */

/*
 ***********************
 *** chargeTrigger() ***
 ***********************
 *
 ***************
 * Description *
 ***************
 *
 * This function is called by the timer interrupt for the charge trigger. It sets the charge trigger
 *  pin low, then starts a timer that sets it high again after the appropriate amount of time.
 *
 */

void chargeTrigger() {
  digitalWriteFast(Charge_Trigger, LOW);
  charge_timer.begin(endChargePulse, charge_trigger_width);
}

/*
 **************************
 *** dischargeTrigger() ***
 **************************
 *
 ***************
 * Description *
 ***************
 *
 * This function is called by the timer interrupt for the discharge trigger. It sets the discharge trigger
 *  pin low, then starts a timer that sets it high again after the appropriate amount of time.
 *
 */

void dischargeTrigger() {
  digitalWriteFast(Discharge_Trigger, LOW);
  discharge_timer.begin(endDischargePulse, discharge_trigger_width);
}

/*
 ***********************
 *** PCRTrigger() ***
 ***********************
 *
 ***************
 * Description *
 ***************
 *
 * This function is called by the timer interrupt for the PCR trigger. It sets the PCR trigger
 *  pin low, then starts a timer that sets it high again after the appropriate amount of time.
 *
 */

void PCRTrigger() {
  digitalWriteFast(PCR_Trigger, LOW);
  PCR_timer.begin(endPCRPulse, PCR_trigger_width);
}

/*
 ************************
 *** endChargePulse() ***
 ************************
 *
 ***************
 * Description *
 ***************
 *
 * This function is called by the timer interrupt from the chargeTrigger() function.
 *  It sets the charge_flag to signify that the charge trigger has been sent and sets
 *  the charge trigger pin high.
 *
 */

void endChargePulse() {
  charge_flag = 1;
  digitalWriteFast(Charge_Trigger, HIGH);
  charge_timer.end();
}

/*
 ***************************
 *** endDischargePulse() ***
 ***************************
 *
 ***************
 * Description *
 ***************
 *
 * This function is called by the timer interrupt from the dischargeTrigger() function.
 *  It sets the discharge_flag to signify that the discharge trigger has been sent and sets
 *  the discharge trigger pin high.
 *
 */

void endDischargePulse() {
  discharge_flag = 1;
  digitalWriteFast(Discharge_Trigger, HIGH);
  discharge_timer.end();
}

/*
 *********************
 *** endPCRPulse() ***
 *********************
 *
 ***************
 * Description *
 ***************
 *
 * This function is called by the timer interrupt from the PCRTrigger() function.
 *  It sets the PCR_flag to signify that the PCR trigger has been sent and sets
 *  the PCR trigger pin high.
 *
 */

void endPCRPulse() {
  PCR_flag = 1;
  digitalWriteFast(PCR_Trigger, HIGH);
  PCR_timer.end();
}

/*
 ***************
 *** reset() ***
 ***************
 *
 ***************
 * Description *
 ***************
 *
 * This function stops all timers, resets triggers detaches the RF start interrupt, and resets the menu.
 *
 */

void reset() {
  digitalWriteFast(Charge_Trigger, HIGH);
  digitalWriteFast(Discharge_Trigger, HIGH);
  digitalWriteFast(PCR_Trigger, HIGH);
  charge_timer.end();
  discharge_timer.end();
  PCR_timer.end();
  menu.reset();
  detachInterrupt(digitalPinToInterrupt(RF_START));
}

/*
 ************************
 *** triggerTimings() ***
 ************************
 *
 **********
 * Inputs *      
 **********
 *
 * PRF: The PRF that the transmitter is operating at.
 * pulse_mode: 0 for short pulse, 1 for long pulse/
 *
 **********
 * Output *
 **********
 *
 * None
 *
 ***************
 * Description *
 ***************
 *
 * This function sets the timings of all of the triggers given the PRF and pulse mode. For information
 *  on where these values come from, see the modulator timing relationships diagram.
 *
 */

void triggerTimings(float PRF, int pulse_mode) {
  /* 
   * The constant 3.0 is added to triggers to advance the timing to offset the latency in IntervalTimers
   *  This value is not precise and could be improved through experimentation.
   */
  if (pulse_mode == 0) { /* If Short Pulse */
    charge_trigger_time     = pulse_interval - (740.0 + 3.0);
    PCR_trigger_time        = pulse_interval - (105.0 + 3.0);
    discharge_trigger_delta = (2.24 + 3.0);
  }/* if (pulse_mode == 0) */
  else if (pulse_mode == 1) { /* If Long Pulse */
    charge_trigger_time     = pulse_interval - (1200.0 + 3.0);
    PCR_trigger_time        = pulse_interval - (205.0 + 3.0);
    discharge_trigger_delta = (4.48 + 3.0);
  } /* else if (pulse_mode == 1) */
  discharge_trigger_time = pulse_interval - discharge_trigger_delta;
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
 * reset(): This function sets the menu back to Start.
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

  void reset() {
    current = ID::Start;
  } /* reset() */

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


/*
 **************
 *** loop() ***
 **************
 *
 ***************
 * Description *
 ***************
 * This function loops forever after setup() is called.
 *  It is similar to a while(1) loop within the main()
 *  function of a typical C program.
 */
 
void loop() {
  
  /* 
   * This menu skeleton does nothing right now. I'm leaving it so that future teams can get an idea
   *  of how the Menu class works in practice.
   */
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
    
  } /* while (menu.id() == Menu::ID::Test) */
 
} /* loop() */
