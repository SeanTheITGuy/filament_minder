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
#define VERSION "v0.3"
#define SPOOLCOUNT 8             // Number of spools we wish to store. Can be up to [ (EEPROM.length()/sizeof(spool)) - sizeof(spool) ] 
#define NEXTPIN 5                 // pin that the "next spool" button is on
#define PREVPIN 4                 // pin that the "prev spool" button is on
#define RESETPIN 6                // pin that the "new spool" button is on
#define BUZZERPIN 13              // screecher pin
#define THRESHOLD 50              // minimum amount (g) of remaining filament before alert.
#define LEDPIN 16                 // pin to drive a error state LED (could be LED_INTERNAL, or hooked to discrete LED)
#define TIMEBETWEENBEEPS 20000    // milliseconds between beep alerts
#define LOADCELL_DOUT_PIN 2       // DOUT pin for HX711 load cell ADC
#define LOADCELL_SCK_PIN 3        // SCK pin of HX711 load cell ADC
#define LOADCELL_OFFSET 50682624  // Define these after calibration.
#define LOADCELL_DIVIDER 5895655  // Define these after calibration.

// Build a structure for the spool data
struct spool {
  int startWeight;
  int capacity;
  int future1;
  int future2;
};

spool currentSpool = { 0, 0, 0, 0 };     // active spool
int id = 0;                            // the index of the current spool
int currentWeight;                // contains current spool's measured weight
int lastBeep = millis();          // keeps record of when a beep last occured to keep from overbeeping

// Set up the load cell.
HX711 scale;

// Set up the display
LiquidCrystal lcd(7, 8, 9, 10, 11 , 12);

void setup() { 
   //Serial.begin(57600);
  
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
  staticText();
 
  // Determine which spool was last used 
  EEPROM.get(sizeof(spool)*SPOOLCOUNT, id);
  if (id < 0 || id >= SPOOLCOUNT) {   // if retrieved data not valid, init to 1
     id = 0;  
  }

  // Retrieve the spool struct
  EEPROM.get(sizeof(struct spool)*id, currentSpool);   
  
  // Execute a "ready" beep and flash the LED.
  tone(BUZZERPIN,1000); 
  delay(100);
  noTone(BUZZERPIN);  
  blink();  
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
  for(int i = 0; i < 2; i++) {
    if(digitalRead(buttons[i]) == HIGH) {
      
      // Set button pressed flag high
      int pressed = 1;

      // Play tone, pitch derived from current spool id
      tone(BUZZERPIN,1000+250*id);
      delay(100);
      noTone(BUZZERPIN);

      // Update the current spool working var and EEPROM val, if outside bounds, do not change.
      if(buttons[i] == PREVPIN) {
        id = id - 1 < 0 ? 0 : id - 1;
      } else {
        id = id + 1 >= SPOOLCOUNT ? SPOOLCOUNT - 1 : id + 1;
      }
      EEPROM.put(sizeof(spool)*SPOOLCOUNT, id);
      
      // Retrieve the new spool's data
      EEPROM.get(sizeof(struct spool)*id, currentSpool);  
      if(currentSpool.startWeight < 0) {
        currentSpool.startWeight = 0;
      }
      if(currentSpool.capacity < 0) {
        currentSpool.capacity = 0;
      }

      // Do not continue until button is released.
      while (digitalRead(buttons[i]) == HIGH) {                
        delay(10);
      }
    }
  }

  // Check for the spool reset button
  if(digitalRead(RESETPIN) == HIGH) {

    // Record push start time
    unsigned long startPush = millis();
    
    // Play initial beep
    tone(BUZZERPIN,3000);
    delay(100);
    noTone(BUZZERPIN);

    // notify instructions for reset
    lcd.setCursor(0,1);
    lcd.print("HOLD FOR NEW   ");
    
    // wait for long press (5s), continue if released early
    while (digitalRead(RESETPIN) == HIGH) {
      if (millis() > startPush + 2000) { // if button has been held 2s or longer

        // Beep to confirm new spool mode
        beep(2000, 50);
        beep(2000, 50);
          
        // Wait for button to be released before continuing.
        while (digitalRead(RESETPIN) == HIGH) {
          delay(10);
        }
      
        // configure new spool's capacity
        int initializing = 1;
        lcd.setCursor(0,1);
        lcd.print("Capacity: ");
        lcd.setCursor(13,1);
        lcd.print("g");
        int capacity = 1000;
        
        while (initializing) {     
          
          lcd.setCursor(9,1);

          // Fix cursor offset if < 4 digits.
          if(capacity < 1000) {
            lcd.print(0);
            lcd.setCursor(10,1);
          }
          lcd.print(capacity);
          
          if (digitalRead(PREVPIN) == HIGH) {
            beep(1000,10);
            capacity = capacity > 0 ? capacity - 100 : 0;
            delay(200);
          }
          if (digitalRead(NEXTPIN) == HIGH) {
            beep(2000,10);
            capacity = capacity < 9900 ? capacity + 100 : 9900;
            delay(200);
          }
          if (digitalRead(RESETPIN) == HIGH) {
            beep(2500,10);
            beep(1000,250);
            initializing = 0;            
          }
        }
        currentSpool.capacity = capacity;
        // Record the new spool's data to eeprom
        currentSpool.startWeight = currentWeight;
        lcd.setCursor(0,1);
        lcd.print("RECORDING NEW  ");
        EEPROM.put(sizeof(spool)*id, currentSpool);  // Update EEPROM with new data for this spool.          

        // blink the LED
        blink();
        
        // Play reset tune.
        tone(BUZZERPIN,500);
        delay(250);
        tone(BUZZERPIN,1500);
        delay(250);
        tone(BUZZERPIN,2500);     
        delay(800);
        noTone(BUZZERPIN);
        startPush = millis();
      }
    }
  }
  
  // Find remaining filament from spool size, start weight and current weight
  int remaining = currentSpool.capacity - (currentSpool.startWeight - currentWeight);
  
  // Update the current spool and weight on LCD
  lcd.setCursor(13,0);
  lcd.print(id+1, DEC);
  lcd.print(" ");
  lcd.setCursor(10,1);
  
  // Fix cursor offset if < 4 digits.
  if(remaining < 1000) {
    lcd.print(0);
    lcd.setCursor(11,1);
  }
  lcd.print(remaining, DEC);

  // Check if current weight is below threshold, set alert status if so
  if (remaining - THRESHOLD <= 0) {
    error("LOW ALERT:", 0, true);
  }
  else {
    staticText();
  }
}

// Display an error message and optionally play an annoying notification noise and/or flash an LED.
void error(char string[], int hz, bool flash) {
  lcd.setCursor(0,1);
  lcd.print(string);
  if (flash) {
   blink();
  }

  // Don't beep more often than defined
  if (millis() > lastBeep + TIMEBETWEENBEEPS && hz > 0) {
    beep(hz, 100);
    beep(hz, 100);
    lastBeep = millis();
  }
  
  //delay(2000); // Wait 2 seconds to display LED and LCD error
  lcd.setCursor(0,1); // Return cursor to home location for normal operation.
  digitalWrite(LEDPIN, LOW); // Turn off LED when returning to operation.
}

// Display/refresh the static text
void staticText() {
  lcd.setCursor(0,0);
  lcd.print("Active Spool:");
  lcd.setCursor(0,1);
  lcd.print("Remaining:");
  lcd.setCursor(14,1);
  lcd.print("g");
}

// Flash the LED pin
void blink() {
  digitalWrite(LEDPIN, HIGH);
  delay(250);
  digitalWrite(LEDPIN, LOW);
  delay(50);
  digitalWrite(LEDPIN, HIGH);
  delay(250);
  digitalWrite(LEDPIN, LOW);
}

// play a single tone for defined delay
void beep(int hz, int s) {
    tone(BUZZERPIN, hz);
    delay(s);
    noTone(BUZZERPIN);
    delay(s);
}
