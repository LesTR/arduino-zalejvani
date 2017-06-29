#include <avr/sleep.h>
#include <avr/wdt.h>
#include <DS1302.h>

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif


volatile boolean _interrupt = true;

const bool DEBUG = true;
const int debugSleepCycluses = 5;

const int soilSensorAPin = A4;
const int soilSensorBPin = A3;
const int soilSensorCPin = A2;
const int soilSensorDPin = A1;
const int soilSensorEPin = A0;


const int soilSensorPins[] = { A4, A3, A2, A1, A0 };
const int numberOfSensors = (int)(sizeof(soilSensorPins)/sizeof(int));
const int wateringHours[] = {8,12,18};
const int numberOfWateringHours = (int)(sizeof(wateringHours)/sizeof(int));

const int measurementEnablePin = 9;
const int wateringLevel = 850;
const int minWateringVotesNeeded = (int) floor(numberOfSensors/2)+1;

boolean wateredInThisHour = false;
int nint = 0;
int sleepCycluses = 60;

const int kCePin   = 5;  // Chip Enable
const int kIoPin   = 6;  // Input/Output
const int kSclkPin = 7;  // Serial Clock

// Create a DS1302 object.
DS1302 rtc(kCePin, kIoPin, kSclkPin);


void setup() {
  Serial.begin(57600);
  if (DEBUG) {
    sleepCycluses = debugSleepCycluses;
    Serial.println("\n\n\n===============================================");
    Serial.println("System configuration: ");
    Serial.print("Sleep cycluses: ");
    Serial.println(sleepCycluses);
    Serial.print("Number of sensors: ");
    Serial.println(numberOfSensors);
    Serial.print("Watering level: ");
    Serial.println(wateringLevel);
    Serial.print("Minimal watering votes needed: ");
    Serial.println(minWateringVotesNeeded);
    Serial.print("Watering hours: ");
    for (int i = 0 ; i < numberOfWateringHours; i++){
      Serial.print(wateringHours[i]);
      Serial.print(" ");
    }
    Serial.println("\nData line header:");
    Serial.println("Date;SensorA;SensorB;SensorC;SensorD;SensorE;Votes;checkWatering;watering"); 
    Serial.println("===============================================");
  }
  pinMode(measurementEnablePin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  if (false) {
    rtc.writeProtect(false);
    rtc.halt(false);
  
    // Make a new time object to set the date and time.
    Time t(2017, 6, 29, 20, 42, 05, Time::kThursday);
  
    // Set the time and date on the chip.
    rtc.time(t);
    rtc.writeProtect(true);
  }
  // CPU Sleep Modes
  // SM2 SM1 SM0 Sleep Mode
  // 0    0  0 Idle
  // 0    0  1 ADC Noise Reduction
  // 0    1  0 Power-down
  // 0    1  1 Power-save
  // 1    0  0 Reserved
  // 1    0  1 Reserved
  // 1    1  0 Standby(1)

  cbi( SMCR,SE );      // sleep enable, power down mode
  cbi( SMCR,SM0 );     // power down mode
  sbi( SMCR,SM1 );     // power down mode
  cbi( SMCR,SM2 );     // power down mode
  if (DEBUG) {
    Serial.println("\n\nSetup DONE\n");
    delay(1000);
  }

  setup_watchdog(6);
}

void loop() {
  if (_interrupt == true) {  // wait for timed out watchdog / flag is set when a watchdog timeout occurs
    _interrupt = false;       // reset flag
    if (++nint >= sleepCycluses) {
      if (DEBUG) {
        Serial.println("LOOP");
      }
      digitalWrite(LED_BUILTIN, HIGH);
      digitalWrite(measurementEnablePin, HIGH); // enable soil sensor
      delay(500);
      nint = 0;
      Time t = rtc.time(); // Get time
      
      int soilSensorValues [numberOfSensors];
      bool wateringEnabled = false;
      int wateringVotes = 0;
      // Get value from soil sensor
      for (int i = 0; i < numberOfSensors; i++) {
        soilSensorValues[i] = analogRead(soilSensorPins[i]);
        if (soilSensorValues[i] > wateringLevel) {
          wateringVotes++;
        }
        if (DEBUG) {
          char buf[20];
          sprintf(buf,"Sensor %d: %d",i,soilSensorValues[i]);
          Serial.println(buf);
          delay(100);
        }
      }
      digitalWrite(measurementEnablePin, LOW); // disable soil sensor
      bool checkWatering = false;
      for (int i = 0 ; i < numberOfWateringHours; i++){
        if (t.hr == wateringHours[i] ) {
          if (wateredInThisHour == false) {
            checkWatering = true;
          }
          if (DEBUG) {
            Serial.print("Watering hour! Check: ");
            Serial.println(checkWatering);
          }
        }
      }
      if (wateringVotes >= minWateringVotesNeeded) {
        wateringEnabled = true;
      }
      // Print the formatted string to serial with time and values.
      char buf[50];
      sprintf(buf, "'%04d-%02d-%02d %02d:%02d:%02d';%d;%d;%d;%d;%d;%d;%d,%d",
             t.yr, t.mon, t.date,
             t.hr, t.min, t.sec,
             soilSensorValues[0],
             soilSensorValues[1],
             soilSensorValues[2],
             soilSensorValues[3],
             soilSensorValues[4],
             wateringVotes,
             checkWatering,
             wateringEnabled             
      );
      digitalWrite(LED_BUILTIN, LOW);
      if (DEBUG) {
        Serial.println("Data line: ");
      }
      Serial.println(buf);
      if (DEBUG) {
        Serial.println("END LOOP");
      }
      delay(600);               // wait until the last serial character is send
    }
  }
  system_sleep();
}



//****************************************************************
// set system into the sleep state
// system wakes up when wtchdog is timed out
void system_sleep() {

  cbi(ADCSRA,ADEN);                    // switch Analog to Digitalconverter OFF

  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
  sleep_enable();

  sleep_mode();                        // System sleeps here

  sleep_disable();                     // System continues execution here when watchdog timed out
  sbi(ADCSRA,ADEN);                    // switch Analog to Digitalconverter ON

}


//****************************************************************
// 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms
// 6=1 sec,7=2 sec, 8=4 sec, 9= 8sec
void setup_watchdog(int ii) {

  byte bb;
  int ww;
  if (ii > 9 ) ii=9;
  bb=ii & 7;
  if (ii > 7) bb|= (1<<5);
  bb|= (1<<WDCE);
  ww=bb;
  Serial.println(ww);


  MCUSR &= ~(1<<WDRF);
  // start timed sequence
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  // set new watchdog timeout value
  WDTCSR = bb;
  WDTCSR |= _BV(WDIE);


}
//****************************************************************
// Watchdog Interrupt Service / is executed when  watchdog timed out
ISR(WDT_vect) {
  _interrupt = true;  // set global flag
}
