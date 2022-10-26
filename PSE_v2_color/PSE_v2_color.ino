/*
Software for the Pocket State Explorer by Sylvain Garnavault & Daniel Simu.
Based on Digispark Attiny85
This code is written by Daniel Simu, 2022

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
Switch to single color mode

DATA SAVING:
don't use sin() or pow()
don't use strip.colorHSV
don't use floats
don't use map() REWROTE, USE map8()
could remove strip.brightness() to gain 5%?

Also no fastled, no digispark keyboard (for debugging)

*/

#include <avr/power.h>
#include <TinyWireM.h>
#include <Adafruit_NeoPixel.h> // older version from digispark? uses less storage..
#include "TinyMCP23008.h" // custom library by Sylvain Garnavault

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
  strip.Color(164, 0, 0),
  strip.Color(164, 64, 0),
  strip.Color(108, 128, 0),

  strip.Color(0, 164, 0),
  strip.Color(0, 128, 128),

  strip.Color(0, 0, 192),
  strip.Color(108, 0, 164),
  strip.Color(164, 0, 52),

  strip.Color(102, 102, 102)
};

uint32_t cState = strip.Color(0, 128, 128);
uint32_t cBlack = strip.Color(0, 0, 0);
uint32_t cDebug = strip.Color(0, 255, 0);
uint32_t cWhite = strip.Color(128,128,128);

void setup() {
  delay(5500); // wait for boot thingy
  strip.begin();
  strip.show();

  strip.setBrightness(14); // this function costs 4% memory, could possibly be removed.

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
  
  // loop all pixels & buttons
  for (uint8_t i = 0; i < NUM_PIX; i++) {

    strip.setPixelColor(i, cBlack);

    if (bPressed(i)){

      // if still shifting, shift all before proceeding
      while (shiftState < NUM_PIX){
        shiftNext();
      }

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
          strip.setPixelColor(i, cBalls[j]);
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

void shiftNext(){
  for (uint8_t i = 0; i < NUM_BALLS; i++) {   
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

uint8_t map8(uint8_t x, uint8_t in_min, uint8_t in_max, uint8_t out_min, uint8_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
