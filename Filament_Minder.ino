/*  Title: Filament Minder
 *  Author: Sean The IT Guy (theitguysean@gmail.com)
 *  Date: 2019-04-14
 *  ---
 *  This is a simple sketch to keep track of SPOOLCOUNT number of filament spools' day0 starting weights,
 *  and calculate how much filament remains on the spool based upon that.  It is only as accurate as
 *  the assumption that the manufacturer has put the advertised amount on the spool.
 */

#include <HX711.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>

#define TITLE "Filament Minder"
#define VERSION "v0.2.2"
#define SPOOLCOUNT 8              // Number of spools we wish to store. Can be up to [ (EEPROM.length()/sizeof(int)) - sizeof(int) ]
#define NEXTPIN 4                 // pin that the "next spool" button is on
#define PREVPIN 5                 // pin that the "prev spool" button is on
#define RESETPIN 6                // pin that the "new spool" button is on
#define BUZZERPIN 13              // screecher pin
#define THRESHOLD 50              // minimum amount (g) of remaining filament before alert.
#define LEDPIN 16                 // pin to drive a error state LED (could be LED_INTERNAL, or hooked to discrete LED)
#define SPOOLSIZE 1000            // Assumption of 1KG spool.  This may be updated for options via the reset menu if warranted
#define TIMEBETWEENBEEPS 20000    // milliseconds between beep alerts
#define LOADCELL_DOUT_PIN 15      // DOUT pin for HX711 load cell ADC
#define LOADCELL_SCK_PIN 14       // SCK pin of HX711 load cell ADC
#define LOADCELL_OFFSET 50682624  // Define these after calibration.
#define LOADCELL_DIVIDER 5895655  // Define these after calibration.

int currentSpool;                 // global to store which spool we're currently using
int currentWeight;                // contains current spool's measured weight
int startWeight;                  // contains current spool's day0 weight, retrieved from EEPROM
int lastBeep = millis();          // keeps record of when a beep last occured to keep from overbeeping

// Set up the load cell.
HX711 scale;

// Set up the display
LiquidCrystal lcd(7, 8, 9, 10, 11 , 12);

void setup() { 
  // Configure the load cell
   scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
   scale.set_scale(LOADCELL_DIVIDER);
   scale.set_offset(LOADCELL_OFFSET);
  
  // Confgure the IO pins
  pinMode(BUZZERPIN, OUTPUT);
  pinMode(LEDPIN, OUTPUT);
  pinMode(RESETPIN, INPUT);
  pinMode(NEXTPIN, INPUT);
  pinMode(PREVPIN, INPUT);
  
  // Display the Version Data and static text.
  lcd.begin(16, 2);
  lcd.setCursor(0,0);
  lcd.print(VERSION);
  delay(300);
  lcd.setCursor(0,0);
  lcd.print(TITLE);
  delay(300);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Active Spool:");
 
  // Determine which spool was last used and retrieve it's weight.
  EEPROM.get(sizeof(int)*SPOOLCOUNT, currentSpool);
  if (currentSpool < 0 || currentSpool >= SPOOLCOUNT) {   // if retrieved data not valid, init to 0
     currentSpool = 0;  
  }
  EEPROM.get(sizeof(int)*currentSpool, startWeight);      // if retrieved data not valid, init to 0
  if(startWeight < 0) {
    startWeight = 0;
  }
 
  // Execute a "ready" beep
  tone(BUZZERPIN,1000); 
  delay(100);
  noTone(BUZZERPIN);  
}

void loop() {   
  // Get the current weight from the loadcell (retry 100 times, then error out). get_units(X) averages X measurements.
  if (scale.wait_ready_retry(100)) {
    currentWeight = (int) scale.get_units(10);    // cast the long float response to an int for 1g granularity
    if (currentWeight < 0) {
      currentWeight = 0;                          // if measured data not valid, init to 0
    }
  } else {
      error("SCALE ERROR!!!!", 1000, true);       // if load cell ADC not ready after 100 retries, display error.
 }

  // Check to see if a spool button is pressed to switch between spools
  int buttons[] = {PREVPIN, NEXTPIN};
  for(int i = 0; i < SPOOLCOUNT; i++) {
    int buttonState = digitalRead(buttons[i]);
    if(buttonState == HIGH) {

      // Set button pressed flag high
      int pressed = 1;

      // Play tone, pitch derived from current spool id
      tone(BUZZERPIN,1000+250*currentSpool);
      delay(100);
      noTone(BUZZERPIN);

      // Update the current spool var, if outside bounds, do not change.
      if(buttons[i] == PREVPIN) {
        currentSpool = currentSpool - 1 < 0 ? 0 : currentSpool - 1;
      } else {
        currentSpool = currentSpool + 1 >= SPOOLCOUNT ? SPOOLCOUNT - 1 : currentSpool + 1;
      }
      
      // Retrieve the new spool's starting weight
      EEPROM.get(currentSpool*sizeof(int), startWeight);
      if(startWeight < 0) {
        startWeight = 0;
      }

      // Do not continue until button is released.
      while (pressed) {
        if(digitalRead(buttons[i]) == LOW) {
          pressed = 0;
        }         
      }
    }
  }

  // Check for the spool reset button
  int buttonState = digitalRead(RESETPIN);
  if(buttonState == HIGH) {

    // Set button flag high and record push start time
    int pressed = 1;
    int startPush = millis();

    // Play initial beep
    tone(BUZZERPIN,3000);
    delay(100);
    noTone(BUZZERPIN);

    // notify instructions for reset
    lcd.setCursor(0,1);
    lcd.print("HOLD FOR NEW   ");

    // wait for long press (5s), continue if released early
    while (pressed) {
      if(digitalRead(RESETPIN) == LOW) {
        pressed = 0;
      }
      else {      
        if (millis() > startPush + 5000) { // if button has been held 5s or longer
          
          // Play reset tune.
          tone(BUZZERPIN,500);
          delay(250);
          tone(BUZZERPIN,1500);
          delay(250);
          tone(BUZZERPIN,2500);     
          delay(800);
          noTone(BUZZERPIN);

          // Record the new spool's start weight to eeprom
          lcd.setCursor(0,1);
          lcd.print("RECORDING NEW  ");
          EEPROM.put(currentSpool*sizeof(int), currentWeight);
        }
      }
    }
  }
  
  // Find remaining filament from spool size, start weight and current weight
  int remaining = SPOOLSIZE - (startWeight - currentWeight);
  
  // Update the current spool and weight on LCD
  lcd.setCursor(13,0);
  lcd.print(currentSpool+1, DEC);
  lcd.setCursor(10,1);
  lcd.print(remaining, DEC);

  // Check if current weight is below threshold, set alert status if so
  if (remaining - THRESHOLD <= 0) {
    error("LOW ALERT      ", 0, true);
  }
  else {
    lcd.setCursor(0,1);
    lcd.print("Remaining:      ");
  }
}

// Display an error message and optionally play an annoying notification noise and/or flash an LED.
void error(char string[], int hz, bool flash) {
  lcd.setCursor(0,1);
  lcd.print(string);
  if (flash) {
    digitalWrite(LEDPIN, HIGH);
  }

  // Don't beep more often than defined
  if (millis() > lastBeep + TIMEBETWEENBEEPS) {
    tone(BUZZERPIN, hz);
    delay(100);
    noTone(BUZZERPIN);
    delay(30);
    tone(BUZZERPIN, hz);
    delay(100);
    noTone(BUZZERPIN);
    lastBeep = millis();
  }
  
  delay(2000); // Wait 2 seconds to display LED and LCD error
  lcd.setCursor(0,1); // Return cursor to home location for normal operation.
  digitalWrite(LEDPIN, LOW); // Turn off LED when returning to operation.
}
