/*

Q: Why does no debounce seem needed?
Q: What's the easiest way to short pin 5? Don't want to wait 5 seconds every time
Q: Is there a way to prevent plugging in and out all the time? Maybe a programmer board with a switch for the power and data lines?

DATA SAVING:
don't use sin() or pow()
don't use strip.colorHSV
don't use floats
don't use map()

Also no fastled, no digispark keyboard (for debugging)

FUNCTIONALITY:

PLAYING WITH STATES:
DONE State is represented with (teal) leds
When button is pressed, new ball appears on led (magenta?), all other balls turn (blue)?
One by one the pixels shift to the left, turning (teal) again.

DONE give 0 a bouncing animation, going in and out of brightness.
When holding a new number, it jumps on the press, but the shifting animation won't start untill the button release

Show error (red) when illegal move is attempted:
flash 0 when a 0 is required
flash number if there already is an object. 
On release only, need to allow for holds...

GO TO GROUND STATE:
Hold down a button to set the amount of balls.

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
#include <Adafruit_NeoPixel.h>
#include "TinyMCP23008.h"

#define PIN_PIX  1
#define NUM_PIX 10
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIX, PIN_PIX, NEO_GRB + NEO_KHZ800);

TinyMCP23008 mcp;

uint8_t br = 10; //brightness
uint16_t pButtonState;
uint16_t buttonState;
uint32_t holdTime[NUM_PIX];

uint8_t balls = 3; // amount of balls or objects
uint16_t ssState = 0b0000000111; ; 

uint32_t cState = strip.Color(0, br, br);
uint32_t cBlack = strip.Color(0, 0, 0);

void setup() {
  delay(5500); // wait for boot thingy
  strip.begin();
  strip.show();
  ssState = setBalls(balls);

  pinMode(3, INPUT);
  pinMode(4, INPUT);

  TinyWireM.begin();
  mcp.begin(0x27);
  for (int j = 0; j < 8; j++) {
    mcp.pinMode(j, INPUT);
    mcp.pullUp(j, HIGH);
  }

  changeColor(0, br, 0);
  delay(100);

  pButtonState = 0b1111111111;

  changeColor(0, 0, br);
  delay(100);
  changeColor(0, 0, 0);

}

void loop() {
  buttonState = (mcp.readGPIO() << 2) | (!digitalRead(4) << 1) | digitalRead(3) ;

  // TODO this effect is too small when at higher brightness, needs to be multiplied by "brightness setting". Other solutions seem too expensive?
  // up and down value, between 0 and 16/2
  uint8_t fader = (millis() / 32) % 16;
  if (fader > 8) fader = 16 - fader;

  uint32_t cBounce = strip.Color(0, fader, br);
  
  // loop all pixels & buttons
  for (byte i = 0; i < NUM_PIX; i++) {

    if (bPressed(i)){

      holdTime[i] = millis();

      // if empty space AND (0 is lit OR 0 is pressed)
      if (!(ssState >> i & 1) && ((ssState & 1) || i == 0)) {
        
        ssState |= 1 << i; // turn on led i
        ssState >>= 1; // shift left
      }

    }

    if (bReleased(i)) {
      holdTime[i] = 0;
    }

    // if not 0, currently held, longer than 1s
    if (i > 0 && holdTime[i] != 0 && millis() - holdTime[i] > 1000 ){
      ssState = setBalls(i);
    }

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
    else {
      strip.setPixelColor(i, cBlack);
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
// ssState = (1 << n) | ssState;
// Flip bit n:
// ssState ^= 1 << n ;
