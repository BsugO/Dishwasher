
//Peter Nikolov's Dishwasher v.1.1.
//Programer: Peter Nikolov.
//Based on Rivera_1.0 by Pedro Rivera and the work of steve8428 - Thank you.
//A dishwasher controller with different wash programs defined via a matrix of constants,
//also includes a Custom programme - allows user selection of custom temperature and wash time, and constructs a washing cycle to fulfill the custom parameters.
//Using Dallas OneWire thermometer and rotary encoder for program selector, 6-7 relays control, opto-isolator adviceable.
//Licensed under Creative Commons.

//#include <LiquidCrystal.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <math.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//Pin assignment
#define InletValve 4 //water inlet valve
#define Heater 5 //Heater 
#define WashPump 6 //WashPump
#define DrainPump 7 //drain pump
#define DetergentSolenoid 8 //detergent dispenser solenoid 
#define RinseAidSolenoid 9 //rinse aid dispenser solenoid
#define RegenerationSolenoid A7 //regeneration solenoid 
#define WaterLevelSensor 11 //fill sensor level 1
// #define WaterLevelSensor2 12 //fill sensor level 2
// #define ErrorSensor 13 //overfill sensor error
#define DoorSwitch 12 //door switch
#define ONE_WIRE_BUS 10 // OneWire interface (for thermometer)
#define Buzzer A3 //Buzzer/beeper 
#define RotaryEncoderPinB A2 // Rotary Encoder CLK
#define RotaryEncoderPinA A1 // Rotary Encoder DT
#define StartButton A0 //start/PausedState/resume/reset button

rgb_lcd lcd;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//constants declaration
const String SoftwareVersion = "1.1.1";
const int NumberOfPrograms = 9;
const String ProgramNames[] = { "Restart", "Lavagem Intensiva", "Lavagem Normal", "Lavagem Eco ", "Lavagem Rapida", "Lavagagem Forte ", "Lavagem a Frio", "Enxaguamento", "Personalizada" }; // Names of programs
const byte HighestTemperatures[] = { 0, 65, 60, 48, 36, 60, 5, 20, 65 }; // Highest allowed temperatures for programs in degrees Celsius
const byte ExpectedDurations[] = { 0, 120, 100, 80, 40, 50, 55, 20, 120 }; // Expected durations of programs in minutes
const byte MatrixStructure[] = { 0, 1, 1, 0, 1, 1, 1, 1, 0 }; // 1 = matrix structure; 0 = custom program
const byte PreWashDurations[] = { 0, 12, 2, 8, 0, 0, 0, 0, 20 }; // Pre-wash durations per programs in minutes; 0 = no Pre-wash
const byte WashDurations[] = { 0, 25, 25, 11, 10, 20, 25, 0, 50 }; // Wash durations per programs in minutes; 0 = no Wash
const byte Rinse1Durations[] = { 0, 6, 6, 6, 6, 6, 6, 10, 14 }; // Rinse 1 durations per programs in minutes; 0 = no Rinse 1
const byte Rinse2Durations[] = { 0, 6, 6, 2, 2, 2, 6, 0, 11 }; // Rinse 2 durations per programs in minutes; 0 = no Rinse 2
const byte ClearRinseDurations[] = { 0, 10, 10, 10, 10, 10, 10, 0, 20 }; // Clear Rinse durations per programs in minutes; 0 = no Clear Rinse
const byte DryDurations[] = { 0, 11, 11, 11, 2, 2, 1, 1, 12 }; // Drying durations per programs in minutes; 0 = no Drying
const int ButtonDebounceDelay = 10; // delay to count as button pressed - 15 milliseconds
const int ResetButtonDebounceDelay = 5000; // delay to count as reset - 5 seconds
const int OverheatLimit = 72; // Temperature limit to count for overheating

//variables declaration
volatile int DoorSwitchState;
volatile byte PausedState = false;
volatile unsigned long StopTime = 0;       // Record time program was paused
volatile unsigned long PausedDuration = 0; // Record how long the program was paused for
volatile byte PrePausedStateHeater;
volatile byte PrePausedStateWashPump;
volatile byte PrePausedStateDetergentSolenoid;
volatile byte PrePausedStateDrainPump;
volatile byte PrePausedStateInletValve;
volatile byte PrePausedStateRinseAidSolenoid;
volatile byte PrePausedStateRegenerationSolenoid;
volatile int FaultCode = 0;
unsigned long TotalPeriodStart, TotalPeriodDuration;
unsigned long CurrentPeriodStart, CurrentPeriodDuration;
unsigned long CurrentFillStart = 0, TotalFillDuration = 0;
int ExpectedDuration = 0; //Expected program duration in minutes
int MenuSelectorValue = 0;
int SelectorDialValue = 0;
unsigned long CustomTemperature = 0;
unsigned long CustomDuration = 0;
double CustomCoefficient;
int WaterLevelSensorState = 0;
int TemperatureLimit = 0;
// int RawADC = 240; //temporary temperature around 0 degrees Celsius - to be removed
String ProgramName = "                ";
String SubCycleName = "                ";
//-----On/PausedState/restart/reset Button reference ------
int RotaryEncoderPosition = 0;
int RotaryEncoderPinALast = LOW;
int StartButtonState = HIGH;
int StartButtonRecentState = HIGH;
int StartButtonLastState = HIGH;
unsigned long LastButtonDebounceTime = 0;
int StartButtonCode = 0;  //switch case statement to control what each button push does


//General actions


void TotalTimeElapsedFunction() {  // Total time counter with countdown time display on screen
    TotalPeriodDuration = (millis() - TotalPeriodStart - PausedDuration);
    lcd.setCursor(13, 0);
    lcd.print(ExpectedDuration - (TotalPeriodDuration / 60000));
    lcd.print("' ");
}

void TimeElapsedFunction() { // Time counter
    CurrentPeriodDuration = (millis() - CurrentPeriodStart);
    TotalTimeElapsedFunction();
    StartButtonFunction();
}

void WaitXmsecFunction(unsigned long WaitDuration) { // Wait X milliseconds
    unsigned long waitStart = millis();
    while ((millis() - waitStart) < WaitDuration) {
        TimeElapsedFunction();
        TotalTimeElapsedFunction();
        StartButtonFunction();
    }
}

void Wait3secFunction() {  // Wait 3 seconds
    WaitXmsecFunction(3000);
}

double WaterTemperatureFunction() { // Measure water temperature
    double Temp;
    sensors.requestTemperatures();
    Temp = sensors.getTempCByIndex(0);
    return Temp;
}


void DisplayTemperatureFunction() { // Display current temperature on screen
    lcd.setCursor(12, 1);
    lcd.print(int(WaterTemperatureFunction()));
    lcd.print((char)223);
    lcd.print("C ");
}


unsigned int EncoderSelectFunction(int StartPosition, int EndPosition) { // Read rotary encoder
    int RotaryEncoderPinAState = digitalRead(RotaryEncoderPinA);
    if ((RotaryEncoderPinALast == LOW) && (RotaryEncoderPinAState == HIGH)) {
        if (digitalRead(RotaryEncoderPinB) == LOW) {
            RotaryEncoderPosition--;
        }
        else {
            RotaryEncoderPosition++;
        }
        tone(Buzzer, 600, 7); // Buzz for 7 milliseconds with frequency 600 Hz
    }
    RotaryEncoderPinALast = RotaryEncoderPinAState;
    if (RotaryEncoderPosition < StartPosition) RotaryEncoderPosition = StartPosition;
    if (RotaryEncoderPosition > EndPosition) RotaryEncoderPosition = EndPosition;
    return RotaryEncoderPosition;
}


int StartButtonFunction() { // Start/Pause/Resume/Reset Button
    if (digitalRead(DoorSwitch) == HIGH) DoorSwitchFunction();
    do { // Start loop to check for "PausedStated" state
        StartButtonState = digitalRead(StartButton);
        if (StartButtonState != StartButtonLastState) { //meaning button changed state
            LastButtonDebounceTime = millis();
        }
        if ((StartButtonState == LOW) && ((millis() - LastButtonDebounceTime) >= ResetButtonDebounceDelay)) { // button held down long enough to count as Reset
            StartButtonLastState = StartButtonState;
            StartButtonCode = 4;
            ResetFunction(); // Reset the machine.
            StartButtonCode = 0;
            return StartButtonCode;
        }
        if ((millis() - LastButtonDebounceTime) >= ButtonDebounceDelay) { // button held down long enough to count as normal push
            if (StartButtonState != StartButtonRecentState) {
                StartButtonRecentState = StartButtonState;
                if (StartButtonRecentState == HIGH) { //If button has been pushed for long enough time, and then released, increment StartButtonCode by 1
                    StartButtonCode++;
                    tone(Buzzer, 700, 100); // Buzz for 100 milliseconds with frequency 700 Hz
                }
            }
        }
        StartButtonLastState = StartButtonState;
        switch (StartButtonCode) { //At startup StartButtonCode will == 0
        case 1:              //Button pushed so StartButtonCode == 1, start wash cycle, or continue unchanged if already StartButtonCode == 1
            return StartButtonCode;
            break;
        case 2:             //Button pushed again during wash so call PausedState function
            PausedStateFunction();
            break;
        case 3:             // button pushed for 3rd time so call restartFun but take 1 down from StartButtonCode
            ResumeFunction();
            StartButtonCode = 1;
            break;
        case 4:             // button pushed for so long as to count as reset
            ResetFunction();
            StartButtonCode = 0;
        }
    } while (StartButtonCode == 2);
    return StartButtonCode;
}



// Program flow actions



void (*InitiateResetFunction) (void) = 0; // Declaration of reset function


void FinishProgrammeFunction() { // Washing programme finished
    Serial.println(F("Fim Iniciado.")); //temp
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Loiça esta Limpa."));
    lcd.setCursor(0, 1);
    lcd.print(F("Desligue 'Off' btn."));
    tone(Buzzer, 700, 150); // Buzz for 150 milliseconds with frequency 700 Hz
    delay(400);
    tone(Buzzer, 700, 150); // Buzz for 150 milliseconds with frequency 700 Hz
    delay(400);
    tone(Buzzer, 700, 250); // Buzz for 250 milliseconds with frequency 700 Hz
    Serial.print(F("Duracao actal do programa: ")); //temp
    Serial.print(TotalPeriodDuration / 60000);
    Serial.println(F(" minutes.")); //temp
    while (true) {
        // StartButtonFunction();
    };
    Serial.println(F("Fim.")); //temp
}


void DoorSwitchFunction() { // Door swich action
    Serial.println(F("Sensor da porta activado.")); //temp
    PausedStateFunction();
    Serial.println(F("Lavagem em pausa - Porta Aberta.")); //temp
    DoorSwitchState = digitalRead(DoorSwitch);
    while (DoorSwitchState == HIGH && PausedState == true) {
        DoorSwitchState = digitalRead(DoorSwitch);
    }
    Serial.println(F("Continuacao .....")); //temp
    ResumeFunction();   //resume program
    Serial.println(F("Sensor da porta desactivado")); //temp
}

void StopAllFunction() {  // Stop all parts of the machine
    Serial.println(F("Ciclo Paragem iniciada.")); //temp
    digitalWrite(InletValve, LOW);
    digitalWrite(Heater, LOW);
    digitalWrite(WashPump, LOW);
    digitalWrite(DrainPump, LOW);
    digitalWrite(DetergentSolenoid, LOW);
    digitalWrite(RinseAidSolenoid, LOW);
    digitalWrite(RegenerationSolenoid, LOW);
    digitalWrite(Buzzer, LOW);
    Serial.println(F("Ciclo Paragem Acabada.")); //temp
}

void PausedStateFunction() { // PausedState - stop all wash processes and raise PausedState flag
  //Serial.println("PausedStateFun started."); //temp
    if (PausedState == false) {
        tone(Buzzer, 600, 500); // Buzz for 500 milliseconds with frequency 700 Hz
        PausedState = true;
        Serial.println(F("Pausa... ")); //temp
        StopTime = millis();
        PrePausedStateHeater = digitalRead(Heater);
        PrePausedStateWashPump = digitalRead(WashPump);
        PrePausedStateDetergentSolenoid = digitalRead(DetergentSolenoid);
        PrePausedStateDrainPump = digitalRead(DrainPump);
        PrePausedStateInletValve = digitalRead(InletValve);
        PrePausedStateRinseAidSolenoid = digitalRead(RinseAidSolenoid);
        PrePausedStateRegenerationSolenoid = digitalRead(RegenerationSolenoid);
        StopAllFunction();
        lcd.setCursor(0, 1);
        lcd.print(F("Pausa... "));
    }
    // Serial.println("PausedStateFun ended."); //temp
}

void ResumeFunction() { // Resume wash processes after PausedState
    Serial.println(F("Resuno de Fun iniciada.")); //temp
    lcd.setCursor(0, 1);
    lcd.print(F("Resumindo...     "));
    Serial.println(F("Resuno de pausa state.")); //temp
    tone(Buzzer, 800, 150); // Buzz for 150 milliseconds with frequency 700 Hz
    delay(400);
    tone(Buzzer, 800, 300); // Buzz for 300 milliseconds with frequency 700 Hz
    PausedState = false;
    PausedDuration = millis() - StopTime;
    digitalWrite(Heater, PrePausedStateHeater);
    digitalWrite(WashPump, PrePausedStateWashPump);
    digitalWrite(DetergentSolenoid, PrePausedStateDetergentSolenoid);
    digitalWrite(DrainPump, PrePausedStateDrainPump);
    digitalWrite(InletValve, PrePausedStateInletValve);
    digitalWrite(RinseAidSolenoid, PrePausedStateRinseAidSolenoid);
    digitalWrite(RegenerationSolenoid, PrePausedStateRegenerationSolenoid);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(ProgramName);
    lcd.setCursor(0, 1);
    lcd.print(SubCycleName);
    Serial.println(F("resumeFun ended.")); //temp
}


void ResetFunction() { // Reset wash, drain washer and restart machine
    Serial.println(F("resetFun started.")); //temp
    lcd.clear();
    lcd.home(); // go home
    lcd.print(F("Machine reset...     "));
    tone(Buzzer, 700, 1000); // Buzz for 1000 milliseconds with frequency 700 Hz
    StopAllFunction();  // Stop all devices
    delay(2000); //Wait for 2 seconds
    DrainAction(); // Drain
    PausedState = false;
    StopTime = 0;
    PausedDuration = 0;
    delay(2000); //Wait for 2 seconds
    // Restart machine
    Serial.println(F("Restarting machine...")); //temp
    InitiateResetFunction(); // Restart machine
}


void ErrorSensororFunction() { // Error sensor - to be developed
    Serial.println(F("Erro de Sensor de temperatura")); //temp
    cli();
    StopAllFunction();
    lcd.setCursor(0, 0);
    lcd.print(F("ERROR - DISCONNECT"));
    lcd.setCursor(0, 1);
    lcd.print(F("THE DISHWASHER FROM SOCKET"));
    while (true) {};
    Serial.println(F("ErrorSensororFunction ended.")); //temp
}



void ErrorFunction() { // Error encountered - stop all wash processes and show fault code
    Serial.println(F("ErrorFunction started.")); //temp
    tone(Buzzer, 600, 3000); // Buzz for 3 seconds with frequency 600 Hz
    StopAllFunction(); // Stop all wash processes
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("ERROR Fault:    "));
    lcd.setCursor(12, 0);
    lcd.print(FaultCode);
    lcd.setCursor(0, 1);
    lcd.print(F("Machine halted.  "));
    String faultDescription;
    switch (FaultCode) {
    case 20:
        faultDescription = F("20: Over-heating - temperature exceeds ");
        faultDescription = faultDescription + OverheatLimit;
        faultDescription = faultDescription + F(" degrees Celsius. All wash processes stopped.");
        break;
    case 21:
        faultDescription = F("21: Water temperature not reached after 25 minutes of heating - possible heater or sensor malfunction. Water drained. All wash processes stopped.");
        break;
    case 30:
        faultDescription = F("30: Heater ON while wash pump OFF - possible wash pump malfunction or heater does not turn off. All wash processes stopped. There may be water in the machine.");
        break;
    case 40:
        faultDescription = F("40: Water level high after 3 minutes of draining - possible drain pump malfunction or water outlet blocked. All wash processes stopped. There may be water in the machine.");
        break;
    case 50:
        faultDescription = F("50: Total filling time since last drain exceeds 4 minutes - possible water leakage. Water drained. All wash processes stopped.");
        break;
    case 51:
        faultDescription = F("51: Water level low after 3 minutes of filling - check water supply, possible inlet valve malfunction, fill sensor malfunction or water leakage. Water drained. All wash processes stopped.");
        break;
    default:
        faultDescription = F(": Unspecified error.");
        faultDescription = FaultCode + faultDescription;
        break;
    }
    Serial.print(F("ERROR! Fault code = "));
    Serial.println(faultDescription);
    while (true) {};
    /* Fault codes general groups:
      ======= Fault Codes ====================
      10 - 19 - inlet valve faults;
      20 - 29 - Heater circuit faults;
      30 - 39 - wash pump circuit faults;
      40 - 49 - drain pump circuit faults;
      50 - 59 - water leakage faults;
      60 - 69 - water level sensor circuit faults;
      70 - 79 - temperature sensor circuit faults;
      80 - 89 - detergent solenoid circuit faults;
      90 - 99 - rinse aid solenoid circuit faults;
      100 - 109 - control electronics faults;
      110 - 119 - general faults.
      ========================================
    */
    Serial.println(F("ErrorFunction ended.")); //temp
}


//Washing actions


void DrainAction() { // Drain machine
    Serial.println(F("    DrainAction started.")); //temp
    TotalTimeElapsedFunction();
    lcd.setCursor(0, 1);
    lcd.print(F("Draining...     "));
    HeaterOFFAction();                      // Heater OFF
    digitalWrite(WashPump, LOW);         // Wash pump OFF
    Wait3secFunction();
    unsigned long DrainStartPausedDuration = PausedDuration;   // Save PausedState time at start of draining
    //WaterLevelSensorState = digitalRead(WaterLevelSensor); //temporarily removed - to be reused
    if (WaterLevelSensorState == HIGH) {         // Fill sensor HIGH - start draining
        CurrentPeriodStart = millis();
        TimeElapsedFunction();
        digitalWrite(DrainPump, HIGH);     // Drain pump ON
        //WaterLevelSensorState = digitalRead(WaterLevelSensor); //temporarily removed - to be reused
        while (WaterLevelSensorState == HIGH) {
            TimeElapsedFunction();
            if ((CurrentPeriodDuration - (PausedDuration - DrainStartPausedDuration)) >= 180000) {   // Check if draining continuing for more than 3 minutes
                digitalWrite(DrainPump, LOW);     // Drain pump OFF
                FaultCode = 40; // Fault code 40: Water level sensor does not go LOW after 3 minutes of draining - water outlet blocked or drain pump malfunction.
                ErrorFunction();
            }
            digitalWrite(DrainPump, HIGH);     // Drain pump ON
            // WaterLevelSensorState = digitalRead(WaterLevelSensor); // temporarily removed - to be reused
            WaitXmsecFunction(5000); //temp - to be removed
            WaterLevelSensorState = LOW; //temp - to be removed
            StartButtonFunction();
        }
    }
    digitalWrite(DrainPump, HIGH);      // Drain pump ON
    WaitXmsecFunction(5000);                   // Wait 5 sec
    digitalWrite(DrainPump, LOW);       // Drain pump OFF
    TotalFillDuration = 0;  // Initialize fill timer
    lcd.setCursor(0, 1);
    lcd.print(F("            "));
    Serial.println(F("    DrainAction ended.")); //temp
}


void FillAction() { // Fill water
    Serial.println(F("    FillAction started.")); //temp
    int HeaterStatus = digitalRead(Heater); // Save Heater state
    int WashPumpStatus = digitalRead(WashPump); // Save WashPump state
    unsigned long FillStartPausedDuration = PausedDuration;   // Save PausedState time at start of filling
    TotalTimeElapsedFunction();
    // WaterLevelSensorState = digitalRead(WaterLevelSensor); //temporarily removed - to be reused
    // WaterLevelSensorState = 0; //temp - to be removed
    if (WaterLevelSensorState == 0) {          // Fill sensor LOW - start filling
        lcd.setCursor(0, 1);
        lcd.print(F("Filling...  "));
        DisplayTemperatureFunction();
        StartButtonFunction();
        HeaterOFFAction();                    // Heater OFF
        CurrentPeriodStart = millis();
        TimeElapsedFunction();
        CurrentFillStart = millis();
        digitalWrite(InletValve, HIGH);    // Inlet valve ON
        WaitXmsecFunction(7000);                  // Wait 7 sec
        digitalWrite(WashPump, HIGH);      // Wash pump ON
        digitalWrite(InletValve, HIGH);    // Inlet valve ON
        //WaterLevelSensorState = digitalRead(WaterLevelSensor); //temporarily removed - to be reused
        while (WaterLevelSensorState == LOW) {
            TimeElapsedFunction();
            StartButtonFunction();
            if (TotalFillDuration + ((millis() - CurrentFillStart - (PausedDuration - FillStartPausedDuration))) >= 240000) {  // Check if total fill time exceeds 4 minutes
                HeaterOFFAction(); // Heater OFF
                digitalWrite(WashPump, LOW);   // Wash pump OFF
                digitalWrite(InletValve, LOW); // Inlet valve OFF
                FaultCode = 50; // Fault code 50: Total filling time since last drain exceeds 4 minutes - water leakage.
                DrainAction();
                ErrorFunction();
            }
            if ((CurrentPeriodDuration - (PausedDuration - FillStartPausedDuration)) >= 180000) {  // Check if filling continuing for more than 3 minutes
                HeaterOFFAction(); // Heater OFF
                digitalWrite(WashPump, LOW);   // Wash pump OFF
                digitalWrite(InletValve, LOW); // Inlet valve OFF
                FaultCode = 51; // Fault code 51: Water level sensor does not go HIGH after 3 minutes of filling - water leakage or inlet valve or sensor malfunction.
                DrainAction();
                ErrorFunction();
            }
            digitalWrite(WashPump, HIGH);    // Wash pump ON
            digitalWrite(InletValve, HIGH);  // Inlet valve ON
            // WaterLevelSensorState = digitalRead(WaterLevelSensor); // temporarily removed - to be reused
            WaitXmsecFunction(5000); //temp - to be removed
            WaterLevelSensorState = HIGH; //temp - to be removed
            StartButtonFunction();
            DisplayTemperatureFunction();
        }
        digitalWrite(WashPump, HIGH);      // Wash pump ON
        digitalWrite(InletValve, HIGH);    // Inlet valve ON
        WaitXmsecFunction(5000);                  // Wait 5 sec
        digitalWrite(InletValve, LOW);     // Inlet valve OFF
        TotalFillDuration += ((millis() - CurrentFillStart - (PausedDuration - FillStartPausedDuration))); // Increase total fill timer with current fill duration
        Serial.print(F("    Total fill time = "));  // temp - to be removed
        Serial.println(TotalFillDuration);  // temp - to be removed
        CurrentFillStart = 0;
        //    RawADC = 340; //temporary temperature - around 9.7 degrees C - to be removed
        digitalWrite(WashPump, WashPumpStatus);  // Restore wash pump state
        if (HeaterStatus == HIGH) {
            HeaterONAction(); // Restore Heater state
        }
        else {
            HeaterOFFAction();
        }
        lcd.setCursor(0, 1);
        lcd.print(SubCycleName);
    }
    DisplayTemperatureFunction();
    StartButtonFunction();
    // LastFillTime = millis();           // Reset fill timer
    Serial.println("    FillAction ended.");  //temp
}

void CheckFillLevelAction() { // Check current water level
  // WaterLevelSensorState = digitalRead(WaterLevelSensor); //temporarily removed - to be reused
  // WaterLevelSensorState = HIGH; //temp - to be removed
    if (WaterLevelSensorState == LOW) {  // Fill sensor LOW
        Serial.println(F("    Water level low during wash - refilling.")); //temp
        FillAction();     // Refill
    }
    TimeElapsedFunction();
    DisplayTemperatureFunction();
    StartButtonFunction();
}


void HeaterONAction() { // Turn Heater on
    CheckFillLevelAction();
    StartButtonFunction();
    DisplayTemperatureFunction();
    if (digitalRead(Heater) == LOW) {
        digitalWrite(Heater, HIGH);
        Serial.println(F("     HeaterONAction executed.")); //temp
    }
}


void HeaterOFFAction() { // Turn Heater off
    StartButtonFunction();
    DisplayTemperatureFunction();
    if (digitalRead(Heater) == HIGH) {
        digitalWrite(Heater, LOW);
        Serial.println(F("     HeaterOFFAction executed.")); //temp
    }
}


void CheckForHeatAlarmsAction(int TargetTemperature, unsigned long MaxDuration) {  // Check for heat alarms
    StartButtonFunction();
    DisplayTemperatureFunction();
    CheckFillLevelAction();
    if (WaterTemperatureFunction() >= OverheatLimit) { // Over heating
        HeaterOFFAction(); // Heater OFF
        FaultCode = 20; // Foult code 20: Over-heating. Heater does not turn off.
        ErrorFunction();
    }
    if (digitalRead(Heater) == HIGH && digitalRead(WashPump) == LOW) {  // Heater ON while pump OFF
        HeaterOFFAction(); // Heater OFF
        FaultCode = 30; // Foult code 30: Heater ON while wash pump OFF - wash pump does not turn on or Heater does not turn off.
        ErrorFunction();
    }
    if (MaxDuration > 0) {
        if ((CurrentPeriodDuration >= (MaxDuration)) && (WaterTemperatureFunction() < TargetTemperature)) {
            HeaterOFFAction(); // Heater OFF
            FaultCode = 21; // Water temperature not reached after MaxDuration (25 minutes) of heating. Heater or sensor malfunction.
            DrainAction();
            ErrorFunction();
        }
    }
    // Code for checking of other Heater failure may be introduced here
    StartButtonFunction();
    DisplayTemperatureFunction();
}

void KeepTempAction(unsigned int KeepTemperature, unsigned int KeepDuration, byte Detergent) { // Reach and maintain given temperature for given time, with or without detergent dispensing
    Serial.print(F("    KeepTempAction started at ")); //temp
    Serial.print(KeepTemperature); //temp
    Serial.print(F(" degrees, for ")); //temp
    Serial.print(KeepDuration); //temp
    Serial.print(F(" minutes, Detergent: ")); //temp
    Serial.println(Detergent); //temp
    DisplayTemperatureFunction();
    TotalTimeElapsedFunction();
    CheckFillLevelAction();
    digitalWrite(WashPump, HIGH); // Wash pump ON
    CurrentPeriodStart = millis(); // Starting heating water to reach desired temperature
    TimeElapsedFunction();
    while ((CurrentPeriodDuration < 1500000) && (WaterTemperatureFunction() < (KeepTemperature)) && digitalRead(WashPump) == HIGH) { // Absolute maximum duration of heat rising is 25 minutes
        HeaterONAction(); // Heater ON
        CheckForHeatAlarmsAction(KeepTemperature, 1500000); // check for heat alarms
        CheckFillLevelAction(); // check water fill level
        DisplayTemperatureFunction();
        TimeElapsedFunction();
        StartButtonFunction();
    }  // Temperature reached
    CheckForHeatAlarmsAction(KeepTemperature, 1500000);  //check also for Heater failure
    Serial.println(F("    Requested temperature reached, starting temperature maintenance for requested time.")); //temp
    CheckFillLevelAction(); // check water fill level
    if (Detergent == true) {  // After temperature is reached, dispense detergent if requested
        digitalWrite(DetergentSolenoid, HIGH); // Dispense detergent if requested
        Serial.println(F("    DetergentSolenoid put to ON.")); //temp
        WaitXmsecFunction(3000);
        digitalWrite(DetergentSolenoid, LOW);
        Serial.println(F("    DetergentSolenoid put to OFF.")); //temp
    }
    CheckFillLevelAction(); // check water fill level
    CurrentPeriodStart = millis(); // Starting temperature maintenance
    TimeElapsedFunction();
    while (CurrentPeriodDuration < (KeepDuration * 60000)) {
        CheckForHeatAlarmsAction(KeepTemperature, (KeepDuration + 5) * 60000); // check for heat alarms - parameters meant to not check for Heater failure
        if ((WaterTemperatureFunction() < (KeepTemperature - 2)) && digitalRead(WashPump) == HIGH) {
            HeaterONAction(); // Heater ON
        }
        if (WaterTemperatureFunction() > (KeepTemperature + 2)) {
            HeaterOFFAction(); // Heater OFF
        }
        CheckFillLevelAction(); // check water fill level
        DisplayTemperatureFunction();
        TimeElapsedFunction();
        StartButtonFunction();
        // Serial.print("Maintaining temperature - current: " ); //temp
        // Serial.println(WaterTemperatureFunction()); //temp
    }
    HeaterOFFAction(); // Heater OFF
    digitalWrite(WashPump, LOW); // Wash pump OFF
    CheckFillLevelAction(); // check water fill level
    DisplayTemperatureFunction();
    TimeElapsedFunction();
    Wait3secFunction();
    Serial.println("    KeepTempAction ended."); //temp
}


//Washing sub-cycles


void WashSubcycle(unsigned int WashCycleTemperature, unsigned int WashCycleDuration, byte Detergent, byte RinseAid, String SubCycleName1) { // Basic wash cycle engine
    Serial.print(F("   WashSubcycle '"));
    Serial.print(SubCycleName1);
    Serial.print(F("' started at ")); //temp
    Serial.print(WashCycleTemperature); //temp
    Serial.print(F(" degrees C, for ")); //temp
    Serial.print(WashCycleDuration); //temp
    Serial.print(F(" minutes, Detergent: ")); //temp
    Serial.print(Detergent); //temp
    Serial.print(F(" , RinseAid: ")); //temp
    Serial.println(RinseAid); //temp
    SubCycleName = SubCycleName1;
    DisplayTemperatureFunction();
    TotalTimeElapsedFunction();
    FillAction();                    // Fill
    lcd.setCursor(0, 1);
    lcd.print(SubCycleName);
    Wait3secFunction();
    digitalWrite(WashPump, HIGH); // Wash pump ON
    TimeElapsedFunction();
    CheckFillLevelAction();
    DisplayTemperatureFunction();
    StartButtonFunction();
    KeepTempAction(WashCycleTemperature, WashCycleDuration, Detergent); // Reach and maintain temperature for period
    HeaterOFFAction(); // Heater OFF
    CheckFillLevelAction();
    Wait3secFunction();
    if (RinseAid == true) { // Perform rinse aid finish if requested
        digitalWrite(WashPump, HIGH);  // Wash pump ON
        if ((WaterTemperatureFunction() < WashCycleTemperature) && digitalRead(WashPump) == HIGH) {
            HeaterONAction();               // Heater ON if applicable
        }
        digitalWrite(RinseAidSolenoid, HIGH); // Rinse Aid solenoid ON
        WaitXmsecFunction(60000);             // Wait 1 minute
        digitalWrite(RinseAidSolenoid, LOW);  // Rinse Aid solenoid OFF
        WaitXmsecFunction(3000);              // Wait 3 seconds
        digitalWrite(RinseAidSolenoid, HIGH); // Rinse Aid solenoid ON
        WaitXmsecFunction(60000);             // Wait 1 minute
        digitalWrite(RinseAidSolenoid, LOW);  // Rinse Aid solenoid OFF
        KeepTempAction(WashCycleTemperature, 1, 0);  // Wash at desired temperature for 1 minute
        digitalWrite(WashPump, HIGH);  // Wash pump ON
        HeaterOFFAction();                // Heater OFF
        WaitXmsecFunction(60000);             // Wait 1 minute
    }
    digitalWrite(WashPump, LOW); // Wash pump OFF
    Wait3secFunction();
    DrainAction();  // Drain
    TimeElapsedFunction();
    DisplayTemperatureFunction();
    StartButtonFunction();
    Serial.println(F("   WashSubcycle ended.")); //temp
}


void DrySubcycle(byte DryDur) { // Dry sub-cycle
    Serial.println(F("   DrySubcycle started.")); //temp
    TotalTimeElapsedFunction();
    lcd.setCursor(0, 1);
    SubCycleName = "Drying... ";
    lcd.setCursor(0, 1);
    lcd.print(SubCycleName);
    WaitXmsecFunction(120000);              // Wait 2 minutes
    //digitalWrite(RegenerationSolenoid, HIGH); // Regeneration solenoid ON
    DrainAction();                   // Drain
    lcd.setCursor(0, 1);
    lcd.print(SubCycleName);
    WaitXmsecFunction(60000);               // Wait 1 minute
    digitalWrite(InletValve, HIGH);  // Inlet valve ON
    WaitXmsecFunction(1000);                // Wait 1 second
    digitalWrite(InletValve, LOW);   // Inlet valve OFF
    WaitXmsecFunction(3000);                // Wait 3 seconds
    digitalWrite(InletValve, HIGH);  // Inlet valve ON
    WaitXmsecFunction(1000);                // Wait 1 second
    digitalWrite(InletValve, LOW);   // Inlet valve OFF
    //digitalWrite(RegenerationSolenoid, LOW); // Regeneration solenoid OFF
    DrainAction();                   // Drain
    lcd.setCursor(0, 1);
    lcd.print(SubCycleName);
    WaitXmsecFunction(DryDur * 60000);      // Wait arbitrary time according to parameter
    DrainAction();                   // Drain
    Serial.println(F("   DrySubcycle ended.")); //temp
}


//Washing cycles


void MatrixCycle(String ProgramName, byte PreWashDuration, byte WashDuration, byte Rinse1Duration, byte Rinse2Duration, byte ClearRinseDuration, byte DryDuration, byte ExpectedDuration, byte TemperatureLimit) { // Matrix cycle engine
    Serial.println(F("  MatrixCycle started.")); //temp
    Serial.print(F("  Program: '")); //temp
    Serial.print(ProgramName); //temp
    Serial.print(F("', temperature: ")); //temp
    Serial.print(TemperatureLimit); //temp
    Serial.print(F(" degrees C, exp. duration: ")); //temp
    Serial.print(ExpectedDuration); //temp
    Serial.print(F(", Prewash dur.: ")); //temp
    Serial.print(PreWashDuration); //temp
    Serial.print(F(", Wash dur.: ")); //temp
    Serial.print(WashDuration); //temp
    Serial.print(F(", Rinse1 dur.: ")); //temp
    Serial.print(Rinse1Duration); //temp
    Serial.print(F(", Rinse2 dur.: ")); //temp
    Serial.print(Rinse2Duration); //temp
    Serial.print(F(", ClearRinse dur.: ")); //temp
    Serial.print(ClearRinseDuration); //temp
    Serial.print(F(", Dry dur.: ")); //temp
    Serial.println(DryDuration); //temp
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(ProgramName);
    TimeElapsedFunction();
    DrainAction();  // Initial drain
    Wait3secFunction();
    DisplayTemperatureFunction();
    TimeElapsedFunction();
    if (PreWashDuration > 0) {
        WashSubcycle(min(36, TemperatureLimit), PreWashDuration, 0, 0, "Prewash...    ");  // Prewash without detergent
    };
    Wait3secFunction();
    DisplayTemperatureFunction();
    TimeElapsedFunction();
    Wait3secFunction();
    if (WashDuration > 0) {
        WashSubcycle(TemperatureLimit, WashDuration, 1, 0, "Washing...    ");  // Wash with detergent
    };
    Wait3secFunction();
    if (Rinse1Duration > 0) {
        WashSubcycle(min(15, TemperatureLimit), Rinse1Duration, 0, 0, "Rinsing...    ");  // Rinse 1
    };
    Wait3secFunction();
    TimeElapsedFunction();
    if (Rinse2Duration > 0) {
        WashSubcycle(min(15, TemperatureLimit), Rinse2Duration, 0, 0, "Rinsing...    ");  // Rinse 2
    };
    Wait3secFunction();
    TimeElapsedFunction();
    if (ClearRinseDuration > 0) {
        WashSubcycle(min(48, TemperatureLimit), ClearRinseDuration, 0, 1, "Clear Rinse...    ");  // Clear Rinse with Rinse Aid
    };
    Wait3secFunction();
    if (DryDuration > 0) {
        DrySubcycle(DryDuration);  // Dry
    }
    Serial.println(F("  MatrixCycle ended.")); //temp
}


//Washing programmes


void MatrixProgramme(byte ProgramIndex) {  // Matrix programme engine
    Serial.println(F(" MatrixProgramme started.")); //temp
    ProgramName = ProgramNames[ProgramIndex]; // Program name
    byte PreWashDuration = PreWashDurations[ProgramIndex]; // Pre-wash duration
    byte WashDuration = WashDurations[ProgramIndex]; // Wash duration
    byte Rinse1Duration = Rinse1Durations[ProgramIndex]; // Rinse 1 duration
    byte Rinse2Duration = Rinse2Durations[ProgramIndex]; // Rinse 2 duration
    byte ClearRinseDuration = ClearRinseDurations[ProgramIndex]; // Clear Rinse duration
    byte DryDuration = DryDurations[ProgramIndex]; // Drying duration
    ExpectedDuration = ExpectedDurations[ProgramIndex]; // Expected duration
    TemperatureLimit = HighestTemperatures[ProgramIndex]; // Max temperature
    if (MatrixStructure[ProgramIndex] == 0) { // Program is not matrix - exiting procedure
        return;
    }
    MatrixCycle(ProgramName, PreWashDuration, WashDuration, Rinse1Duration, Rinse2Duration, ClearRinseDuration, DryDuration, ExpectedDuration, TemperatureLimit);
    FinishProgrammeFunction();
    Serial.println(F(" MatrixProgramme ended.")); //temp
}


void EconomyProgramme() {  // Eco programme
    Serial.println(F(" EconomyProgramme started.")); //temp
    ExpectedDuration = ExpectedDurations[3]; //Expected duration for Eco Wash
    TemperatureLimit = HighestTemperatures[3]; // Max temperature for Eco Wash
    ProgramName = ProgramNames[3];
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(ProgramName); // name of Eco Wash program
    SubCycleName = "Washing... ";
    lcd.print(SubCycleName);
    DrainAction(); // Initial drain
    Wait3secFunction();
    WashSubcycle(36, 8, 0, 0, "Prewash...    ");  // Prewash without detergent
    Wait3secFunction();
    FillAction();  // Fill
    Wait3secFunction();
    CheckFillLevelAction();
    KeepTempAction(41, 10, 1);  // Wash at 41 degrees for 10 minutes with detergent
    KeepTempAction(TemperatureLimit, 1, 0); // Conitinue to wash at temperature TemperatureLimit for 1 minute without detergent
    Wait3secFunction();
    DrainAction();  // Drain
    Wait3secFunction();
    TimeElapsedFunction();
    WashSubcycle(15, 6, 0, 0, "Rinsing...     ");  // Request wash cycle without detergent
    TimeElapsedFunction();
    StartButtonFunction();
    Wait3secFunction();
    DisplayTemperatureFunction();
    WashSubcycle(48, 1, 0, 1, "Clear rinse...    ");  // Wash at 48 degrees Celsius for 1 minute and finish with Rinse Aid
    Wait3secFunction();
    TimeElapsedFunction();
    DrySubcycle(11); //  Dry
    FinishProgrammeFunction();
    Serial.println(F(" EconomyProgramme ended.")); //temp
}


void CustomProgramme() {  // Custom programme - allows user selection of custom temperature and wash time, and constructs a washing cycle to fulfill the custom parameters
    Serial.println(F(" CustomProgramme started.")); //temp
    ExpectedDuration = ExpectedDurations[8]; //Expected duration for Custom Wash
    TemperatureLimit = HighestTemperatures[8]; // Max temperature for Custom Wash
    ProgramName = ProgramNames[8];
    byte PreWashDuration = PreWashDurations[8];
    byte WashDuration = WashDurations[8];
    byte Rinse1Duration = Rinse1Durations[8];
    byte Rinse2Duration = Rinse2Durations[8];
    byte ClearRinseDuration = ClearRinseDurations[8];
    byte DryDuration = DryDurations[8];
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(ProgramName); // name of program
    lcd.setCursor(0, 1);
    lcd.print(F("Temperature:      "));
    StartButtonCode = 0; // initialize selector to select maximum temperature
    while (StartButtonFunction() != 1) {
        SelectorDialValue = EncoderSelectFunction(1, 13); // read custom temperature
        CustomTemperature = SelectorDialValue * 5;
        if (CustomTemperature < 5) CustomTemperature = 5;
        if (CustomTemperature > TemperatureLimit) CustomTemperature = TemperatureLimit;
        lcd.setCursor(12, 1);
        lcd.print(CustomTemperature);
        lcd.print((char)223);
        lcd.print("C   ");
    }
    lcd.setCursor(0, 1);
    lcd.print(F("Duration:          "));
    lcd.setCursor(10, 0);
    lcd.print("______  ");
    StartButtonCode = 0; // initialize selector to select maximum duration
    while (StartButtonFunction() != 1) {
        SelectorDialValue = EncoderSelectFunction(1, 24);  // read custom duration
        CustomDuration = SelectorDialValue * 5;
        if (CustomDuration < 10) CustomDuration = 10;
        if (CustomDuration > ExpectedDuration) CustomDuration = ExpectedDuration;
        lcd.setCursor(12, 1);
        lcd.print(CustomDuration);
        lcd.print("'   ");
        CustomCoefficient = (CustomDuration * 1.0) / (ExpectedDuration * 1.0);
        lcd.setCursor(10, 0);
        PreWashDuration = (PreWashDurations[8] * CustomCoefficient / 8.0); // Pre-Wash duration, divider 7.0 makes it less important
        PreWashDuration = PreWashDuration * 8;
        if (PreWashDuration > 0) lcd.print("P");
        else lcd.print("_");
        WashDuration = (WashDurations[8] * CustomCoefficient / 1.0); // Wash duration, divider 1.0 makes it very important
        WashDuration = WashDuration * 1;
        if (WashDuration > 0) lcd.print("W");
        else lcd.print("_");
        Rinse1Duration = (Rinse1Durations[8] * CustomCoefficient / 1.0); // Rinse 1 duration, divider 1.0 makes it very important
        Rinse1Duration = Rinse1Duration * 1;
        if (Rinse1Duration > 0) lcd.print("R");
        else lcd.print("_");
        Rinse2Duration = (Rinse2Durations[8] * CustomCoefficient / 4.0); // Rinse 2 duration, divider 4.0 makes it less important
        Rinse2Duration = Rinse2Duration * 4;
        if (Rinse2Duration > 0) lcd.print("R");
        else lcd.print("_");
        ClearRinseDuration = (ClearRinseDurations[8] * CustomCoefficient / 2.0); // Clear Rinse duration, divider 2.0 makes it more important
        ClearRinseDuration = ClearRinseDuration * 2;
        if (ClearRinseDuration > 0) lcd.print("C");
        else lcd.print("_");
        DryDuration = (DryDurations[8] * CustomCoefficient / 4.0); // Drying duration, divider 4.0 makes it less important
        DryDuration = DryDuration * 4;
        if (DryDuration > 0) lcd.print("D");
        else lcd.print("_");
        lcd.print("      ");
    }
    tone(Buzzer, 700, 300); // Buzz for 300 milliseconds with frequency 700 Hz - selected custom programme starting
    ExpectedDuration = CustomDuration;
    TotalPeriodStart = millis();
    StopTime = 0;
    PausedDuration = 0;
    PausedState = false;
    Serial.print(F(" Selected custom temperature: "));
    Serial.print(CustomTemperature);
    Serial.print(" degrees C, custom duration: ");
    Serial.print(CustomDuration);
    Serial.println(" minutes.");
    String ProgramName = "Custom            ";
    MatrixCycle(ProgramName, PreWashDuration, WashDuration, Rinse1Duration, Rinse2Duration, ClearRinseDuration, DryDuration, CustomDuration, CustomTemperature);
    FinishProgrammeFunction();
    Serial.println(F(" CustomProgramme ended.")); //temp
}

void setup() {
    lcd.begin(16, 2);
    lcd.clear();
    lcd.home(); // go home
    Serial.begin(9600); // opens serial port, sets data rate to 9600 bps
    Serial.println(F("setup started.")); //temp
    pinMode(Heater, OUTPUT);
    pinMode(WashPump, OUTPUT);
    pinMode(DetergentSolenoid, OUTPUT);
    pinMode(RegenerationSolenoid, OUTPUT);
    pinMode(RinseAidSolenoid, OUTPUT);
    pinMode(DrainPump, OUTPUT);
    pinMode(InletValve, OUTPUT);
    pinMode(Buzzer, OUTPUT);
    pinMode(RotaryEncoderPinA, INPUT); // Rotary Encoder DT
    pinMode(RotaryEncoderPinB, INPUT); // Rotary Encoder CLK
    pinMode(StartButton, INPUT_PULLUP);
    pinMode(DoorSwitch, INPUT_PULLUP);
    // pinMode(ErrorSensor, INPUT);
    pinMode(WaterLevelSensor, INPUT);
    digitalWrite(InletValve, LOW);
    digitalWrite(Heater, LOW);
    digitalWrite(WashPump, LOW);
    digitalWrite(DrainPump, LOW);
    digitalWrite(DetergentSolenoid, LOW);
    digitalWrite(RinseAidSolenoid, LOW);
    digitalWrite(RegenerationSolenoid, LOW);
    digitalWrite(WaterLevelSensor, LOW);
    digitalWrite(Buzzer, LOW);
    // attachInterrupt(digitalPinToInterrupt(DoorSwitch), actDoorSwitch, FALLING); // Not used at this time - door switch is currently checked via polling together with the On/Off button
    // attachInterrupt(digitalPinToInterrupt(3), ErrorSensororFunction, FALLING); // For future use
    WaterTemperatureFunction();
    Serial.println("setup ended."); //temp
}


void loop() {
    Serial.println(F("loop started.")); //temp
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Welcome! Arduino "));
    lcd.setCursor(0, 1);
    lcd.print(F("Dishwasher "));
    lcd.print(SoftwareVersion);
    delay(4000);
    lcd.clear();
    Serial.println(F("Starting program selector.")); //temp
    lcd.setCursor(0, 0);
    lcd.print(F("Select a program"));
    lcd.setCursor(0, 1);
    lcd.print(F("and press Start"));
    while (StartButtonFunction() != 1) {
        MenuSelectorValue = EncoderSelectFunction(0, NumberOfPrograms - 1); //read the value from the rotary encoder
        lcd.setCursor(0, 1);
        lcd.print(">");
        lcd.print(ProgramNames[MenuSelectorValue]);
        lcd.print(F("                  "));
    }
    TotalPeriodStart = millis();
    StopTime = 0;
    PausedDuration = 0;
    PausedState = false;
    StartButtonFunction();
    Serial.print(F("Programme '"));
    Serial.print(ProgramNames[MenuSelectorValue]);
    Serial.println(F("' selected.")); //temp
    delay(300);
    tone(Buzzer, 700, 300); // Buzz for 300 milliseconds with frequency 700 Hz - selected programme starting
    switch (MenuSelectorValue) {
    case 0:
        ResetFunction(); // Drain and restart machine
        break;
    case 1:
        MatrixProgramme(MenuSelectorValue);
        break;
    case 2:
        MatrixProgramme(MenuSelectorValue);
        break;
    case 3:
        EconomyProgramme(); // Eco program - not matrix
        break;
    case 4:
        MatrixProgramme(MenuSelectorValue);
        break;
    case 5:
        MatrixProgramme(MenuSelectorValue);
        break;
    case 6:
        MatrixProgramme(MenuSelectorValue);
        break;
    case 7:
        MatrixProgramme(MenuSelectorValue);
        break;
    case 8:
        CustomProgramme(); // Custom program - not matrix
        break;
    }
    Serial.println(F("loop ended.")); //temp
}