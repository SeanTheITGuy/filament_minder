

/*  Title: Filament Minder
 *  Author: Sean The IT Guy (theitguysean@gmail.com)
 *  Inception Date: 2019-04-14
 *  ---
 *  This is a simple sketch to keep track of SPOOLCOUNT number of filament spools' day0 starting weights,
 *  and calculate how much filament remains on the spool based upon that.  It is only as accurate as
 *  the assumption that the manufacturer has put the advertised amount on the spool.
 */

#include <HX711.h>
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <splash.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>

#define TITLE "Filament Minder"
#define VERSION "v0.4.1"
#define SPOOLCOUNT 8              // Number of spools we wish to store. Can be up to [ (EEPROM.length()/sizeof(spool)) - sizeof(spool) ] 
#define PREVPIN 5                 // pin that the "prev spool" button is on
#define NEXTPIN 4                 // pin that the "next spool" button is on
#define RESETPIN 6                // pin that the "new spool" button is on
#define BUZZERPIN 7               // screecher pin
#define THRESHOLD 50              // minimum amount (g) of remaining filament before alert.
#define TIMEBETWEENBEEPS 20000    // milliseconds between beep alerts
#define OLED_ADDR 0x3D            // I2C address of the OLED panel.
#define LOADCELL_DOUT_PIN 2       // DOUT pin for HX711 load cell ADC
#define LOADCELL_SCK_PIN 3        // SCK pin of HX711 load cell ADC
#define LOADCELL_OFFSET 50682624  // Define these after calibration.
#define LOADCELL_DIVIDER 5895655  // Define these after calibration.
#define SCREEN_WIDTH 128          // OLED display width, in pixels
#define SCREEN_HEIGHT 64          // OLED display height, in pixels
#define OLED_RESET -1             // Reset pin # (or -1 if sharing Arduino reset pin)

// Build a structure for the spool data
struct spool {
  int startWeight;
  int capacity;
  int future1;
  int future2;
};

spool currentSpool = { 0, 0, 0, 0 };     // active spool data struct
int id = 0;                            // the index of the current spool
int currentWeight;                // contains current spool's measured weight
int lastBeep = millis();          // keeps record of when a beep last occured to keep from overbeeping

// Set up the load cell.
HX711 scale;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() { 
   Serial.begin(115200);

   // Set up the display
   if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Display stored logo buffer TODO: replace the logo.
  display.display();
  delay(1000); // pause for 1 second.
  display.clearDisplay();
  
  // Configure the load cell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(LOADCELL_DIVIDER);
  scale.set_offset(LOADCELL_OFFSET);
  
  // Confgure the IO pins
  pinMode(BUZZERPIN, OUTPUT);
  pinMode(RESETPIN, INPUT);
  pinMode(NEXTPIN, INPUT);
  pinMode(PREVPIN, INPUT);

  // Display startup on OLED
  
  // Determine which spool was last used 
  EEPROM.get(sizeof(spool)*SPOOLCOUNT, id);
  if (id < 0 || id >= SPOOLCOUNT) {   // if retrieved data not valid, init to first spool
     id = 0;  
  }

  // Retrieve the spool struct
  EEPROM.get(sizeof(struct spool)*id, currentSpool);   
  
  // Execute a "ready" beep
  Serial.println("Ready!");
  beep(1000,100);
}

void loop() {  
   
  // Get the current weight from the loadcell (retry 100 times, then error out). get_units(X) averages X measurements.
  if (scale.wait_ready_retry(100)) {
    currentWeight = (int) scale.get_units(10);    // cast the long float response to an int for 1g granularity
    if (currentWeight < 0) {
      currentWeight = 0;                          // if measured data not valid, init to 0
    }
  } else {
      error("SCALE ERROR!!!!", 0);       // if load cell ADC not ready after 100 retries, display error.
 }

  // Check to see if a spool button is pressed to switch between spools
  int buttons[] = {PREVPIN, NEXTPIN};
  for(int i = 0; i < 2; i++) {
    if(digitalRead(buttons[i]) == HIGH) {
      
      // Set button pressed flag high
      int pressed = 1;

      // Play tone, pitch derived from current spool id
      beep(1000+250*id, 70);

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
      Serial.print("Active spool changed to: ");
      Serial.println(id);
    }
  }

  // Check for the spool reset button
  if(digitalRead(RESETPIN) == HIGH) {

    // Record push start time
    unsigned long startPush = millis();
    
    // Play initial beep
    beep(3000,100);

    // notify instructions for reset
        
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
        int capacity = 1000;
                
        while (initializing) {     
          
          // display updated capacity TODO
                    
          // adjust capacity of new spool
          if (digitalRead(PREVPIN) == HIGH) {
            beep(1000,10);
            capacity = capacity > 0 ? capacity - 100 : 0;
            Serial.print("Capacity ");
            Serial.println(capacity);
            delay(200);
          }
          if (digitalRead(NEXTPIN) == HIGH) {
            beep(2000,10);
            capacity = capacity < 9900 ? capacity + 100 : 9900;
            Serial.print("Capacity ");
            Serial.println(capacity);
            delay(200);
          }
          if (digitalRead(RESETPIN) == HIGH) {
            beep(2500,10);
            beep(1000,250);
            Serial.print("Accepted: ");
            Serial.println(capacity);
            initializing = 0;            
          }
        }

        // Record the defined capacity in the current spool's data structure
        currentSpool.capacity = capacity;
        
        // Record the new spool's data to eeprom
        EEPROM.put(sizeof(spool)*id, currentSpool);  // Update EEPROM with new data for this spool.          

        // display save status to OLED TODO
        
        // Play reset tune.
        beep(500,100);
        beep(1500,100);
        beep(2500,100);
      }
    }
  // Update display with status. TODO
  }
  
  // Find remaining filament from spool size, start weight and current weight
  int remaining = currentSpool.capacity - (currentSpool.startWeight - currentWeight);
  
  // Update the current spool and remnaining filament weight on LCD TODO
   
  // Check if current weight is below threshold, set alert status if so
  if (remaining - THRESHOLD <= 0) {
    error("LOW ALERT:", 0);
  }
  else {
    // display the latest data
  }
}

// Display an error message and optionally play an annoying notification noise and/or flash an LED.
void error(char string[], int hz) {
  // display error status TODO

  // Don't beep more often than defined
  if (millis() > lastBeep + TIMEBETWEENBEEPS && hz > 0) {
    beep(hz, 100);
    beep(hz, 100);
    lastBeep = millis();
  }
}

// play a single tone for defined delay
void beep(int hz, int s) {
    tone(BUZZERPIN, hz);
    delay(s);
    noTone(BUZZERPIN);
    delay(s);
}
