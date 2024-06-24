//  NeoPixel
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

//  USB/HID
#include "Adafruit_TinyUSB.h"
// HID report descriptor using TinyUSB's template
// Single Report (no ID) descriptor
uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD()
};

Adafruit_USBD_HID usb_hid;
uint8_t const report_id = 0;
uint8_t const modifier = 0;
uint8_t keycode[6] = { 0 };


// TRRS pin assignments
uint8_t allpins[] = {PIN_TIP, PIN_RING1, PIN_RING2, PIN_SLEEVE};
const int DITPIN = PIN_TIP;
const int DAHPIN = PIN_RING1;
const int SIDETONEPIN = PIN_RING2;

// Global variables for ISR
volatile int DIT_FLAG=0;
volatile int DAH_FLAG=0;

// ISR Mode (FALLING, LOW, HIGH, RISING)
#define INTERRUPT_MODE  FALLING

// variable to detect how long a button is held
volatile int holdCounter=0;

// Math time for intervals since we are using milliseconds
// 20 words per minute 
const int wpm = 20;
// PARIS constant 1200 millis divided by wpm
int dit_interval = (1200/wpm);
//  DAH interval is 3 times the dit interval
int dah_interval = (dit_interval * 3);
//  if the blank between dit/dah elements times out (> letter_interval) start a new letter 
//int letter_interval = (dit_interval * 3);
int letter_interval = (dit_interval * 3);

//  Set debounce buffer to our smallest interval
//volatile int debounceTime=dit_interval;
volatile int debounceTime=20;

//  pseudo queue to store incoming dit/dah elements (dde)
#define MAXQ  9
volatile char letterQueue[MAXQ];
volatile int qTail=-1;
String outputCode;

// letter map
#include <map>
std::map <String, int> morse_to_HID =
{
{".-", HID_KEY_A},
{"-...", HID_KEY_B},
{"-.-.", HID_KEY_C},
{"-..", HID_KEY_D},
{".", HID_KEY_E},
{"..-.", HID_KEY_F},
{"--.", HID_KEY_G},
{"....", HID_KEY_H},
{"..", HID_KEY_I},
{".---", HID_KEY_J},
{"-.-", HID_KEY_K},
{".-..", HID_KEY_L},
{"--", HID_KEY_M},
{"-.", HID_KEY_N},
{"---", HID_KEY_O},
{".--.", HID_KEY_P},
{"--.-", HID_KEY_Q},
{".-.", HID_KEY_R},
{"...", HID_KEY_S},
{"-", HID_KEY_T},
{"..-", HID_KEY_U},
{"...-", HID_KEY_V},
{".--", HID_KEY_W},
{"-..-", HID_KEY_X},
{"-.--", HID_KEY_Y},
{"--..", HID_KEY_Z},
{".----", HID_KEY_1},
{"..---", HID_KEY_2},
{"...--", HID_KEY_3},
{"....-", HID_KEY_4},
{".....", HID_KEY_5},
{"-....", HID_KEY_6},
{"--...", HID_KEY_7},
{"---..", HID_KEY_8},
{"----.", HID_KEY_9},
{"-----", HID_KEY_0},

{"..--", HID_KEY_SPACE},
{".-.-", HID_KEY_ENTER},

{".-.-.-", HID_KEY_PERIOD},
{"--..--", HID_KEY_COMMA},
{"..--..", HID_KEY_EUROPE_1},
{".----.", HID_KEY_APOSTROPHE},
{"-..-.", HID_KEY_SLASH},
{"........", HID_KEY_BACKSPACE},
/*
{"-....", HID_KEY_6},
{"--...", HID_KEY_7},
{"---..", HID_KEY_8},
{"----.", HID_KEY_9},
{"-----", HID_KEY_0},
*/
};

/*
    ???['!'] = "-.-.--",
    ['/'] = "-..-.",
    ['('] = "-.--.",
    [')'] = "-.--.-",
    ['&'] = ".-...",
    [':'] = "---...",
    [';'] = "-.-.-.",
    ['='] = "-...-",
    ['+'] = ".-.-.",
    ['-'] = "-....-",
    ['_'] = "..--.-",
    ['\\'] = ".-..-.",
    ['$'] = "...-..-",
    ['@'] = ".--.-.",
*/
/*
std::map< String, char > morse_to_ascii =
{
{".-", 'A'},
{"-...", 'B'},
{"-.-.", 'C'},
{"-..", 'D'},
{".", 'E'},
{"..-.", 'F'},
{"--.", 'G'},
{"....", 'H'},
{"..", 'I'},
{".---", 'J'},
{"-.-", 'K'},
{".-..", 'L'},
{"--", 'M'},
{"-.", 'N'},
{"---", 'O'},
{".--.", 'P'},
{"--.-", 'Q'},
{".-.", 'R'},
{"...", 'S'},
{"-", 'T'},
{"..-", 'U'},
{"...-", 'V'},
{".--", 'W'},
{"-..-", 'X'},
{"-.--", 'Y'},
{"--..", 'Z'},
{".----", '1'},
{"..---", '2'},
{"...--", '3'},
{"....-", '4'},
{".....", '5'},
{"-....", '6'},
{"--...", '7'},
{"---..", '8'},
{"----.", '9'},
{"-----", '0'},
/*
{".-.-.-", '.'},
{"--..--", ','},
{"..--..", '?'},
{"-...-", '='}

};
*/

void setup() {

  //Open HID Keyboard
  usb_hid.setBootProtocol(HID_ITF_PROTOCOL_KEYBOARD);
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("Morse TrinKeyboard");
  usb_hid.begin();

  // put your setup code here, to run once:
  Serial.begin(115200);
  //while (!Serial) { yield(); delay(10); }     // wait till serial port is opened

  //Open NeoPixel
  pixel.begin();
  pixel.setBrightness(10);
  pixel.show();

  // Report initialization
  /*
  Serial.println("");
  Serial.println("Restarted");
  Serial.println("Serial port opened");
  Serial.print("Words Per Minute: ");
  Serial.println(wpm);
  Serial.print("dit interval: ");
  Serial.println(dit_interval);
  Serial.print("dah interval: ");
  Serial.println(dah_interval);
*/
//  Serial.print("Test map: ");
//  Serial.println(morse_to_ascii[".--."]);

  // Initialize input pins
  pinMode(DITPIN, INPUT_PULLUP);
  pinMode(DAHPIN, INPUT_PULLUP);
  // Initialize Sidetone pin
  pinMode(SIDETONEPIN, OUTPUT);
  digitalWrite(SIDETONEPIN, LOW);
  // set the 'ground' pin - Sleeve
  pinMode(PIN_SLEEVE, OUTPUT);
  digitalWrite(PIN_SLEEVE, LOW);

//  Attach Interrupts to our pins
  attachInterrupt(digitalPinToInterrupt(DITPIN), ISR_dit, INTERRUPT_MODE);
  attachInterrupt(digitalPinToInterrupt(DAHPIN), ISR_dah, INTERRUPT_MODE);

//  Sidetone Test
  toneTest();
}

void loop() {
  if (DIT_FLAG == 1) {
    addToQueue('.');
//    Serial.println("DIT_FLAG Triggered");
  //  Start blink and tone
    pixel.setPixelColor(0, 0xFF0000);
    pixel.show();
    tone(SIDETONEPIN, 1000, dit_interval);
    delay(dit_interval);
    pixel.setPixelColor(0, 0x000000);
    pixel.show();
  //  Stop neopixel and tone
    delay(dit_interval);
    DIT_FLAG=0;
  }

  if (DAH_FLAG == 1) {
    addToQueue('-');
//    Serial.println("DAH_FLAG Triggered");
  //  Start blink and tone
    pixel.setPixelColor(0, 0x0000FF);
    pixel.show();
    tone(SIDETONEPIN, 1000, dah_interval);
    delay(dah_interval);
    pixel.setPixelColor(0, 0x000000);
    pixel.show();
  //  Stop neopixel and tone
    delay(dit_interval);
    DAH_FLAG=0;
  }

  //  Check to see if paddle is held and if it goes beyond dit_interval retrigger
  if ((digitalRead(DITPIN)==LOW) && ((holdCounter + dit_interval) < millis())) {
    ISR_dit();
//    Serial.println("DIT RETRIGGERED");
  }
  //  Check to see if paddle is held and if it goes beyond dah_interval retrigger
  if ((digitalRead(DAHPIN)==LOW) && ((holdCounter + dah_interval) < millis())) {
    ISR_dah();
//    Serial.println("DAH RETRIGGERED");
  }

  if ( TinyUSBDevice.suspended() ) {
    TinyUSBDevice.remoteWakeup();
  }

//  The meat - if letter timeout reached and elements are in the queue dump it!
  if (((letter_interval + holdCounter) < millis()) && (qTail > -1)) {
    outputCode=dumpQueue();
//    Serial.println(morse_to_ascii[outputCode]);

    usb_hid.keyboardRelease(0);
    //required or else it will switch faster than can be reported
    delay(5);
    keycode[0] = morse_to_HID[outputCode];
    usb_hid.keyboardReport(report_id, modifier, keycode);
    //required or else it will switch faster than can be reported
    delay(5);
    usb_hid.keyboardRelease(0);
  }
}

//  DIT Interrupt function
void ISR_dit() {
  //  prevent interrupt while we are interrupted
  detachInterrupt(digitalPinToInterrupt(DITPIN));
  //  If button is pressed outside of last button press plus debounce timeout
  if ((holdCounter + debounceTime) < millis()) {
    // Tell loop that it was a dit
    DIT_FLAG=1;
    //reset the last time a button was pressed timer
    holdCounter=millis();
  }
  // reattach interrupt
  attachInterrupt(digitalPinToInterrupt(DITPIN), ISR_dit, INTERRUPT_MODE);
}

//  DAH Interrupt function
void ISR_dah() {
  //  prevent interrupt while we are interrupted
  detachInterrupt(digitalPinToInterrupt(DAHPIN));
  //  If button is pressed outside of last button press plus debounce timeout
  if ((holdCounter + debounceTime) < millis()) {
    // Tell loop that it was a dah
    DAH_FLAG=1;
    //reset the last time a button was pressed timer
    holdCounter=millis();
  }
  // reattach interrupt
  attachInterrupt(digitalPinToInterrupt(DAHPIN), ISR_dah, INTERRUPT_MODE);
}

void toneTest() {
  tone(SIDETONEPIN, 1000, dah_interval);
  delay(dah_interval + dit_interval);

  tone(SIDETONEPIN, 1000, dit_interval);
  delay(dit_interval + dit_interval);

  tone(SIDETONEPIN, 1000, dah_interval);
  delay(dah_interval + dit_interval);

  tone(SIDETONEPIN, 1000, dit_interval);
  delay(dit_interval + dit_interval);

  delay(letter_interval);

  tone(SIDETONEPIN, 1000, dah_interval);
  delay(dah_interval + dit_interval);

  tone(SIDETONEPIN, 1000, dah_interval);
  delay(dah_interval + dit_interval);

  tone(SIDETONEPIN, 1000, dit_interval);
  delay(dit_interval + dit_interval);

  tone(SIDETONEPIN, 1000, dah_interval);
  delay(dah_interval + dit_interval);

}

String dumpQueue() {
  String currString = String("");
  int tailLength = qTail;
//  Serial.print("qTail=");
//  Serial.println(qTail);
  if(qTail > -1 ) {
    for(int i=0; i <= tailLength; i++ ) {
      currString.concat(popQueue());
//  Serial.print("qTail=");
//  Serial.println(qTail);
    }
  }
//  Serial.print("dumpQueue=");
//  Serial.println(currString);
  return currString;
}

char popQueue() {
  char headValue = '\0';
//  if (qTail > -1) {
  if (qTail > -1) {
    headValue = letterQueue[0];
    shiftArrayLeft();
  }
  return headValue;
}

void shiftArrayLeft() {
//  for(int i=1; i < MAXQ; i++) {
  for(int i=0; i <= qTail; i++) {
    letterQueue[i] = letterQueue[(i+1)];
  }
  qTail--;
}

void addToQueue (char tailValue){
  if (qTail < MAXQ) {
    qTail++;
  }
  letterQueue[qTail] = tailValue;
}