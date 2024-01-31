// Program:      GeniesEnHerbe.ino
// Author:       Vincent Fortin
// Contact:      vincent.fortin@gmail.com
// License:      GNU GPL version 3 (https://www.gnu.org/licenses/gpl-3.0.html)
// Purpose:      Use Arduino board to build a quiz game machine
//               for the "Genies en Herbe" high-school game
//               See https://en.wikipedia.org/wiki/G%C3%A9nies_en_herbe
//
// Changelog:    2019-08-02 Initial version, Vincent Fortin (vincent.fortin@gmail.com)
//               2024-01-16 2-team, 8-buzzer version of the code commited to Github
//                          (code developed shortly after the initial version)
//               2024-01-17 This version of the code allows for a LCD screen to be
//                          connected to the machine. LCD screen can be used in addition
//                          to or in replacement of individual LED lights
//               2024-01-31 Replace potentiometer with menu for selecting time before reset
//
// Behaviour:    The quiz machine helps determine which player and which team answers the fastest
//               to a question by pushing a buzzer. Each player has his own buzzer (push-button),
//               to which a LED is associated. The purpose of the machine is to determine which
//               player answers first, by lighting up the LED associated with the buzzer being
//               pushed first, and by making a sound. When a buzzer is pushed, the other buzzers
//               are deactivated. The buzzers can be re-activated by a reset button operated by
//               the referee. When the buzzers are activated, a green light is lit. There is a
//               timer after which the buzzers are re-activated if the referee does not push the
//               reset button. The duration of the timer is determined by a potentiometer. If
//               the potentiometer is set to its maximum value, then the buzzer must be
//               re-activated by the referee (there is no time limit).
//               Instead of using LED lights to identify which player has pushed the buzzer,
//               it is now possible to use an LCD screen. Both options are supported by the code
//
// Known issues: 1) The buttons take time to "warm up" (up to 30 seconds). A delay has been
//                  added during startup to deal with this issue.
//               4) Currently coded for Arduino Mega. Small modifications will be needed to
//                  make it work with other boards. A version that also works on the Arduino
//                  Nano is in development.

/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>
*/

/****************/
/* DEPENDENCIES */
/****************/

#include <stdio.h>

// Libraries required for SunFounder LCD screen
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Required in order to used the improved toneAC function
// instead of the tone function to activate the speaker
#include <toneAC.h>

/*****************/
/* CONFIGURATION */
/*****************/

/*
 * Number of teams and number of players per team
 */

// Configure the number of teams and number of players per team
// Normally there are two teams and four players per team
const int numTeams = 2;   // 2 teams
const int numPlayers = 4; // 4 players per team

/*
 * Speaker
 */

// Assign a pin number to the speaker and configure the sound
const int pinSpeaker = 11;
const int pinSpeaker2 = 12;
const int frequencySpeaker = 440; // Hz
const int durationSpeaker = 500; // milliseconds

/*
 * Delay before reset
 */

// Determine if we use potentiometer to determine the delay before a reset
bool usePotentiometer = false;

// Assign a pin number to the potentiometer used to determine the delay after a buzz before reset
// and define maximum delay (pinDelay not used if usePotentiometer is false)

const int pinDelay = 2;
const unsigned long delayResetMax = 5000; // maximum delay in milliseconds (scaled by potentiometer read)

// If we use the potentiometer delayReset will be recomputed based on the reading of the potentiometer
int delayReset = delayResetMax;

//

/*
 * Delay before the quiz machine starts
 */

// Startup delay to let the buttons warm up (I don't understand why this is needed!!!)
const unsigned long startupDelay = 30000; // milliseconds

/*
 * Minimum time during which a button must be pushed
 */

// Debounce delay to filter out flickers of button states
const unsigned long debounceDelay = 50;    // the debounce time for buttons in milliseconds

/* 
 * Attributes associated with each button / buzzer
 */

// Define structure for standard button
struct Button {
  int pinButton;        // pin number of the pushbutton
  int pinLED;           // pin number of the associated LED
  int buttonState;      // last stable state of the button (after filtering)
  int lastButtonState;  // last known state of the button (including flickers)
  int lastDebounceTime; // last time the button flickered (for debounce purposes)
}; 

/*
 * Configuration of each button
 * SET THE PIN NUMBERS CORECTLY BASED ON HOW YOUR ARDUINO BOARD IS WIRED)
 */

// Define each button by their pin numbers (for the button and the LED) and initial state
struct Button buzzers[numTeams][numPlayers] = {
  {
    {31,53,HIGH,LOW,0},
    {33,51,HIGH,LOW,0},
    {35,49,HIGH,LOW,0},
    {37,47,HIGH,LOW,0}
  }
  ,
  {
    {23,45,HIGH,LOW,0},
    {25,43,HIGH,LOW,0},
    {27,41,HIGH,LOW,0},
    {29,39,HIGH,LOW,0}
  }
};

// Configure the button used to reset the machin and the associated LED
struct Button whiteButton = {7,5,HIGH,LOW,0};
static int pinGreenLED; // will be set to the pinLED attribute of the white button

// Configure the LCD screen
LiquidCrystal_I2C lcd(0x27,20,4); //set the LCD address to 0x27 for a 16 chars and 2 line display

byte customChar[8] = {
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111,
	0b11111
};

// Initial setup
void setup() {

 // Make a little music to let people know the machine is booting
 for (unsigned long freq = 150; freq <= 15000; freq += 10) {  
    toneAC(freq); // Play the frequency (150 Hz to 15 kHz in 10 Hz steps).
    delay(1);     // Wait 1 ms so you can hear it.
  }
  toneAC(0); // Turn off toneAC, can also use noToneAC().

  // Loop through each buzzer and initialize the associated button and LED (in particular, set the pin mode)
  int team,player;
  for (team=0; team<numTeams; team++) {
    for (player=0; player<numPlayers; player++) {
      pinMode(buzzers[team][player].pinLED, OUTPUT);
      pinMode(buzzers[team][player].pinButton, INPUT_PULLUP);
      digitalWrite(buzzers[team][player].pinLED, LOW);
    }
  }

  // Set the pin mode to OUTPUT for the speaker
  // This line could be removed now that we are using toneAC instead of tone
  // to activate the speaker because toneAC uses specific pins
  pinMode(pinSpeaker, OUTPUT);
	
  // Set the pin mode to OUTPUT for the green LED
  pinGreenLED = whiteButton.pinLED;
  pinMode(pinGreenLED, OUTPUT);
  
  // Set the pin mode to INPUT for the white button
  pinMode(whiteButton.pinButton, INPUT_PULLUP);
  
  // Initialization of the LCD
  lcd.init(); 
  lcd.init();
  lcd.backlight();
  // Creation of a full square character
  lcd.createChar(0,customChar);
   
  // Wait for the buttons to warm up! (don't know why they need time before they start working...)

  // Use LCD to inform the users that the machine is warming up
  lcd.setCursor(0,0);
  lcd.print("Merci d'attendre ");
  lcd.print(startupDelay / 1000);
  lcd.setCursor(4,1);
  lcd.print(" secondes :)");
  for(int i = 0; i < 5; i++)
  {
    if(i == 0)
      lcd.setCursor(9,2);
    else
      lcd.setCursor(8,2);
    lcd.print(20 * i);
    lcd.print('%');
    if (i != 0)
    {
      lcd.setCursor(i*4 - 4, 3);
      for (int a = 0; a < 4; a++)
        lcd.write(byte(0));
    }
    delay(startupDelay / 5);
  }
  lcd.setCursor(7,2);
  lcd.print("100%");
  lcd.setCursor(16,3);
  for(int a = 0; a < 4; a++)
    lcd.write(byte(0));
  delay(100);
	
  // Light up the green LED to tell players that the quiz machine is working
  digitalWrite(pinGreenLED, HIGH);
 
 // Display the ready light on the LCD as well
 lcd.clear();
 for(int i = 0; i < 4; i ++)
      for (int a = 0; a < 4; a ++)
      {
      lcd.setCursor(16 + i, a);
      lcd.write(byte(0));
      } 
}

void loop() {
  /* 
   * Local variables
   */
  // Iterators
  int team, player;
  // Identification of the winning team and player
  int teamBuzz, playerBuzz;
  // Variables used to determine the delay before the reset of the buzzers
  int valPot;     // value of the potentiometer, between 0 and 1023
  // Boolean indicating if a buzzer was activated
  bool someoneBuzzed;
  // Timestamp used to determine how much time has passed since a buzzer was activated
  unsigned long timestamp;
  //temporary variables for LCD display
  int x = 0, y = 0;
  char message[21];
	
  /* 
   *  Start of the loop
   */

  // Determine if somebody has buzzed by scanning all the buzzers
  someoneBuzzed = false;
  for (team=0; team<numTeams; team++) {
    for (player=0; player<numPlayers; player++) {
      if (readButtonState(buzzers[team][player]) == LOW) {
        // A buzzer is activated: set the flag, keep track of which buzzer is activated and exit inner loop
        someoneBuzzed = true;
        teamBuzz = team;
        playerBuzz = player;
        break;
      }
      if (not usePotentiometer and readButtonState(whiteButton) == LOW) {
        // White button is pushed: enter setup mode
        while (true) {
          whiteButton.buttonState = HIGH;
          whiteButton.lastButtonState = HIGH;
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Config. du delai");
          lcd.setCursor(0,2);
          if (delayReset > 0) {
            sprintf(message,"Delai de %d sec.",int(delayReset/1000));
          }
          else {
            sprintf(message,"Delai infini");
          }
          lcd.print(message);
          delay(1000);
          if (readButtonState(whiteButton) == LOW) {
            lcd.setCursor(0,3);
            sprintf(message,"Delai confirme");
            lcd.print(message);
            delay(2000);
            break;
          }
          if (delayReset < 0) {
            delayReset = 1000;
          }
          else if (delayReset < delayResetMax) {
            delayReset += 1000;
          }
          else if (delayReset == delayResetMax) {
            delayReset *= 2;
          }
          else if (delayReset > delayResetMax) {
            delayReset = -1;
          }
        }
        reset_lcd_display();
      }
    }
    // A buzzer is activated: exit outer loop
    if (someoneBuzzed) {
      break;
    }
  }
  // Do this when someone activates his buzzer
  if (someoneBuzzed) {
    // turn off the green LED
    digitalWrite(pinGreenLED, LOW);
    // turn on the LED associated with the buzzer which was activated
    digitalWrite(buzzers[teamBuzz][playerBuzz].pinLED, HIGH);
    // play a sound (now using toneAC instead of tone)
    toneAC(frequencySpeaker, 10, durationSpeaker, true);
    // Obtain the delay by scaling the value read from the potentiometer by the maximum delay
    // Negative delays mean that the reset must be done manually
    if (usePotentiometer) {
      // read the value of the potentiometer to determine the duration of the delay before the reset
      // a value between 0 and 1023 is obtained
      valPot = analogRead(pinDelay);
      // If the value read is higher than 1000, set it to minus one.
      // This value will be used as a flag meaning that the white button must be pushed to reset the quiz machine.
      if (valPot > 1000) {
        valPot = -1; // quiz machine must be reset manually by the referee
      }
      delayReset = valPot*delayResetMax/1000;
    }
    //LCD display
    lcd.clear();
    y = team * 2;
    if(team == 1)
      x = player * 4;
    else
      x = abs(player - 3) * 4; //we need the player numbers inverted for this LCD setup, (team 1 are naturally inverted due to the wiring)
    for (int i = 0; i < 4; i++)
      for(int a = 0; a < 2; a++)
        {
         lcd.setCursor(x + i, y + a);
         lcd.write(byte(0));
        }

    // Wait for the end of the delay before doing a reset
    timestamp = millis();
    while ((delayReset < 0) || millis() - timestamp < delayReset) {
      // The delay has not ended: check if white button is pushed by the referee
      if (readButtonState(whiteButton) == LOW) {
        // White button is pushed: reset the quiz machine before the end of the delay
        while (readButtonState(whiteButton) == LOW) {
          delay(debounceDelay);
        }
        break;
      }  
    }
    // End of the delay or white button pushed: reset the quiz machine
    // Turn on the green LED
    digitalWrite(pinGreenLED, HIGH);
    // Turn off the LED of the buzzer which was activated
    digitalWrite(buzzers[teamBuzz][playerBuzz].pinLED, LOW);
    // Reset the LCD display
    reset_lcd_display();
  }
  // end of the loop: go back to the beginning!
}

void reset_lcd_display(){
    lcd.clear();
    for(int i = 0; i < 4; i ++)
      for (int a = 0; a < 4; a ++)
      {
      lcd.setCursor(16 + i, a);
      lcd.write(byte(0));
      }
}

// Read the state of a button (ensure that the button is pushed for a long enough time to deal with bouncing)
// Input:  button structure
// Output: state of the button (LOW or HIGH)
int readButtonState(Button button)
{
  // read the state of the switch into a local variable:
  int reading = digitalRead(button.pinButton);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != button.lastButtonState) {
    // reset the debouncing timer
    button.lastDebounceTime = millis();
  }
  
  if ((millis() - button.lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:
    button.buttonState = reading;
  }

  // Save last reading of the button state (including flickers)
  button.lastButtonState = reading;

  // Return last stable state of the button
  return button.buttonState;
}
