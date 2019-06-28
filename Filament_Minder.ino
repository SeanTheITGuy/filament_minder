

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
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define TITLE "Filament Minder"
#define VERSION "v0.5.0"
#define SPOOLCOUNT 8              // Number of spools we wish to store. Can be up to [ (EEPROM.length()/sizeof(spool)) - sizeof(spool) ] 
#define PREVPIN 4                 // pin that the "prev spool" button is on
#define NEXTPIN 5                 // pin that the "next spool" button is on
#define RESETPIN 6                // pin that the "new spool" button is on
#define BUZZERPIN 7               // screecher pin
#define THRESHOLD 50              // minimum amount (g) of remaining filament before alert.
#define TIMEBETWEENBEEPS 20000    // milliseconds between beep alerts
#define LOADCELL_DOUT_PIN 3       // DOUT pin for HX711 load cell ADC
#define LOADCELL_SCK_PIN 2        // SCK pin of HX711 load cell ADC
#define LOADCELL_OFFSET 145700    // Define these after calibration.
#define LOADCELL_DIVIDER -172.5   // Define these after calibration.
#define SCREEN_WIDTH 128          // OLED display width, in pixels
#define SCREEN_HEIGHT 64          // OLED display height, in pixels
#define OLED_RESET -1             // Reset pin # (or -1 if sharing Arduino reset pin)
#define OLED_ADDR 0x3C            // I2C address of the OLED panel.

#define DEBUG
#ifdef DEBUG
  #define DEBUG_START(x) Serial.begin(x)
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_START(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)  
#endif


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
  
  DEBUG_START(115200);

  // Set up the display
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  
  // Display stored logo buffer TODO: replace the logo.
  display.display();
  delay(1000); // pause for 1 second.
  
  
  // Configure the load cell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(LOADCELL_DIVIDER);
  scale.set_offset(LOADCELL_OFFSET);
  
  // Confgure the IO pins
  pinMode(BUZZERPIN, OUTPUT);
  pinMode(RESETPIN, INPUT);
  pinMode(NEXTPIN, INPUT);
  pinMode(PREVPIN, INPUT);

  // Display startup on OLED TODO
  
  // Determine which spool was last used, retrieving data from memory above highest spool
  EEPROM.get(sizeof(spool)*SPOOLCOUNT, id);
  if (id < 0 || id >= SPOOLCOUNT) {   // if retrieved data not valid, init to first spool
     id = 0;  
  }

  // Retrieve the last used spool struct
  EEPROM.get(sizeof(struct spool)*id, currentSpool);   

  // Configure display
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(WHITE);
  
  // Execute a "ready" beep
  DEBUG_PRINTLN("Ready!");
  beep(1000,100);

  // Wipe display
  blank();
}

void loop() {  

  // Get the current weight from the loadcell. get_units(X) averages X measurements.
  currentWeight = (int) scale.get_units(5);    // cast the long float response to an int for 1g granularity
  DEBUG_PRINTLN(currentWeight);
  if (currentWeight < 0) {
    currentWeight = 0;                          // if measured data not valid, init to 0
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
      EEPROM.update(sizeof(spool)*SPOOLCOUNT, id);
      
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
      DEBUG_PRINT("Active spool changed to: ");
      DEBUG_PRINTLN(id);
    }
  }

  // Check for the spool reset button
  if(digitalRead(RESETPIN) == HIGH) {

    // Record push start time
    unsigned long startPush = millis();
    
    // Play initial beep
    beep(3000,100);

    // wait for long press (5s), continue if released early
    blank();
    
    while (digitalRead(RESETPIN) == HIGH) {      
      error("HOLD",0);
      if (millis() > startPush + 2000) { // if button has been held 2s or longer

        // Beep to confirm new spool mode
        beep(2000, 50);
        beep(2000, 50);
          
        // Wait for button to be released before continuing.
        blank();
        while (digitalRead(RESETPIN) == HIGH) {
          error("RELEASE",0);
          delay(10);
        }
        blank();

        // configure new spool's capacity
        int initializing = 1;
        int capacity = 1000;
                
        while (initializing) {     
          
          // display updated capacity TODO
          display.setTextColor(WHITE, BLACK);
          display.setCursor(10,0);
          display.println("CAPACITY:");
          display.setCursor(10,31);
          display.print(capacity);
          display.println(" g    ");
          display.display();
                    
          // adjust capacity of new spool
          if (digitalRead(PREVPIN) == HIGH) {
            beep(1000,10);
            capacity = capacity > 0 ? capacity - 100 : 0;
            DEBUG_PRINT("Capacity ");
            DEBUG_PRINTLN(capacity);
            delay(50);
          }
          if (digitalRead(NEXTPIN) == HIGH) {
            beep(2000,10);
            capacity = capacity < 9900 ? capacity + 100 : 9900;
            DEBUG_PRINT("Capacity ");
            DEBUG_PRINTLN(capacity);
            delay(50);
          }
          if (digitalRead(RESETPIN) == HIGH) {
            beep(2500,10);
            beep(1000,250);
            DEBUG_PRINT("Accepted: ");

            // Display accepted message
            blank();
            display.setCursor(10,19);
            display.println("ACCEPTED!");
            display.display();            
            initializing = 0; 
            delay(500);
            blank();          
          }
        }

        // Record the defined capacity in the current spool's data structure
        currentSpool.capacity = capacity;
        currentSpool.startWeight = (int) scale.get_units(20);
        
        // Record the new spool's data to eeprom
        EEPROM.put(sizeof(spool)*id, currentSpool);  // Update EEPROM with new data for this spool.          
        
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
    error("LOW ALERT", 0);
  }
  else {
    error("          ", 0);
  }

  // Display current information
  display.setTextColor(WHITE, BLACK);
  display.setCursor(2, 0);
  display.print("SPOOL:");
  display.print(id+1);
  display.setCursor(2,42);
  display.print("                  ");
  display.setCursor(2,42);
  display.print("LEFT:");
  display.print(remaining);
  display.print("g");
  display.display();
}

// Display an error message and optionally play an annoying notification noise
void error(char string[], int hz) {
  // display error status TODO
  display.setTextColor(WHITE, BLACK);
  display.setCursor(2, 20);
  display.print(string);
  display.display();
  
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

void blank() {
  // Wipe display
  display.clearDisplay();
  display.display();
  display.setTextColor(WHITE, BLACK);
}
