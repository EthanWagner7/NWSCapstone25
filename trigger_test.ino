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
float         charge_trigger_time;
int           charge_trigger_width = 20;
unsigned int  current_monitor_raw;
float         current_monitor_voltage;
float         discharge_trigger_time;
int           discharge_trigger_width = 20;
int           discharge_trigger_delta;
char          key;
int           menu_position;
unsigned long old_start_time;
int           operating_mode;
float         PCR_trigger_time;
int           PCR_trigger_width = 20;
float         PRF;
float         pulse_interval;
int           pulse_mode;
int           start_menu;
unsigned long start_time;
int           charge_flag;
int           discharge_flag;
int           PCR_flag;



IntervalTimer charge_timer;
IntervalTimer discharge_timer;
IntervalTimer PCR_timer;
IntervalTimer clockTimer;



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
  detachInterrupt(digitalPinToInterrupt(RF_Start));
}



void toggleClock() {
  int low_time = 1;
  if (digitalReadFast(test_pulse)){
    clockTimer.begin(toggleClock, low_time);
  }
  else {
    clockTimer.begin(toggleClock, (pulse_interval - low_time));
  }
  digitalWriteFast(test_pulse, !digitalReadFast(test_pulse));
}

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
  PRF = 517.24;
  pulse_interval = ((1.0/PRF)*1e6);
  float pulse_cycle_time = pulse_interval/2.0;
  pulse_mode = 0;
  outputPRIs(PRF);
  if (pulse_mode == 0) { /* If Short Pulse */
      charge_trigger_time     = pulse_interval - (740.0 + 3);
      PCR_trigger_time        = pulse_interval - (105.0 + 3);
      discharge_trigger_delta = (2.24 + 3);
  }/* if (pulse_mode == 0) */
  else if (pulse_mode == 1) { /* If Long Pulse */
      charge_trigger_time     = pulse_interval - (1200.0 + 3);
      PCR_trigger_time        = pulse_interval - (205.0 + 3);
      discharge_trigger_delta = (4.48 + 3);
  } /* else if (pulse_mode == 1) */
  discharge_trigger_time = pulse_interval - discharge_trigger_delta;
  charge_flag = 1;
  discharge_flag = 1;
  PCR_flag = 1;
  //clockTimer.begin(toggleClock, pulse_cycle_time); 
}


void loop() {

}
