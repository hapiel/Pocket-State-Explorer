/*
Software for the Pocket State Explorer by Sylvain Garnavault & Daniel Simu.
Based on Digispark Attiny85
This code is written by Daniel Simu, 2022

FUNCTIONALITY:

PLAYING WITH STATES:
DONE State is represented with colored leds
DONE One by one the pixels shift to the left
DONE give 0 a bouncing animation, going in and out of brightness.

DONE Show error (red) when illegal move is attempted:
DONE flash 0 when a 0 is required
DONE flash number if there already is an object. 

GO TO GROUND STATE:
DONE Hold down a button to set the amount of balls.
DONE Store ball count in EEPROM

RUN:
// PROBLEM: What if you don't want to do a 0 before you run? try button release instead of button press?
Hold down 0
The pattern will be animated on screen from the last time you were in this particular state.
Shows error if no loop was created since the last ball count reset.

FREE MODE:
Hold down button 9 to enter free mode. Works like normal state mode, 
but new balls will appear whenever you need.
Can be used to enter directly into excited state pattern.
New ball appears with different color and fades to regular

MENU:
DONE Hold 0 and 9 to enter brightness menu
DONE Set brightness (store in EEPROM?)
hold 0 and 8 to set animation speed
hold 0 and 7 to switch to single color mode (one led animated rainbow for rainbow mode, default)

OTHER IDEAS:
Undo? which button to use for this?
Could we have a fancy transition finder?
Siteswap generator?


BOOT ANIMATION:
Some kind of fancy rainbow?

DATA SAVING:
don't use sin() or pow()
don't use strip.colorHSV
don't use floats
don't use map(), REWROTE, USE map8()
could remove strip.brightness() to gain 5%?

Also no fastled, no digispark keyboard (for debugging)

*/

#include <avr/power.h>
#include <TinyWireM.h>
#include <Adafruit_NeoPixel.h> // older version from digispark? uses less storage..
#include "TinyMCP23008.h" // custom library by Sylvain Garnavault

#include <EEPROM.h> // normal arduino library, needs to be copied to digispark folder
// EEPROM addresses:
// 0: ball count
// 1: brightness

#define PIN_PIX  1
#define NUM_PIX 10
#define MAX_BALLS 9
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIX, PIN_PIX, NEO_GRB + NEO_KHZ800);

TinyMCP23008 mcp;

uint16_t pButtonState;
uint16_t buttonState;

uint32_t holdTime[NUM_PIX];     // how long button has been down
uint32_t shiftTime;             // time since last shift
uint8_t shiftState = 255;       // which pixel is checked for shifting
uint8_t shiftSpeed = 150;       // how fast do we shift

uint8_t errorPixel = 255;       // which pixel should be displaying an error message

uint8_t br = 2;                // brightness setting
uint8_t brOptions[9] = {4, 9, 14, 20, 30, 60, 90, 150, 255};


enum modes {explore, run, menuBr};
enum modes currentMode = explore;

#define HIST_SIZE 32            // size of state history

uint16_t stateHist[HIST_SIZE];  // storing ss states
uint8_t stateHistPos;           // current storing position

/*
TODO:
DONE Store each new state in the stateHist, and update the stateHistPos, wrapping around
DONE update state when updating ballcount, and empty the history (so that you can't accidentally jump between 4 balls, 3 balls and then back to 4)
DONE when holding 0, check if same state appears in history. 
  if yes: store current position, jump back to previous, and go into animation mode: one by one cycle through states (compare states to find which ball moved), until a button is pressed

  display: Current pixels show green whilst holding to signal loop is found, red when not found

*/

uint32_t cWhite = strip.Color(128,128,128);

uint8_t ballPos[MAX_BALLS];

// starting with red makes prettier rainbow, but is confusing with the 'error' color.
// the cyan, blue and magenta look particularily pretty on the board..

uint32_t cBalls[MAX_BALLS]={

  strip.Color(0, 100, 128), // 0 cyan
  strip.Color(0, 0, 192),   // 1 blue
  strip.Color(80, 0, 164),  // 2 magenta
  strip.Color(164, 0, 52),  // 3 pink

  strip.Color(176, 0, 0),   // 4 red
  strip.Color(164, 64, 0),  // 5 orange
  strip.Color(108, 128, 0), // 6 yellow

  strip.Color(0, 164, 0),   // 7 green
  strip.Color(0, 164, 40)   // 8 turquoise
};



void setup() {
  delay(5500); // wait for boot thingy
  strip.begin();
  strip.show();
  
  if (EEPROM.read(0) != 255){
    setBalls(EEPROM.read(0));
  }
  if (EEPROM.read(1) != 255){
    br = EEPROM.read(1);
  }

  strip.setBrightness(brOptions[br]); // this function costs 4% memory, could possibly be removed.

  pinMode(3, INPUT);
  pinMode(4, INPUT);

  TinyWireM.begin();
  mcp.begin(0x27);
  for (int j = 0; j < 8; j++) {
    mcp.pinMode(j, INPUT);
    mcp.pullUp(j, HIGH);
  }

  changeColor(0, 255, 0);
  delay(100);

  pButtonState = 0b1111111111;

  changeColor(0, 0, 255);
  delay(100);
  changeColor(0, 0, 0);
}

void loop() {
  buttonState = (mcp.readGPIO() << 2) | (!digitalRead(4) << 1) | digitalRead(3) ;

  // for fading animations, producing a pingpong effect
  uint8_t fader = (millis() / 3) % 256;
  if (fader > 128) fader = 256 - fader;

  // for flicker animations, switching between on and off.
  bool flicker = (millis() / 100) % 2;

  bool runError = false; // used if no such state in history
  
  if (currentMode == explore){

    // loop all pixels & buttons
    for (uint8_t i = 0; i < NUM_PIX; i++) {

      // turn all pixels off
      strip.setPixelColor(i, 0);

      if (bPressed(i)){

        holdTime[i] = millis();

        // if still shifting, shift all before proceeding
        while (shiftState < NUM_PIX){
          shiftNext();
        }

        uint8_t zeroBall = 255;
        bool collision = false;

        // which ball is the 0 position ball?
        for (uint8_t j = 0; j < MAX_BALLS; j++) {
          if (ballPos[j] == 0){
            zeroBall = j;
          }
          if (ballPos[j] == i){
            collision = true;
            errorPixel = i;
          }
        }

        // CORRECT MOVE if empty space AND (0 is lit OR 0 is pressed), AND NOT (not button 0, but 0 is down too)
        if (!collision && (zeroBall != 255 || i == 0) && !(i !=0 && bDown(0))) {

          // we're gonna start shifting!
          shiftTime = millis();
          shiftState = 0;
          
          // add new balls
          ballPos[zeroBall] = i;


          // load current state and add new one to history
          uint16_t newState = stateHist[stateHistPos];
          newState |= 1 << i; // turn on bit i
          newState >>= 1; // shift left
          addState(newState);

        }

        // if 0 is off and 0 is not pressed
        if (zeroBall == 255 && i != 0){
          errorPixel = 0;
        }

      }

      if (bReleased(i)) {
        holdTime[i] = 0;
        errorPixel = 255;
      }

      // if not 0, currently held, longer than 1.3s
      if (i > 0 && holdTime[i] != 0 && millis() - holdTime[i] > 1300 ){
        setBalls(i);
        errorPixel = 255;

        // Flash cyan
        for (uint8_t j = 0; j < i; j++){ 
          strip.setPixelColor(j, cBalls[0]); 
        }
      }

      // if 0 is held longer than 1s
      if (i == 0 && holdTime[i] != 0 && millis() - holdTime[i] > 1000){

        // if 9 also down
        if (bDown(9)){
          currentMode = menuBr;
        }
        
        // if next occurence of state is not current position, start run animation
        if (findState(stateHist[stateHistPos]) != stateHistPos){
          // currentMode = run;
          changeColor(0, 100, 0);
        } else {
          // show all balls as red, takes place in display balls script below
          runError = true;
        }
      }


      // check all the balls to see which is at current position
      for (uint8_t j = 0; j < MAX_BALLS; j++) {
        if (ballPos[j] == i){

          // if 0, then bounce
          if (i == 0){

            uint8_t cComponents[3];

            // somehow using 2 loops compiles faster than combining these loops?

            // split color into components
            for (uint8_t k = 0; k < 3; k++) {
              cComponents[k] = cBalls[j] >> k*8;
            }

            // substract mapped fader from components
            for (uint8_t k = 0; k < 3; k++) {
              cComponents[k] -= map8(fader,0,128,0,(cComponents[k]));
            }

            // rebuild the color
            uint32_t cBounceColor = strip.Color(cComponents[2],cComponents[1], cComponents[0]);

            strip.setPixelColor(i, cBounceColor);
          }
          // else set color without animation
          else {
            if (runError){
              strip.setPixelColor(i, cBalls[4]); // show all red
            } else {
              strip.setPixelColor(i, cBalls[j]);
            }
            
          }

        }
      }

      
    }

    // shift animation has not finished
    if (shiftState < NUM_PIX){
      // if shiftSpeed time has passed, shift
      if (millis() - shiftTime > shiftSpeed){
        shiftNext();
      }
    }

    // if error exists & we're blinking on
    if (errorPixel < 255 ){
      if (flicker){
        strip.setPixelColor(errorPixel, cBalls[4]); // red
      } else {
        strip.setPixelColor(errorPixel, 0);
      }
    }
  }

  if (currentMode == run){
    // TODO:
    // PROBLEM: What if you don't want to do a 0 before you run? try button release instead of button press?
    // runstart, show all current items as green
    // remain in normal mode, but ignore normal key action
    // key action takes you back to explore mode
    // deduce next move
    // every x time, perform next move
  }

  if (currentMode == menuBr){

    // first pixel to "return" color
    strip.setPixelColor(0, cBalls[7]); // green

    // pressing 0 takes you back
    if (bPressed(0)){

      for (uint8_t i = 1; i < NUM_PIX; i++){
        holdTime[i] = 0; // to not mess up when going back to normal mode
      }

      EEPROM.update(1, br); 
      currentMode = explore; 

    }
    // all other buttons adjust brightness
    for (uint8_t i = 1; i < NUM_PIX; i++){

      if (bPressed(i)){
        br = i - 1;
        strip.setBrightness(brOptions[br]);
      }
      strip.setPixelColor(i, cBalls[0]); // cyan
      if (i - 1 == br){
        strip.setPixelColor(i, cBalls[3]);
      }
    }
  }

  // update
  strip.show();
  pButtonState = buttonState;
}

// button handling
bool bDown(uint8_t button){
  return !((buttonState >> button) & 1);
}

bool bPressed(uint8_t button){
  return (!((buttonState >> button) & 1) and ((pButtonState >> button) & 1));
}

bool bReleased(uint8_t button){
  return (((buttonState >> button) & 1) and !((pButtonState >> button) & 1));
}

// set all colors at once TEMP FUNCTION
void changeColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_PIX; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

void setBalls(uint8_t balls){
  
  for (uint8_t i = 0; i < MAX_BALLS; i++){
    if (i < balls){
      ballPos[i] = i;
    } else {
      ballPos[i] = 255;
    }
  }

  EEPROM.update(0, balls);

  // clean the history
  for (uint8_t i = 0; i < HIST_SIZE; i++){
    stateHist[i] = 0;
  }

  // TODO: Refactor this part
  if (balls == 1){
    addState(1);
  }
  if (balls == 2){
    addState(3);
  }
  if (balls == 3){
    addState(7);
  }
  if (balls == 4){
    addState(15);
  }
  if (balls == 5){
    addState(31);
  }
  if (balls == 6){
    addState(63);
  }
  if (balls == 7){
    addState(127);
  }
  if (balls == 8){
    addState(255);
  }
  if (balls == 9){
    addState(511);
  }

}

void shiftNext(){
  for (uint8_t i = 0; i < MAX_BALLS; i++) {   
    if (ballPos[i] == shiftState){
      // shift the ball
      ballPos[i] -= 1;
      // prepare the new time
      shiftTime = millis();
    }
  }
  // update state
  shiftState += 1;
}

// copied from arduino map() but using uint8_t instead of long
uint8_t map8(uint8_t x, uint8_t in_min, uint8_t in_max, uint8_t out_min, uint8_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void addState(uint16_t state){
  stateHistPos ++;
  stateHistPos %= HIST_SIZE;
  stateHist[stateHistPos] = state;
}

// check to see if state appears in history, return position. Return current position if no history.
uint8_t findState(uint16_t state){
  // looping backwards through positions
  uint8_t pos; 
  for (uint8_t i = stateHistPos + HIST_SIZE -1 ;  i >= stateHistPos; i --){
    pos = i % HIST_SIZE; 
    if (stateHist[pos] == state){
      break;
    }
  }

  return pos;
}