/*
Quellcode für den Teabot single Bausatz von LED-Studien.de
Wir bedanken uns bei Agile-Hardware/Objectfab für die Bereitstellung des Bausatzes.
*/

#include <Servo.h>
#include <Adafruit_NeoPixel.h>

#define DELAY             100
#define HALF_WAY          200 
#define UP_DELAY           20

#define START_VALUE      3600
#define DELTA_VALUE       600
#define MAX_VALUE        6600
 
#define NUM_MEAS           50
#define NUM_SMOOTH          1

#define HOT                 2
#define COOL                1
#define MEASURING_DELTA  5000

#define IDLE                0
#define BTN_PRESSED         1
#define BTN_RELEASED        2
#define COUNTDOWN           3
#define FINISHED            4
#define RESET               5

#define AUTO_RELEASE_TIME   1

#define PIN_SPEAKER         1
#define PIN_LEDS           10
#define NUM_LEDS           12
#define OFFSET_LEDS         6   # Korrektur der "oben"-Position des LED-Rings

#define PIN_UP              0
#define PIN_DOWN            2

#define PIN_SERVO           5

#define ARM_UPPER           45  # untere Armposition in Grad
#define ARM_LOWER           135 # obere Armposition in Grad

struct Side {
  byte  pinServo;
  byte  pinUp;
  byte  pinDown;
  byte  pinSensor;
  byte  up;
  byte  down;
  int   state;
  int   time;
  int   tempOffset;
  int   temp;
  int   lastTempRef;
  long  lastTimestamp;
  Servo servo;
  int   autoReleaseTimer;
  byte  autoReleaseTimerFirst;
  int   autoReleaseResetTimer;
};

Servo servo;

           // pins                             value   state  time    tempOffset temp lastTempRef lastTimestamp servo autoRealeaseTimer autoRealeaseTimerFirst autoRealeaseResetTimer
           // servo      up      down  sensor  up  down
Side right = {PIN_SERVO, PIN_UP, PIN_DOWN, A5, ARM_UPPER, ARM_LOWER, IDLE, START_VALUE, 2,    40,     40,           0,       servo,      0,                   1,                    0 };

Adafruit_NeoPixel leds = Adafruit_NeoPixel(NUM_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);


boolean buttonPressed(int pin) {
  return (digitalRead(pin) == LOW);
}

boolean isHot(void *_side) {
  Side*   side = (Side*) _side;
  boolean result = false;
  
  if (side->lastTimestamp > 0) {
    result = (side->temp - side->lastTempRef >= HOT);
  } // no else
  
  return result;
}

boolean isCool(void *_side) {
  Side* side = (Side*) _side;
  
  return ((side->temp - side->lastTempRef) <= COOL);
}

void playSound() {
  
  tone(PIN_SPEAKER,  440);
  delay(100);
  tone(PIN_SPEAKER,  880);
  delay(100);
  tone(PIN_SPEAKER, 1760);
  delay(100);
  noTone(PIN_SPEAKER);
}

void processSide(void *_side) {
  Side* side        = (Side*) _side;
  byte  autoRelease = 0;
  int   newState;
  
  side->temp = ((NUM_SMOOTH-1)*(side->temp) + (side->tempOffset) + getTemperature(side->pinSensor)) / NUM_SMOOTH;
  if (millis() - side->lastTimestamp >= MEASURING_DELTA) {
    side->lastTimestamp = millis();
    side->lastTempRef = side->temp;
  } // no else
  
  newState   = side->state;
  
  switch(side->state) {
    case IDLE:
      fastDown(side); 

      if (buttonPressed(side->pinDown) && (side->time > DELTA_VALUE)) {
        side->time -= DELTA_VALUE; 
        newState    = BTN_PRESSED;
      } else if (buttonPressed(side->pinUp) && (side->time < MAX_VALUE)) {
        side->time += DELTA_VALUE; 
        newState    = BTN_PRESSED;
      } else if (isHot(side)) {
        newState    = COUNTDOWN;
      }
      break;
      
    case BTN_PRESSED:
      side->autoReleaseTimer++;
      side->autoReleaseResetTimer = 0;
      
      if (side->autoReleaseTimerFirst == 1) {
        autoRelease = (side->autoReleaseTimer == AUTO_RELEASE_TIME * 10);
      } else {
        autoRelease = (side->autoReleaseTimer == AUTO_RELEASE_TIME * 2);
      }
      
      if ((!buttonPressed(side->pinDown) && !buttonPressed(side->pinUp)) || (autoRelease)) {
        side->autoReleaseTimer      = 0;
        side->autoReleaseTimerFirst = 0;
        newState                     = BTN_RELEASED;
      }
      break;
      
    case BTN_RELEASED:
      side->autoReleaseResetTimer++;
      
      if (side->autoReleaseResetTimer == 2) {
        side->autoReleaseTimerFirst = 1;
        side->autoReleaseResetTimer = 0;
      } // no else
    
      if (buttonPressed(side->pinDown) && (side->time >= DELTA_VALUE)) {
        side->time -= DELTA_VALUE; 
        newState    = BTN_PRESSED;
      } else if (buttonPressed(side->pinUp) && (side->time < MAX_VALUE)) {
        side->time += DELTA_VALUE; 
        newState    = BTN_PRESSED;
      } else if (isHot(side)) {
        newState    = COUNTDOWN;
      }
      break;
      
    case COUNTDOWN:
      side->time--;
      
      if (side->time <= 0) {
        slowUp(side);
        playSound();
        newState = FINISHED;
      }
      break;
      
    case FINISHED:
      if (buttonPressed(side->pinDown) || buttonPressed(side->pinUp)) { 
        side->time = START_VALUE;
        newState   = RESET;
      }
      break;
      
    case RESET:
      delay(DELAY);
      newState = IDLE;
      break;
  }
  
  side->state = newState;
}

void fastDown(void *_side) {
  Side* side = (Side*) _side;
  
  side->servo.attach(side->pinServo);
  delay(50);
  side->servo.write(side->down);
  delay(400);
  side->servo.detach();
}

void slowUp(void *_side) {
  Side* side = (Side*) _side;
  byte  start;
  byte  end;
  
  start = side->down;
  end   = side->up;
  
  side->servo.attach(side->pinServo);
  delay(100);
  
  if (start < end) {
    for (byte value = start; value < end; value++) {
      side->servo.write(value);
      delay(UP_DELAY);
    }
  } else {
    for (byte value = start; value > end; value--) {
      side->servo.write(value);
      delay(UP_DELAY);
    }
  }

  side->servo.detach();
}

int getTemperature(int pin) {
  int result = 0;
  
  for (int i=0; i<NUM_MEAS; i++) { 
    result += analogRead(pin);
  }
  result = result/NUM_MEAS;
  
  if (result<0) {
    result = 0;
  }
  
  if (result>200) {
    result = 200;
  }
  
  return result;	
}

uint32_t getColorForState(int state) {
  uint32_t result = 0x00000000;
  
  switch(state) {
    case IDLE:
    case BTN_PRESSED:
    case BTN_RELEASED:
      result = 0x0000ff00;
      break;
    case COUNTDOWN:
      result = 0x00ff0000;
      break;
    case FINISHED:
      result = 0x00ffffff;
      break;
    case RESET:
      result = 0x000000ff;
      break;
  }
  
  return result;
}

void displayLeds(void *_side) {
  Side*    side = (Side*) _side;
  uint32_t color;

  color = getColorForState(side->state);
  
  for (uint8_t i=0; (i<NUM_LEDS); i++) {
    if (i<=((side->time)/600)) {
      leds.setPixelColor((i+OFFSET_LEDS)%NUM_LEDS, color);
    } else {
      leds.setPixelColor((i+OFFSET_LEDS)%NUM_LEDS, 0x00000000);
    }
  }
  
  leds.show();
}

void setupServo() {
  pinMode(right.pinServo, OUTPUT);
}

void setupBeeper() {
  pinMode(PIN_SPEAKER, OUTPUT); 
}

void setupButtons(){
  pinMode(PIN_UP,   INPUT_PULLUP); 
  pinMode(PIN_DOWN, INPUT_PULLUP); 
}

void setupLeds() {
  pinMode(PIN_LEDS, OUTPUT); 
  leds.begin();
}

void setup() {
  setupServo();
  setupBeeper();
  setupButtons();
  setupLeds();

  playSound();
}

void loop() {
  processSide(&right);
  displayLeds(&right);
  delay(DELAY);
}
