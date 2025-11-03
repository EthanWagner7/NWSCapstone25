/*
 ******************************************************************************************
 *** PCR Test Fixture Software                                                          ***
 *** Version:     0.0                                                                   ***
 *** Date:        11-02-2025                                                            ***
 *** Author:      Ethan Wagner (ewagner@ou.edu)                                         ***
 *** Board:       Teensy 4.0                                                            ***
 *** Repository:  https://github.com/EthanWagner7/NWSCapstone25                         ***
 *** Description: This is the software that controls the Post Charge Regulator (PCR)    ***
 ***                test fixture designed at OU in Fall of 2025. For more details and   ***
 ***                version information, see the repository linked above.               ***
 ******************************************************************************************
 */

#include <SPI.h>

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
 * Keypad[n]:       These pins are connected to the nth pin on the keypad. These should not
 *                    be confused with the numbers 1-8 on the keypad; rather, each pin connects
 *                    to a row or column in the 4x4 matrix of switches.
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
#define Keypad8         15
#define Keypad7         16
#define Keypad6         17
#define Keypad5         18
#define Keypad4         19
#define Keypad3         20
#define Keypad2         21
#define Keypad1         22

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
 *                            between 0 and 2^10 that is proportional to the voltage.
 * current_monitor_voltage: current_monitor_raw converted into a voltage.
 *                            = (current_monitor_raw / 2^10) * 3.3;
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
 */

float        bleed_off_current;
float        bleed_time;
float        charge_trigger_time;
unsigned int current_monitor_raw;
float        current_monitor_voltage;
float        discharge_trigger_time;
float        discharge_trigger_delta;
float        PCR_trigger_time;
float        PRF;
float        pulse_interval;

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
    digitalWrite(PRI0, HIGH);
    digitalWrite(PRI1, HIGH);
    digitalWrite(PRI2, HIGH);
  } /* if (PRF <= 327.09) */

  else if ((372.09 < PRF) && (PRF <= 446.52)) {
    digitalWrite(PRI0, LOW);
    digitalWrite(PRI1, HIGH);
    digitalWrite(PRI2, HIGH);
  } /* else if ((372.09 < PRF) && (PRF <= 446.52)) */

  else if ((446.52 < PRF) && (PRF <= 536.04)) {
    digitalWrite(PRI0, HIGH);
    digitalWrite(PRI1, LOW);
    digitalWrite(PRI2, HIGH);
  } /* else if ((446.52 < PRF) && (PRF <= 536.04)) */

  else if ((536.04 < PRF) && (PRF <= 643.29)) {
    digitalWrite(PRI0, LOW);
    digitalWrite(PRI1, LOW);
    digitalWrite(PRI2, HIGH);
  } /* else if ((536.04 < PRF) && (PRF <= 643.29)) */

  else if ((643.29 < PRF) && (PRF <= 771.90)) {
    digitalWrite(PRI0, HIGH);
    digitalWrite(PRI1, HIGH);
    digitalWrite(PRI2, LOW);
  } /* else if ((643.29 < PRF) && (PRF <= 771.90)) */

  else if ((771.90 < PRF) && (PRF <= 926.35)) {
    digitalWrite(PRI0, LOW);
    digitalWrite(PRI1, HIGH);
    digitalWrite(PRI2, LOW);
  } /* else if ((771.90 < PRF) && (PRF <= 926.35)) */

  else if ((926.35 < PRF) && (PRF <= 1111.72)) {
    digitalWrite(PRI0, HIGH);
    digitalWrite(PRI1, LOW);
    digitalWrite(PRI2, LOW);
  } /* else if ((926.35 < PRF) && (PRF <= 1111.72)) */

  else if (1111.72 < PRF) {
    digitalWrite(PRI0, LOW);
    digitalWrite(PRI1, LOW);
    digitalWrite(PRI2, LOW);
  } /* else if (1111.72 < PRF) */

} /* outputPRIs() */

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
 *  It is functionally identical to a while(1) loop within
 *  the main() function of a typical C program.
 */
 
void loop() {
  /* Output Start Menu */

  /* Read Operating Mode */

  /* Read PRF */
  
  /* Read Pulse Length (Long or Short) */

} /* loop() */




























