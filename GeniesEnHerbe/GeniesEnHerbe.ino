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
//               2024-02-01 Change pin numbers to be able to use an Arduino Nano
//                          (if you want to light up a LED when a player pushes a
//                          buzzer you need to use an Arduino Mega or better)
//               2024-02-02 Adapted and simplified the code for Arduino Nano
//                          (now tested on Nano)
//
// Behaviour:    The quiz machine helps determine which player and which team answers the fastest
//               to a question by pushing a buzzer. Each player has his own buzzer (push-button).
//               The purpose of the machine is to determine which player answers first. When a
//               buzzer is pushed, the other buzzers are deactivated. The buzzers can be
//               re-activated by a reset button operated by the referee. There is a
//               timer after which the buzzers are re-activated if the referee does not push the
//               reset button. The duration of the timer is determined via a configuration menu.
//               An LCD screen is used to identify which player has buzzed.
//
// Known issues: 1) The buttons take time to "warm up" (up to 30 seconds). A delay has been
//                  added during startup to deal with this issue.
//               2) Currently tested only with Arduino Mega and Arduino Nano
//
// How the pins should be connected:
//
// Buzzers for Team 1:
// Player 1: one wire on pin 5, the other on ground
// Player 2: one wire on pin 4, the other on ground
// Player 3: one wire on pin 3, the other on ground
// Player 4: one wire on pin 2, the other on ground
//
// Buzzers for Team 2:
// Player 1: one wire on pin 6, the other on ground
// Player 2: one wire on pin 7, the other on ground
// Player 3: one wire on pin 8, the other on ground
// Player 4: one wire on pin 9, the other on ground (Arduino Mega)
//           one wire on pin 11, the other on ground (Arduino Nano)
//
// Reset button:
// one wire on pin 10, the other on ground (Arduino Mega)
// one wire on pin 12, the other on ground (Arduino Nano)
//
// LCD screen:
// Connect the four wires to ground, +5V, SDA and SCL
// On Arduino Nano use A4 for SDA and A5 for SCL
//
// Speaker:
// pins 11 and 12 for Arduino Mega
// pins 9 and 10 for Arduino Nano
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
 * Constants that need to be adapted depending on what board is used
 */

// Because the toneAC function requires dedicated pins that vary
// from board to board, we need to adjust other pins depending on the board we use

// Arduino Mega
/*
const int pin_team2player4 = 9;
const int pin_reset = 10;
*/

// Arduino Nano
const int pin_team2player4 = 11;
const int pin_reset = 12;

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

// Frequency of the speaker for team 1, will be divided in half for team 2
const int frequencySpeaker = 440; // Hz

const int durationSpeaker = 500; // milliseconds

/*
 * Delay before reset
 */

int delayReset = 3000; // maximum delay in milliseconds
int delayReset2; // 2 x delayReset (used to adjust delayReset through the config menu)

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
const unsigned long debounceDelay = 500;    // the debounce time for buttons in milliseconds

/* 
 * Attributes associated with each button / buzzer
 */

// Define structure for standard button
struct Button {
  int pinButton;        // pin number of the pushbutton
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
    {5,HIGH,LOW,0},
    {4,HIGH,LOW,0},
    {3,HIGH,LOW,0},
    {2,HIGH,LOW,0}
  }
  ,
  {
    {6,HIGH,LOW,0},
    {7,HIGH,LOW,0},
    {8,HIGH,LOW,0},
    {pin_team2player4,HIGH,LOW,0}
  }
};

// Configure the button used to reset the machine
struct Button whiteButton = {pin_reset,HIGH,LOW,0};

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

  // Loop through each buzzer and initialize the associated button
  int team,player;
  for (team=0; team<numTeams; team++) {
    for (player=0; player<numPlayers; player++) {
      pinMode(buzzers[team][player].pinButton, INPUT_PULLUP);
    }
  }

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
      if (readButtonState(whiteButton) == LOW) {
        delay(1000);
        if (readButtonState(whiteButton) == LOW) {
        // White button is pushed for a long time: enter setup mode
          delayReset2 = delayReset*2;
          while (true) {
            // Print a message to tell the user that he's entering setup mode
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Config. du delai");
            lcd.setCursor(0,3);
            lcd.print("Clic pour confirmer");
            lcd.setCursor(0,2);
            if (delayReset > 0) {
              sprintf(message,"Delai de %d sec.",int(delayReset/1000));
            }
            else {
              sprintf(message,"Delai infini");
            }
            lcd.print(message);
            delay(1000);
            // Exit setup mode if the user presses again the button
            if (readButtonState(whiteButton) == LOW) {
              lcd.setCursor(0,3);
              sprintf(message,"Delai confirme      ");
              lcd.print(message);
              delay(2000);
              break;
            }
            // Each time we go through this part of the loop we change delayReset
            // Adding 1 sec if it is lower than delayReset2 (2 x initial value of delayReset)
            // Then once we reach delayReset2 we set it to -1 (no reset) then back to 1 sec
            // This continues until the user exits the setup by pressing the button again
            if (delayReset < 0) {
              delayReset = 1000;
            }
            else if (delayReset < delayReset2) {
              delayReset += 1000;
            }
            else if (delayReset >= delayReset2) {
              delayReset = -1;
            }
          }
          reset_lcd_display();
        }
      }
    }
    // A buzzer is activated: exit outer loop
    if (someoneBuzzed) {
      break;
    }
  }
  // Do this when someone activates his buzzer
  if (someoneBuzzed) {
    // play a sound (now using toneAC instead of tone)
    toneAC(frequencySpeaker/(team+1), 10, durationSpeaker, true);
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
