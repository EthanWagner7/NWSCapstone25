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
#define test_pulse       15 
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
int           charge_trigger_time;
int           charge_trigger_width = 1;
unsigned int  current_monitor_raw;
float         current_monitor_voltage;
int           discharge_trigger_time;
int           discharge_trigger_width = 1;
int           discharge_trigger_delta;
char          key;
int           menu_position;
unsigned long old_start_time;
int           operating_mode;
int           PCR_trigger_time;
int           PCR_trigger_width = 1;
float         PRF;
int           PRF_menu;
int           pulse_interval;
int           pulse_mode;
int           start_menu;
unsigned long start_time;
int           test_flag;


IntervalTimer charge_timer;
IntervalTimer discharge_timer;
IntervalTimer PCR_timer;
IntervalTimer clockTimer;



void startTest() {
  //old_start_time = start_time;
  //start_time = micros();
  test_flag = 0;
  charge_timer.begin(chargeTrigger, charge_trigger_time);
  discharge_timer.begin(dischargeTrigger, discharge_trigger_time);
  PCR_timer.begin(PCRTrigger, PCR_trigger_time);
} /* startTest() */


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
  digitalWriteFast(Charge_Trigger, HIGH);
  charge_timer.end();
}

void endDischargePulse() {
  digitalWriteFast(Discharge_Trigger, HIGH);
  discharge_timer.end();
}

void endPCRPulse() {
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
  detachInterrupt(digitalPinToInterrupt(RF_Start));
}

void endTest() {
  test_flag = 1;
}

void toggleClock() {
  digitalWriteFast(test_pulse, !digitalReadFast(test_pulse));
}

void setup() {
  pinMode(PRI0, OUTPUT); 
  pinMode(PRI1, OUTPUT);
  pinMode(PRI2, OUTPUT);

  pinMode(Charge_Trigger, OUTPUT);
  pinMode(Discharge_Trigger, OUTPUT);
  pinMode(test_pulse, OUTPUT);

  pinMode(PCR_Trigger, OUTPUT);
  pinMode(PCR_Reset, OUTPUT);
  pinMode(Fault_In, INPUT);

  pinMode(Current_Monitor, INPUT);

  pinMode(RF_Start, INPUT);
  reset();
  attachInterrupt(digitalPinToInterrupt(RF_Start), startTest, FALLING);
  test_flag = 1;
  PRF = 500.0;
  pulse_interval = 2000;
  pulse_mode = 1;
  if (pulse_mode == 0) { /* If Short Pulse */
      charge_trigger_time     = pulse_interval - 740;
      PCR_trigger_time        = pulse_interval - 105;
      discharge_trigger_delta = 2;
  }/* if (pulse_mode == 0) */
  else if (pulse_mode == 1) { /* If Long Pulse */
      charge_trigger_time     = pulse_interval - 1200;
      PCR_trigger_time        = pulse_interval - 205;
      discharge_trigger_delta = 4;
  } /* else if (pulse_mode == 1) */
  discharge_trigger_time = pulse_interval - discharge_trigger_delta;
  clockTimer.begin(toggleClock, 2000);
}


void loop() {

}





