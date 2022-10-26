/*

DATA SAVING:
don't use sin() or pow()
don't use strip.colorHSV
don't use floats
don't use map()
could remove strip.brightness() to gain 5%?

Also no fastled, no digispark keyboard (for debugging)

FUNCTIONALITY:

PLAYING WITH STATES:
DONE State is represented with colored leds
DONE One by one the pixels shift to the left
give 0 a bouncing animation, going in and out of brightness.

Show error (red) when illegal move is attempted:
flash 0 when a 0 is required
flash number if there already is an object. 
On release only, need to allow for holds...

GO TO GROUND STATE:
DONE Hold down a button to set the amount of balls.
Store ball count in EEPROM

ANIMATE:
Hold down 0
The pattern will be animated on screen from the last time you were in this particular state.
Shows error if no loop was created since the last ball count reset.

FREE MODE:
Hold down button 9 to enter free mode. Works like normal state mode, 
but new balls will appear whenever you need.
Can be used to enter directly into excited state pattern.
New ball appears with different color and fades to regular

MENU:
Hold 0 and 9 to enter menu
Set brightness (store in EEPROM?)
Set animation speed

*/

#include <avr/power.h>
#include <TinyWireM.h>
#include <Adafruit_NeoPixel.h> // older version from digispark? uses less storage..
#include "TinyMCP23008.h"

#define PIN_PIX  1
#define NUM_PIX 10
#define NUM_BALLS 9
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIX, PIN_PIX, NEO_GRB + NEO_KHZ800);

TinyMCP23008 mcp;

uint16_t pButtonState;
uint16_t buttonState;

uint32_t holdTime[NUM_PIX];
uint32_t shiftTime;
uint8_t shiftState = 255;
uint32_t shiftSpeed = 150;

uint8_t balls = 3; // amount of balls or objects

uint8_t ballPos[NUM_BALLS] = {0, 1, 2, 255, 255, 255, 255, 255, 255};
uint32_t cBalls[NUM_BALLS]={
  strip.Color(128, 0, 0),
  strip.Color(128, 50, 0),
  strip.Color(85, 100, 0),

  strip.Color(0, 128, 0),
  strip.Color(0, 100, 100),

  strip.Color(0, 0, 150),
  strip.Color(85, 0, 128),
  strip.Color(128, 0, 40),

  strip.Color(80, 80, 80)
};

uint32_t cState = strip.Color(0, 128, 128);
uint32_t cBlack = strip.Color(0, 0, 0);
uint32_t cDebug = strip.Color(0, 255, 0);
uint32_t cWhite = strip.Color(128,128,128);

void setup() {
  delay(5500); // wait for boot thingy
  strip.begin();
  strip.show();

  strip.setBrightness(18); // this function costs 4% memory, could possibly be removed.

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
  
  // loop all pixels & buttons
  for (uint8_t i = 0; i < NUM_PIX; i++) {

    strip.setPixelColor(i, cBlack);

    if (bPressed(i)){

      holdTime[i] = millis();

      uint8_t zeroBall = 255;
      bool collision = false;

      // which ball is the 0 position ball?
      for (uint8_t j = 0; j < NUM_BALLS; j++) {
        if (ballPos[j] == 0){
          zeroBall = j;
        }
        if (ballPos[j] == i){
          collision = true;
        }
      }

      // if empty space AND (0 is lit OR 0 is pressed)
      if (!collision && (zeroBall != 255 || i == 0)) {

        // we're gonna start shifting!
        shiftTime = millis();
        shiftState = 0;
        
        // add new balls
        ballPos[zeroBall] = i;
      }
    }

    if (bReleased(i)) {
      holdTime[i] = 0;
    }

    // if not 0, currently held, longer than 1s
    if (i > 0 && holdTime[i] != 0 && millis() - holdTime[i] > 1000 ){
      setBalls(i);

      // Flash white
      for (uint8_t j = 0; j < i; j++){ 
        strip.setPixelColor(j, cWhite);
      }
    }

    // check all the balls to see which is at current position
    for (uint8_t j = 0; j < NUM_BALLS; j++) {
      if (ballPos[j] == i){
        strip.setPixelColor(i, cBalls[j]);
      }
    }
  }

  // animation has not finished
  if (shiftState < NUM_PIX){

    // animation speed
    if (millis() - shiftTime > shiftSpeed){
      
      for (uint8_t i = 0; i < NUM_BALLS; i++) {
        
        if (ballPos[i] == shiftState){
          // shift the ball
          ballPos[i] -= 1;
          // wait until new time
          shiftTime = millis();
          
        }
      }
      // update state
      shiftState += 1;
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

// set all colors at once
void changeColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_PIX; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

void setBalls(uint8_t balls){

  for (uint8_t i = 0; i < NUM_BALLS; i++){
    if (i < balls){
      ballPos[i] = i;
    } else {
      ballPos[i] = 255;
    }
  }
}

// Set bit n to 1:
// ssState |= 1 << n;
// Set bit n to 0:
// ssState &= ~(1 << n);
// Flip bit n:
// ssState ^= 1 << n ;
