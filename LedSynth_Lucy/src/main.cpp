/*
TODO:
- store iso pwms in flash and load them to tlc
- store mask for diodes and load them to tlc
- EEPROM stores 8bit data, so the PWM values have to be saved in 2 slots each and ch mask to 4 each

- decide once and for all if we start with 0 or 1 for first LED and correct in Lucy.h (Discrepabcy between code 0 and user interface 1)

- implement GALOIS

- Make a new calibration protocol that tests PWM, DC and BC and ajusts in DC-(BC)-PWM order
- Make DC and BC reachable for the user via the command line

- Make a GUI that will have PWM, DC and BC values and updates them at connecting

- think about implementing function generator ie SIN at some freq. What would the update freq be? Check it.
  - this would need the mode that runs the PWM cycle only once and then update (will the light flicker?)
*/

#include <Arduino.h>
#include <TLC5948.h>
#include <EEPROM.h>
#include "Lucy.h"
#include "easter.h"

String input = "help";   //used for storing incoming strings, set to prot to go into ProtocolBuilder on startup
String command = ""; // used to store the command that the user sends via Serial port (empty at Init)

bool change         = true;
bool waitForTrigIn  = false;
bool sendTrigOut    = false;
bool protActive     = false;
bool trigReceived   = false;

uint16_t p_ofs   = 500;  // Offset, duration, pause and wait times [ms] for protocol stimulation
uint16_t p_dur   = 50;
uint16_t p_pau   = 147;
uint16_t p_wait  = 107;

float    genAtt     = 0.0;  // General and adaptational atenuation [log]
float    adapAtt    = 0.0;
float    v_f        = -3.6; // V-logI <f>rom, <s>tep and <t>o values [log]
float    v_s        = 0.3;
float    v_t        = 0.0;

const int EEPROM_ADD_START = 0;
const int EEPROM_ADD_STEP = sizeof(uint32_t);

TLC5948 tlc(D_nTLCs, D_NLS, CHmask);

// FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS
// FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS
// FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS
void EEPROMsave() {
  for( int i = 0; i < tlc.nLEDs; i++) {
    EEPROM[i]               = CHmask[i];
    EEPROM[i + 1*tlc.nLEDs] = pwmVals[i];
    EEPROM[i + 2*tlc.nLEDs] = isoDC[i];
  }
  for( int i = 0; i < tlc.nTLCs; i++) {
    EEPROM[i + 3*tlc.nLEDs] = isoBC[i];
  }
}

void EEPROMload() {
  for( int i = 0; i < tlc.nLEDs; i++) {
    CHmask[i]  = EEPROM[i];
    pwmVals[i] = EEPROM[i + 1*tlc.nLEDs];
    isoDC[i]   = EEPROM[i + 2*tlc.nLEDs];
  }
  for( int i = 0; i < tlc.nTLCs; i++) {
    isoBC[i] = EEPROM[i + 3*tlc.nLEDs];
  }
}

void EEPROMpeek() {
  // print out mask, pwmVals, isodc and isobc
  Serial.println("Peek at stored PWM values, use pwmload to load them to the program.");
  Serial.println("LED PWM   DC  BC");
  for( int i = 0; i < tlc.nLEDs; i++) {
    Serial.printf("%3d %5d %3d %3d\n", i, EEPROM[i + 1*tlc.nLEDs], EEPROM[i + 2*tlc.nLEDs], EEPROM[i + 3*tlc.nLEDs]);
  }
  Serial.println("-----------------");
}

void envelope(bool state) {  digitalWrite(ENVELOPEPIN, state); }
void trigInISR() { trigReceived = true; }
void trigOut(uint8_t trigPin, uint16_t dur) { digitalWrite(trigPin, HIGH);  delay(dur);  digitalWrite(trigPin, LOW); }

// TODO: remove
void readPWMsFromSerial(String input, int lenOfCommand) {
  int stepSize = 5;
  Serial.println("Changing PWM.");
  for (uint16_t i = lenOfCommand; i < input.length(); i+=stepSize){
    uint8_t led = (i-(lenOfCommand))/stepSize;
    pwmVals[led] = constrain(input.substring(i, i+stepSize).toInt(), 0, tlc.MAX_PWM);
    Serial.println("  LED: " + String(led+1) + " at PWM: " + String(pwmVals[led]));
  }
  Serial.println("Done");
}

void getPwm(){                                                                    // TODO: change to report log values, or delete
  Serial.println("-- PwmVals for all LEDs --------------------------");
  Serial.println("   Format: index/driver/channel/pwm");
  for (uint8_t i = 0; i < D_NLS; i++) {
    Serial.printf("  %i %i\n\r", i, pwmVals[i]);
  }
  Serial.println("--------------------------------------------------");
}

void set(uint8_t led, int32_t pwmVal) {                                               // TODO: consider removing; this is for now used by protocols
  // TODO constrain should be addressed in the library only?
  // led    = constrain(led,      0,   tlc.nLEDs);       
  // pwmVal = constrain(pwmVal,   0,   tlc.MAX_PWM);
  /* if (pwmVal < 0) {
    pwmVal = pwmVals[led] >> (-pwmVal-1) ;   
    // use ISO values: divide by 1 for -1, divide by 2 for -2, by 4 for -3, ...
  }
  */
  tlc.setpwm(led, pwmVal, "P");
  tlc.setpwm(led, isoDC[led], "D");
  tlc.update();                                                                         
}

void setPwm(uint8_t led, int32_t pwmVal) {
  tlc.setpwm(led, pwmVal, "P");
  tlc.update();                                                                         // TODO: consider tlc.update outside of set? --- CAN BE DONE AT THE END OF THE PARSE
}

void setDc(uint8_t led, int32_t dcVal) {
  tlc.setpwm(led, dcVal, "D");
  tlc.update();                                                                         // TODO: consider tlc.update outside of set? --- CAN BE DONE AT THE END OF THE PARSE
}

void setCent(uint8_t led, int32_t centVal) {
  float fpwm = (float)(constrain(centVal, -100, 999))/100.0;
  tlc.setlog(led, fpwm);                   // constrain?
  tlc.update();                                                                         // TODO: consider tlc.update outside of set? --- CAN BE DONE AT THE END OF THE PARSE
}

void setLog(uint8_t led, float logVal) {
  tlc.setlog(led, logVal);                // constrain?
  tlc.update();                                                                         // TODO: consider tlc.update outside of set? --- CAN BE DONE AT THE END OF THE PARSE
}

void setRainbow (int32_t pwmVal) {                                                      // name used by Protocols, should be setRainbowPWM
  for (uint8_t i = 0; i < D_NLS; i++){
    tlc.setpwm(i, pwmVal, "P");
  }
  tlc.update() ;
}

void setRainbowDc (int32_t dcVal) {       
  for (uint8_t i = 0; i < D_NLS; i++){
    tlc.setpwm(i, dcVal, "D");
  }
  tlc.update() ;
}

void setRainbowBc (int32_t dcVal) {       
  for (uint8_t i = 0; i < D_nTLCs; i++){
    tlc.setpwm(i, dcVal, "B");
  }
  tlc.update() ;
}

void setRainbowCent(int32_t centVal) {                                                  // TODO: missing 'alllog" command
  float fpwm = (float)(constrain(centVal, -100, 999))/100.0;
  for (uint8_t i = 0; i < D_NLS; i++){
    tlc.setlog(i, fpwm);
  }
  tlc.update() ;
}

void setOe(bool state) {
  if (state) {    tlc.C_BLANK = 0;  }
  else       {    tlc.C_BLANK = 1;  }
  tlc.update();
}

void mainWelcome() {
  Serial.println(TextLucy) ;
  for (int i=0; i<D_NLS; i++) {
    Serial.printf ("Led %02i (%c) @ %03i nm\n", i, i+'A', lambdas [i] ) ;
  }
  Serial.println( "\n" LEDSYNTHNAME " (built @ " __DATE__ " " __TIME__") has " + String(D_NLS) + " LEDs.\n");
}

void mainHelp() {
  Serial.println(TextHelp) ;
}


// PROTOCOLS PROTOCOLS PROTOCOLS

// #include "maintext.h"
#include "protocol.h"

// SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP
// SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP M50FIXPWMSETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP
// SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.setTimeout(SERIAL_TIMEOUT);

  tlc.begin();

  pinMode(TRIGOUTPIN, OUTPUT);
  pinMode(ENVELOPEPIN, OUTPUT);
  pinMode(INFOPIN, OUTPUT);         // TODO: CHECK CONNECTION
  pinMode(TRIGINPIN, INPUT_PULLUP); // PULLUP working normally without need for *register stuff: https://forum.pjrc.com/threads/48058-Teensy-3-2-internal-pulldown-and-pullup-resistors
  attachInterrupt(digitalPinToInterrupt(TRIGINPIN), trigInISR, RISING); // interrupt routine to catch the trigIn flag
  //EEPROMsave();
  //EEPROMload();
  mainWelcome();
}

// LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP
// LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP
// LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP
void loop() {
  if (Serial.available() > 0) {
    input = Serial.readStringUntil('*');
    change = true;
  }

  if (change) {
    //----------------------------------------------------------------------------------------------------------------------------- Single line protocols
    if (input.substring(0, 4) == "help")          mainHelp();
    if (input.substring(0, 4) == "welc")          mainWelcome();
    if (input.substring(0, 4) == "prot")          protocolEnvironment();
    if (input.substring(0, 5) == "hello")         mainWelcome();
    if (input.substring(0, 5) == "reset")         WRITE_RESTART(0x5FA0004);

    if (input.substring(0, 7) == "version")       plotEaster(TextSignature, 1, TextSignature_h, TextSignature_w);                             // ver?
    if (input.substring(0, 6) == "sensei")        plotEaster(easter, 1, easter_h, easter_w);

    if (input.substring(0, 5) == "espwm")     {   tlc.C_ESPWM = constrain(input.substring(5).toInt(),0,1) ; 
                                                  tlc.C_ESPWM ? Serial.println("mode ESPWM") : Serial.println("mode PWM");} 
    if (input.substring(0, 5) == "debug")         tlc.debugTLCflag = !tlc.debugTLCflag;
    if (input.substring(0, 5) == "frpri")         tlc.printFramesTLCflag = !tlc.printFramesTLCflag;


    if (input.substring(0, 5) == "print")         tlc.printCPWM();
    if (input.substring(0, 4) == "mask")          tlc.printMask();

    if ((input[0]>='A') & (input[0]<='Z'))    {   LED.curr = constrain ( (input[0]-'A'),0,D_NLS) ; 
                                                  Serial.printf("Led%02i ",LED.curr); }
    if (input.substring(0, 3) == "led")       {   LED.curr = constrain (input.substring(3,5).toInt(),0,D_NLS) ; 
                                                  Serial.printf("Led%02i ",LED.curr);}

    if (input.substring(0,4)  == "cent") {
      float valLog = (input.substring(4).toFloat())/100.0 ;
      Serial.printf("@log %1.3f\n",valLog);                                
      setLog (LED.curr,valLog) ;
    }

    if (input.substring(0,3)  == "log") {
      float valLog = input.substring(3).toFloat() ;
      Serial.printf("@log %1.3f\n",valLog);                                
      setLog (LED.curr,valLog) ;
    }

    if (input.substring(0,3)  == "pwm") {
      uint32_t valPwm = input.substring(3).toInt() ;
      Serial.printf("@pwm %5i (0x%04x)\n",valPwm,valPwm);                                
      setPwm (LED.curr,valPwm) ;
    }

    if (input.substring(0,3)  == "oct") {
      uint32_t valPwm = pow (2, 16 - constrain(input.substring(3).toFloat(),0,17) ) ;
      Serial.printf("@pwm %5i (0x%04x)\n",valPwm,valPwm);                                
      setPwm (LED.curr,valPwm) ;
    }

    if (input.substring(0,2)  == "dc") {
      uint32_t valDc = input.substring(2).toInt() ;
      Serial.printf("@dc %3i (0x%3x)\n",valDc,valDc);                                
      setDc (LED.curr,valDc) ;
    }

    if (input.substring(0,3)  == "off") {
      Serial.println("@off ");
      setPwm (LED.curr,0) ;
      // setDc  (LED.curr,0) ;
    }

    if (input.substring(0,4)  == "full") {
      Serial.println("@full ");
      setPwm (LED.curr,65535) ;
      setDc  (LED.curr,127) ;
    }

    if (input.substring(0,4)  == "half") {
      Serial.println("@half ");
      setPwm (LED.curr,32768) ;
      setDc  (LED.curr,127) ;
    }


    if (input.substring (0, 2) == "oe") {
      int tstate = input.substring(2, 3).toInt();
      setOe( tstate );
      tstate ? Serial.println("Output: on") : Serial.println("Output: off");
    }

    // old, standard set command for PWM
    if (input.substring(0, 3) == "set") {
      uint8_t homeSetLed = input.substring(3, 5).toInt();
      int32_t homeSetVal = input.substring(5).toInt();
      set ( homeSetLed, homeSetVal );
      Serial.println("Set LED " + String(homeSetLed) + "@ PWM: " + String(homeSetVal));
    }

    if (input.substring(0, 6) == "allpwm") {
        int32_t allPwm = input.substring(6).toInt();
        setRainbow (allPwm);
        Serial.printf("Rainbow set to PWM %i\n", allPwm);
    }

if (input.substring(0, 6) == "alloff") {
        setRainbow (0);
        Serial.printf("Rainbow off %i\n");
    }

    if (input.substring(0, 5) == "alldc") {
        int32_t allDc = input.substring(5).toInt();
        setRainbowDc (allDc);
        Serial.printf("Rainbow set to DC %i\n",allDc);
    }

    if (input.substring(0, 5) == "allbc") {
        int32_t allBc = input.substring(5).toInt();
        setRainbowDc (allBc);
        Serial.printf("Rainbow set to BC %i\n",allBc);
    }


    if (input.substring(0, 7) == "allcent") {
        int32_t allCent = input.substring(7).toInt();
        setRainbowCent(allCent);
        Serial.printf("Rainbow set to log %1.3f\n", ((float)allCent)/100.0);
      }

    if (input.substring(0, 3) == "dir") {
      uint8_t  dir_setCh  = input.substring( 3,  5).toInt();
      uint8_t  dir_setDr  = input.substring( 5,  7).toInt();
      uint16_t dir_setPWM = input.substring( 7, 12).toInt();
      uint8_t  dir_setDC  = input.substring(12, 15).toInt();
      uint8_t  dir_setBC  = input.substring(15    ).toInt();
      tlc.setChannel( dir_setCh, dir_setDr, dir_setPWM, dir_setDC, dir_setBC );
    }

    if (input.substring(0, 3) == "iso") {
      if (     input.substring(3, 6) == "get" ) {
        getPwm();
      }
      else if (input.substring(3, 7) == "save") {
        //EEPROMsave();
      }
      else if (input.substring(3, 7) == "load") {
        //EEPROMload();
      }
      else if (input.substring(3, 7) == "peek") {
        //EEPROMpeek();
      }
      else {
        readPWMsFromSerial( input, 3 );
      }
    }

    change = false;
  }
}
