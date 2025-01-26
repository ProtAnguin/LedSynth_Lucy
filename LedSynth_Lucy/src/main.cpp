/*

NOTE: USING ',' COMMA INSTEAD OF ASTERISK AS THE DELIMITER, IT IS MORE EASY TO DO
NOTE: USING '!' for update

1 PP HIGH PRIORITY implement PWM frequency change command, and report the frequency to the user

2 MI HIGH PRIORITY Make a new calibration protocol 
  PP ELECTRICAL AND OPTICAL CORRECTION SHOULD BECOME SEPARATE!!!!
  
  PP CORRECTION OF ELECTRICAL CHARACTERISTICS
     PWL piecewise linear interpolation is the way to go!
     store crrection for each LED as a list of float doublets (desired log,command log).
     for a LED "piflar" you need the 2 logi doublets.
     for a LED with a single "knee" you need 3 doublets
     for a LED both with a low knee and droop you need 4 doublets and more is unnecessary.
     in the first instance, the calculation may be done outside (matlab, octave, python?), after catching the output of OPT4048 values.
     the list of logi doublets should be stored to teensy with a command similar to pwmset (just use commas between the values, because float!)
     PLEASE REFRAIN FROM USING POLYNOMIALS!!!

  PP CORRECTION OF OPTICAL CHARACTERISTICS (ISOBANKS)
     a list of "iso" (log) values (one value per LED) stores isoquantisation or whichever quantisation
     an iso offset should be added separately
     
3 MI EEPROM store 
  PP the storage of "ELECTRICAL" correction and "OPTICAL" correction will be easier
     using structs and memcopy . check demo code for EEPROM?

4 MI decide once and for all if we start with 0 or 1 for first LED and correct in Lucy.h (Discrepabcy between code 0 and user interface 1)
  PP We start with 00. It should be used for the zero order white LED (if it is not connected, then it should still be there in the code :-)

5 PP tlc latch should be routed to a monitor pin or one of the 4 BNCs (in basic mode at least)

6 MI store mask for diodes and load them to tlc
  PP MASKS: if we have LED.mask (instead / with LED.curr and LED.all ), 
     then a command with several capital letters, e.g. 'ABCD' could select several LEDs

7 MI think about implementing function generator ie SIN at some freq. What would the update freq be? Check it.
     this would need the mode that runs the PWM cycle only once and then update (will the light flicker?)
  PP low priority 
  PP for correct update time TIMER library could be used. 
  PP routing a dedicated channel on PWM = 1 (operated from 3.3V) to a pinb with an attached interrupt

8 PP make protocol builder into a separate c-file / library

9 PP I would very much like to see a different parser implementation that would not use all these asterisks

10 MI Make DC and BC reachable for the user via the command line
  PP both reachable, BC implementation iffy

11 PP make echo on / off command so that no inputs are coming back if one deserves so (like zero debug or so!!!)

12 PP extend parsing PWM values to >65536, might be useful for those LEDs with several channels, and it is linear anyway :-)

*/

#include <Arduino.h>
#include <TLC5948.h>
#include <EEPROM.h>
#include "Lucy.h"
#include "easter.h"

String input = "help";   //used for storing incoming strings, set to <prot> to go into ProtocolBuilder on startup
String command = ""; // used to store the command that the user sends via Serial port (empty at Init)

TLC5948 tlc(D_nTLCs, D_NLS, CHmask);

bool trigReceived   = false;
void trigInISR() {
  trigReceived = true;
}

void trigOut(uint8_t trigPin, uint16_t dur) {
  digitalWrite(trigPin, HIGH);
  delay(dur);
  digitalWrite(trigPin, LOW);
}

void envelope(bool state) {
  digitalWrite(ENVELOPEPIN, state);
}

void update() {
  digitalWrite(INFOPIN,1) ; 
  tlc.update() ; 
  digitalWrite(INFOPIN,0) ; 
  // Serial.print("!") ; 
}

void waitTrigUpdate(uint32_t howlong) {
  uint32_t endMillis = millis() + howlong;
  trigReceived = false ;
  while( (millis()<endMillis) || trigReceived) {};
  update();
  trigReceived = false ;
}

// UPDATE ON TRIGGER: should we have trigReceived at the end of the input parsing loop ???

void setLog() {
  if (LED.all)        { for  (int i = 1 ; i<=D_NLS ; i++) { tlc.setlog(       i, LED.logVal); } }
  else                                                    {  tlc.setlog(LED.curr, LED.logVal); }
  #ifdef TLCUPDATEFORCE
  update();
  #endif
  // Serial.printf("setLog All=%i Led=%02i LOG %1.3f\n",LED.all,LED.curr,LED.logVal) ;
}

void setPwm() {
  if (LED.all)        { for  (int i = 1 ; i<=D_NLS ; i++) { tlc.change('P',        i, LED.pwmVal); } }
  else                                                    { tlc.change('P', LED.curr, LED.pwmVal); }
  #ifdef TLCUPDATEFORCE
  update();                                                                         
  #endif
  // Serial.printf("setPwm All=%i Led=%02i PWM %05i\n",LED.all,LED.curr,LED.pwmVal) ;
}

void setDc() {
  if (LED.all)        { for  (int i = 1 ; i<=D_NLS ; i++) { tlc.change('D',        i, LED.dcVal); } }
  else                                                    { tlc.change('D', LED.curr, LED.dcVal); }
  #ifdef TLCUPDATEFORCE
  update();                                                                         
  #endif
  // Serial.printf("setDc All=%i Led=%02i DC %03i\n",LED.all,LED.curr,LED.dcVal) ;
}

void setBc() {
  if (LED.all)        { for  (int i = 1 ; i<=D_NLS ; i++) { tlc.change('B',        i, LED.bcVal); } }
  else                                                    { tlc.change('B', LED.curr, LED.bcVal); }
  #ifdef TLCUPDATEFORCE
  update();                                                                         
  #endif
  // Serial.printf("setDc All=%i Led=%02i DC %03i\n",LED.all,LED.curr,LED.dcVal) ;
}

void setOe(bool state) {
  if (state) { tlc.C_BLANK = 0; }
  else       { tlc.C_BLANK = 1; }
  update();
}

// very basic and not well-thought or tested implementation to store the "optical" iso value for the currently selected LED at the current log value
// TODO: check that we dont mess a write outside the array!, implement copying from current isoLog to a bank
void storeIso() {
    Serial.printf("Store Led%02i iso log %1.3f ",LED.curr,LED.logVal) ;
    if (LED.all)
      Serial.println("aborted: all leds on, switch to one first") ;
    else if ((LED.curr<0) || (LED.curr>D_NLS))
      Serial.println("aborted: current LED number is impossible") ;
    else if ((LED.logVal < 0) || (LED.logVal > 4 ))
      Serial.println("aborted: log value out of range [0 to 4]") ;            
    else {
      Serial.println("success") ; 
      isoLog[isoLogCurr][LED.curr] = LED.logVal;
    }
}

void mainHelp() { Serial.println(TextHelp) ; }
void mainWelcome() {
  Serial.println(TextLucy) ;
  Serial.println( "\n" LEDSYNTHNAME " (built @ " __DATE__ " " __TIME__") has " + String(D_NLS) + " LEDs.\n");
}
void mainPrintLeds() {
  for (int i=0; i<D_NLS; i++) {
    Serial.printf ("Led %02i %03i nm log %-7.3f \n",
    i,
    lambdas[i],
    isoLog[isoLogCurr][i] );
  }
} // this will also print the 'electric' PWL values 

// PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS
// PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS
// PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS PROTOCOLS
#include "protocol.h"                                   // TODO this is a part of protocol, consider making an object / library for the protocol builder

// SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP
// SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP
// SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.setTimeout(SERIAL_TIMEOUT);

  tlc.begin();                            // TODO: implement a frequency change command!!!!!!!!!!!

  pinMode(TRIGOUTPIN,   OUTPUT);
  pinMode(ENVELOPEPIN,  OUTPUT);
  pinMode(INFOPIN,      OUTPUT);               
  pinMode(TRIGINPIN,    INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(TRIGINPIN), trigInISR, RISING); // interrupt routine to catch the trigIn flag

  //EEPROMsave();
  //EEPROMload();

  mainWelcome();
}

// LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP
// LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP
// LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP
void loop() {
  if (Serial.available() > 0) {
    input = Serial.readStringUntil(',');
    change = true;
  }

  if (trigReceived == true) update() ;              // update on external trigger ???

  // TODO: implement ECHO on / off to switch all printf commands off if needed!

  if (change) {
    //----------------------------------------------------------------------------------------------------------------------------- Single line protocols
    // shouldnt this be "else ifs??????"
    if (     input.substring(0, 5) == "reset")         WRITE_RESTART(0x5FA0004);
    else if (input.substring(0, 5) == "hello")         mainWelcome();
    else if (input.substring(0, 4) == "help")          mainHelp();
    else if (input.substring(0, 4) == "leds")          mainPrintLeds();
    else if (input.substring(0, 4) == "prot")          protocolEnvironment();
    else if (input.substring(0, 7) == "version")       plotEaster(TextSignature, 1, TextSignature_h, TextSignature_w);       
    else if (input.substring(0, 6) == "sensei")        plotEaster(easter, 1, easter_h, easter_w);
    else if (input.substring(0, 6) == "debug1")        tlc.debugTLCflag = !tlc.debugTLCflag;
    else if (input.substring(0, 6) == "debug2")        tlc.printFramesTLCflag = !tlc.printFramesTLCflag;
    else if (input.substring(0, 5) == "print")         tlc.printCPWM();
    else if (input.substring(0, 5) == "frame")         tlc.printFrames();
    else if (input.substring(0, 6) == "wiring")        tlc.printMask();

    else if (input.substring(0, 5) == "espwm")     {   tlc.C_ESPWM  = constrain(input.substring(5).toInt(),0,1) ; 
                                                       tlc.C_ESPWM  ? Serial.println("mode ESPWM") : Serial.println("mode PWM");}

    else if (input.substring(0, 5) == "blank")     {   tlc.C_BLANK  = constrain(input.substring(5).toInt(),0,1) ; 
                                                       tlc.C_ESPWM  ? Serial.println("blanking on") : Serial.println("blanking off");} 

    else if (input.substring(0, 6) == "tmgrst")    {   tlc.C_TMGRST = constrain(input.substring(6).toInt(),0,1) ; 
                                                       tlc.C_TMGRST ? Serial.println("timing reset on") : Serial.println("timing reset off");} 

    else if (input.substring(0, 6) == "dsprpt")    {   tlc.C_DSPRPT = constrain(input.substring(6).toInt(),0,1) ; 
                                                       tlc.C_DSPRPT ? Serial.println("display repeat on") : Serial.println("display repeat off");} 

    else if (input.substring(0, 5) == "gsclk")     {   Serial.printf ("GSCLK set to %f MHz\n", tlc.setGSCLK (input.substring(5).toFloat()) ) ; }

    else if (input.substring(0, 2) == "oe")        {   setOe( 1 ); Serial.println("output on" ); }
    else if (input.substring(0, 2) == "od")        {   setOe( 0 ); Serial.println("output off"); }

    else if (input.substring(0, 1) == "!")            update() ;     // the most important command
    else if (input.substring(0, 1) == "?")         {  waitTrigUpdate(100) ; } // TODO: do we need this?
    else if (input.substring(0, 4) == "wait")      {  waitTrigUpdate(constrain(input.substring(4).toInt(),0,MAXWAITMS)) ; }
    
    else if (input.substring(0, 3) == "tic")          delay(7) ;     // this is about 1 frame at 150 Hz, TODO should not be hard-coded
    else if (input.substring(0, 3) == "toc")          delay(40);     // this is about 6 frames at 150 Hz, TODO should not be hard-coded
    else if (input.substring(0, 3) == "del")          delay(constrain(input.substring(3).toInt(),0,1000));
    
    else if (input.substring(0, 4) == "trig")         trigOut(TRIGOUTPIN,1);

    else if ((input[0]>='A') & (input[0]<='Z'))    {   LED.curr = constrain (((input[0]=='Z' ? 0 : input[0]-LED00CHAR)),0,D_NLS);
                                                       LED.all  = false;
                                                       Serial.printf("Led%02i ",LED.curr); }
    else if (input.substring(0, 3) == "led")       {   LED.curr = constrain (input.substring(3,5).toInt(),0,D_NLS) ; 
                                                       LED.all  = false;
                                                       Serial.printf("Led%02i ",LED.curr);
                                                   }
    else if (input.substring(0, 3) == "one")       {   LED.all = false;
                                                       Serial.printf("Led%02i ",LED.curr);
                                                   }
    else if (input.substring(0, 3) == "all")       {   LED.all = true;
                                                       Serial.printf("Rainbow ");
                                                   }
    else if (input.substring(0,3)  == "log")       {   LED.logVal = input.substring(3).toFloat() ;
                                                       Serial.printf("@log %1.3f\n",LED.logVal);                                
                                                       setLog();
                                                   }
    else if (input.substring(0,3)  == "pwm")       {   LED.pwmVal = input.substring(3).toInt() ;
                                                       Serial.printf("@pwm %5i (0x%04x)\n",LED.pwmVal,LED.pwmVal);                                
                                                       setPwm();
                                                   }
    else if (input.substring(0,3)  == "oct")       {  LED.pwmVal = constrain (pow (2, 16-input.substring(3).toFloat()), 0,tlc.MAX_PWM) ;
                                                      Serial.printf("@pwm %5i (0x%04x)\n",LED.pwmVal,LED.pwmVal);
                                                      setPwm();
                                                   }
    else if (input.substring(0,2)  == "dc")        {  LED.dcVal = input.substring(2).toInt() ;
                                                      Serial.printf("@dc %3i (0x%03x)\n",LED.dcVal,LED.dcVal);                                
                                                      setDc();
                                                   }
    else if (input.substring(0, 2) == "bc")        {  LED.bcVal = input.substring(2).toInt() ;  
                                                      Serial.printf("@bc %3i (0x%03x)\n",LED.bcVal,LED.bcVal);                                
                                                      setBc() ;
                                                   }    
    else if (input.substring(0,4)  == "full")      {  LED.pwmVal = tlc.MAX_PWM ; LED.dcVal = tlc.MAX_DC; LED.bcVal = tlc.MAX_BC;
                                                      Serial.println("@full ");
                                                      setDc(); setBc(); setPwm();
                                                   }
    else if (input.substring(0,3)  == "off")       {  LED.pwmVal = 0 ;
                                                      Serial.println("@off\n");
                                                      setPwm();
                                                   }       
    else if (input.substring(0, 5) == "store")     {  storeIso() ; } // iffy
    else if (input.substring(0, 6) == "update")    {  Serial.println("update not implemented yet.") ; }     // TODO: implement as auto update (no need to use '!')
    else if (input.substring(0, 4) == "bank")      {  Serial.println("bank not implemented yet.") ; }
    else if (input.substring(0,4 ) == "pwlx")      {  Serial.println("pwlx not implemented yet.") ; }
    else if (input.substring(0,4 ) == "pwly")      {  Serial.println("pwly implemented yet.") ; }
    else { ; } // we haven't understood, so Serial.println("?") is possible
    
    change = false;    
  }

/*    if (input.substring(0, 3) == "dir") {
      uint8_t  dir_setCh  = input.substring( 3,  5).toInt();
      uint8_t  dir_setDr  = input.substring( 5,  7).toInt();
      uint16_t dir_setPWM = input.substring( 7, 12).toInt();
      uint8_t  dir_setDC  = input.substring(12, 15).toInt();
      uint8_t  dir_setBC  = input.substring(15    ).toInt();
      tlc.setChannel( dir_setCh, dir_setDr, dir_setPWM, dir_setDC, dir_setBC );
    }
*/
/* 
    if (input.substring(0, 3) == "pwm") {
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
*/
}

// use memset, memcopy instead of for loops
// consider making structures to save into EEPROM

/*
void EEPROMsave() {
  for( int i = 0; i < tlc.nLEDs; i++) {
    EEPROM[i]               = CHmask[i];
    EEPROM[i + 1*tlc.nLEDs] = isoLog[i];
    EEPROM[i + 2*tlc.nLEDs] = isoDC[i];
  }
  for( int i = 0; i < tlc.nTLCs; i++) {
    EEPROM[i + 3*tlc.nLEDs] = isoBC[i];
  }
}

void EEPROMload() {
  for( int i = 0; i < tlc.nLEDs; i++) {
    CHmask[i]  = EEPROM[i];
    isoLog[i] = EEPROM[i + 1*tlc.nLEDs];
    isoDC[i]   = EEPROM[i + 2*tlc.nLEDs];
  }
  for( int i = 0; i < tlc.nTLCs; i++) {
    isoBC[i] = EEPROM[i + 3*tlc.nLEDs];
  }
}

void EEPROMpeek() {
  // print out mask, isoLog, isodc and isobc
  Serial.println("Peek at stored PWM values, use pwmload to load them to the program.");
  Serial.println("LED PWM   DC  BC");
  for( int i = 0; i < tlc.nLEDs; i++) {
    Serial.printf("%3d %5d %3d %3d\n", i, EEPROM[i + 1*tlc.nLEDs], EEPROM[i + 2*tlc.nLEDs], EEPROM[i + 3*tlc.nLEDs]);
  }
  Serial.println("-----------------");
}
*/

// FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS
// FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS
// FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS FUNCTIONS

/*
// TODO: change significantly i think
void readPWMsFromSerial(String input, int lenOfCommand) {
  int stepSize = 5;
  Serial.println("Changing PWM.");
  for (uint16_t i = lenOfCommand; i < input.length(); i+=stepSize){
    uint8_t led = (i-(lenOfCommand))/stepSize;
    isoLog[led] = constrain(input.substring(i, i+stepSize).toInt(), 0, tlc.MAX_PWM);
    Serial.println("  LED: " + String(led+1) + " at PWM: " + String(isoLog[led]));
  }
  Serial.println("Done");
}

void getPwm(){                                                                    // TODO: change to report log values, or delete
  Serial.println("-- isoLog for all LEDs --------------------------");
  Serial.println("   Format: index/driver/channel/pwm");
  for (uint8_t i = 0; i < D_NLS; i++) {
    Serial.printf("  %i %i\n\r", i, isoLog[i]);
  }
  Serial.println("--------------------------------------------------");
}
*/
