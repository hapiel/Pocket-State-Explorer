/*

Q: Why does no debounce seem needed?
Q: What's the easiest way to short pin 5? Don't want to wait 5 seconds every time
Q: Is there a way to prevent plugging in and out all the time? Maybe a programmer board with a switch for the power and data lines?

DATA SAVING:
don't use sin() or pow()
don't use strip.colorHSV
don't use floats
don't use map()
could remove strip.brightness() to gain 5%?

Also no fastled, no digispark keyboard (for debugging)

FUNCTIONALITY:

PLAYING WITH STATES:
DONE State is represented with (teal) leds
DONE One by one the pixels shift to the left
DONE give 0 a bouncing animation, going in and out of brightness.

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
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIX, PIN_PIX, NEO_GRB + NEO_KHZ800);

TinyMCP23008 mcp;

uint16_t pButtonState;
uint16_t buttonState;

uint32_t holdTime[NUM_PIX];
uint32_t shiftTime;

uint8_t balls = 3; // amount of balls or objects
uint16_t ssState = 0b0000000111; 
uint16_t ssStateDisplayOld = 0b0000000111; 
uint16_t ssStateDisplayNew = 0b0000000111; 

uint32_t cState = strip.Color(0, 128, 128);
uint32_t cBlack = strip.Color(0, 0, 0);
uint32_t cDebug = strip.Color(0, 128, 0);
uint32_t cWhite = strip.Color(128,128,128);

void setup() {
  delay(5500); // wait for boot thingy
  strip.begin();
  strip.show();

  ssState = setBalls(balls);
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

  // up and down value, between 0 and 128
  uint8_t fader = (millis() / 3) % 256;
  if (fader > 128) fader = 256 - fader;

  uint32_t cBounce = strip.Color(0, 16 + fader, 16 + fader);
  
  // loop all pixels & buttons
  for (uint8_t i = 0; i < NUM_PIX; i++) {

    strip.setPixelColor(i, cBlack);

    if (bPressed(i)){

      holdTime[i] = millis();

      // if empty space AND (0 is lit OR 0 is pressed)
      if (!(ssState >> i & 1) && ((ssState & 1) || i == 0)) {

        // state will be updated!
        ssStateDisplayOld = ssState;
        ssStateDisplayOld &= ~1; // remove led 0
        ssStateDisplayNew = 0;
        if (i > 0) {
          ssStateDisplayOld |= 1 << i; // add new led;
        }
        shiftTime = millis();
        
        ssState |= 1 << i; // turn on bit i
        ssState >>= 1; // shift left
      }

    }

    if (bReleased(i)) {
      holdTime[i] = 0;
    }

    // if not 0, currently held, longer than 1s
    if (i > 0 && holdTime[i] != 0 && millis() - holdTime[i] > 1000 ){
      ssState = setBalls(i);
      ssStateDisplayNew = ssState;
      for (uint8_t i = 0; i < NUM_PIX; i++){
        if ((ssState >> i) & 1){
          strip.setPixelColor(i, cWhite);
        }
      }
    }

    if (ssState == ssStateDisplayNew){

      // if state is 1
      if ((ssState >> i) & 1){
        // if 0, use bounce animation instead
        if (i == 0){
          strip.setPixelColor(0, cBounce);
        } 
        else {
          strip.setPixelColor(i, cState);
        }
      } 
      
    } 
  }

  // animation has not finished
  if (ssState != ssStateDisplayNew){

    // animation speed
    if (millis() - shiftTime > 150){
      shiftTime = millis();
      for (uint8_t i = 1; i < NUM_PIX; i++) {

        if ((ssStateDisplayOld >> i) & 1 ) {

          // remove from old
          ssStateDisplayOld &= ~(1 << i);
          // add to new, one shifted
          ssStateDisplayNew |= 1 << (i - 1);
          break;
        }
      }
    }

    for (uint8_t i = 0; i < NUM_PIX; i++) {
      if ((ssStateDisplayOld >> i) & 1 || (ssStateDisplayNew >> i) & 1 ){
        strip.setPixelColor(i, cState);
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

// set all colors at once
void changeColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_PIX; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

uint16_t setBalls(uint8_t balls){
  // TODO: Refactor me. Still much smaller program size than the commented code below:
  // return pow(2, balls);

  if (balls == 1){
    return 1;
  }
  if (balls == 2){
    return 3;
  }
  if (balls == 3){
    return 7;
  }
  if (balls == 4){
    return 15;
  }
  if (balls == 5){
    return 31;
  }
  if (balls == 6){
    return 63;
  }
  if (balls == 7){
    return 127;
  }
  if (balls == 8){
    return 255;
  }
  if (balls == 9){
    return 511;
  }
  if (balls == 10){
    return 1023;
  }

}

// Set bit n to 1:
// ssState |= 1 << n;
// Set bit n to 0:
// ssState &= ~(1 << n);
// Flip bit n:
// ssState ^= 1 << n ;
