#include <SPI.h>
#include <Keypad.h>
#include <string.h>
#include <IntervalTimer.h>

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



void setup() {
  pinMode(PRI0, OUTPUT); 
  pinMode(PRI1, OUTPUT);
  pinMode(PRI2, OUTPUT);

  pinMode(PCR_Trigger, OUTPUT);
  pinMode(PCR_Reset, OUTPUT);
  pinMode(Fault_In, INPUT);

  pinMode(Current_Monitor, INPUT);

  pinMode(RF_Start, INPUT);

  
  Keypad keypad = Keypad(makeKeymap(keymap), row_pins, col_pins, rows, cols); 
  
  keypad.setDebounceTime(10); 

  
  start_menu     = 1; 
  operating_mode = 0;
  start_time     = 0;
  old_start_time = 0;


  PRF = 517.24;
  pulse_interval = ((1.0/PRF)*1e6); 
  pulse_mode = 0;
  outputPRIs(PRF);
  triggerTimings(PRF, pulse_mode);
  charge_flag = 1;
  discharge_flag = 1;
  PCR_flag = 1;
  attachInterrupt(digitalPinToInterrupt(RF_Start), startTest, FALLING);
} 

void outputPRIs(float PRF) {

  if (PRF <= 327.09) {
    digitalWriteFast(PRI0, HIGH);
    digitalWriteFast(PRI1, HIGH);
    digitalWriteFast(PRI2, HIGH);
  } 

  else if ((372.09 < PRF) && (PRF <= 446.52)) {
    digitalWriteFast(PRI0, LOW);
    digitalWriteFast(PRI1, HIGH);
    digitalWriteFast(PRI2, HIGH);
  } 

  else if ((446.52 < PRF) && (PRF <= 536.04)) {
    digitalWriteFast(PRI0, HIGH);
    digitalWriteFast(PRI1, LOW);
    digitalWriteFast(PRI2, HIGH);
  } 

  else if ((536.04 < PRF) && (PRF <= 643.29)) {
    digitalWriteFast(PRI0, LOW);
    digitalWriteFast(PRI1, LOW);
    digitalWriteFast(PRI2, HIGH);
  } 

  else if ((643.29 < PRF) && (PRF <= 771.90)) {
    digitalWriteFast(PRI0, HIGH);
    digitalWriteFast(PRI1, HIGH);
    digitalWriteFast(PRI2, LOW);
  } 

  else if ((771.90 < PRF) && (PRF <= 926.35)) {
    digitalWriteFast(PRI0, LOW);
    digitalWriteFast(PRI1, HIGH);
    digitalWriteFast(PRI2, LOW);
  } 

  else if ((926.35 < PRF) && (PRF <= 1111.72)) {
    digitalWriteFast(PRI0, HIGH);
    digitalWriteFast(PRI1, LOW);
    digitalWriteFast(PRI2, LOW);
  } 

  else if (1111.72 < PRF) {
    digitalWriteFast(PRI0, LOW);
    digitalWriteFast(PRI1, LOW);
    digitalWriteFast(PRI2, LOW);
  } 

} 

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
  }
}

char* concatenateKeys(char* key_string, char* word) {
  char combine[16];
  strcpy(combine, word);
  strcat(combine, key_string);
  return combine;
} 

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

void startTest() {
  if (charge_flag && discharge_flag && PCR_flag) {
    charge_flag = 0;
    discharge_flag = 0;
    PCR_flag = 0;
    charge_timer.begin(chargeTrigger, charge_trigger_time);
    discharge_timer.begin(dischargeTrigger, discharge_trigger_time);
    PCR_timer.begin(PCRTrigger, PCR_trigger_time);
  }
} 

void chargeTrigger() {
  digitalWriteFast(Charge_Trigger, LOW);
  charge_timer.begin(endChargePulse, charge_trigger_width);
}

void dischargeTrigger() {
  digitalWriteFast(Discharge_Trigger, LOW);
  discharge_timer.begin(endDischargePulse, discharge_trigger_width);
}

void PCRTrigger() {
  digitalWriteFast(PCR_Trigger, LOW);
  PCR_timer.begin(endPCRPulse, PCR_trigger_width);
}

void endChargePulse() {
  charge_flag = 1;
  digitalWriteFast(Charge_Trigger, HIGH);
  charge_timer.end();
}

void endDischargePulse() {
  discharge_flag = 1;
  digitalWriteFast(Discharge_Trigger, HIGH);
  discharge_timer.end();
}

void endPCRPulse() {
  PCR_flag = 1;
  digitalWriteFast(PCR_Trigger, HIGH);
  PCR_timer.end();
}

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


void triggerTimings(float PRF, int pulse_mode) {
  
  if (pulse_mode == 0) { 
    charge_trigger_time     = pulse_interval - (740.0 + 3.0);
    PCR_trigger_time        = pulse_interval - (105.0 + 3.0);
    discharge_trigger_delta = (2.24 + 3.0);
  }
  else if (pulse_mode == 1) { 
    charge_trigger_time     = pulse_interval - (1200.0 + 3.0);
    PCR_trigger_time        = pulse_interval - (205.0 + 3.0);
    discharge_trigger_delta = (4.48 + 3.0);
  } 
  discharge_trigger_time = pulse_interval - discharge_trigger_delta;
}



class Menu {
public:
  enum class ID : uint8_t {Options, Start, PRF, PulseMode, Test};

  Menu() : current(ID::Start) {} //Initialize current screen

  void reset() {
    current = ID::Start;
  } 

  void enter() {
    switch(current) {
      case ID::Options:   current = ID::Start; break;
      case ID::Start:     current = ID::PRF; break;
      case ID::PRF:       current = ID::PulseMode; break;
      case ID::PulseMode: current = ID::Test; break;
      case ID::Test:      break;
    } 
  } 

  void back() {
    switch(current) {
      case ID::Options:   break;
      case ID::Start:     current = ID::Options; break;
      case ID::PRF:       current = ID::Start; break;
      case ID::PulseMode: current = ID::PRF; break;
      case ID::Test:      break;
    } 
  } 

  const char* title() const {
    switch(current) {
      case ID::Options:   return "Options";
      case ID::Start:     return "Start";
      case ID::PRF:       return "PRF Input";
      case ID::PulseMode: return "Pulse Mode Input";
      case ID::Test:      return "Test Results";
    } 
  } 

  ID id() const {return current;}

}


void loop() {
  
  
  if    (menu.id() == Menu::ID::Start) { 
    
  } 

  while (menu.id() == Menu::ID::Start) {
    key = keyToString(keypad.getkey()); 
    
    if (key == "1") {      
      operating_mode = 1;
    } 

    else if (key == "2") { 
      operating_mode = 0;
    } 
  } 

  if    (menu.id() == Menu::ID::PRF) {

  } 

  while (menu.id() == Menu::ID::PRF) {

  } 

  if    (menu.id() == Menu::ID::PulseMode) {

  } 

  while (menu.id() == Menu::ID::PulseMode) {
    key = keyToString(keypad.getkey());
    
    if (key == "1") {      
      pulse_mode = 0;
    } 

    else if (key == "2") { 
      pulse_mode = 1;
    } 
  } 

  if    (menu.id() == Menu::ID::Test) {
    attachInterrupt(digitalPinToInterrupt(RF_Start), startTest, FALLING);
  }

  while (menu.id() == Menu::ID::Test) {
    
  } 
 
} 
