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
#include "SparkFun_OPT4048.h"
#include <Wire.h>

//#include "Lucy.h"
#include "Maxim1.h"
//#include "Maxim5.h"
//#include "Maxim15.h"
#include "easter.h"
#include "storeDataToEEPROM.h"


String input = "help";   //used for storing incoming strings, set to <prot> to go into ProtocolBuilder on startup
String command = ""; // used to store the command that the user sends via Serial port (empty at Init)
char delimiter = '*' ;
TLC5948 tlc(D_nTLCs, D_NLS, CHmask, 6);

SparkFun_OPT4048 opt;

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
  // the INFOpin will go rising when the update is finished
  digitalWrite(INFOPIN,0) ; 
  tlc.update() ; 
  digitalWrite(INFOPIN,1) ; 
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
  else                                                    { tlc.setlog(LED.curr, LED.logVal); }
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
      Serial.println("aborted: all leds on, switch to one led first") ;
    else if ((LED.curr<0) || (LED.curr>D_NLS))
      Serial.println("aborted: current LED number is impossible") ;
    else if ((LED.logVal < 0) || (LED.logVal > 4 ))
      Serial.println("aborted: log value out of range [0 .. 4]") ;            
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

#include "chase.h" // Fast sync protocols for Adam

// SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP
// SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP
// SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP
void setup() {
  pinMode(TRIGOUTPIN,   OUTPUT);
  pinMode(ENVELOPEPIN,  OUTPUT);
  pinMode(INFOPIN,      OUTPUT);               
  pinMode(TRIGINPIN,    INPUT_PULLUP);

  Serial.begin(SERIAL_BAUD_RATE);
  Serial.setTimeout(SERIAL_TIMEOUT);

  SPI.begin();
  tlc.begin();                            
  Wire.begin();

  // interrupt routine to catch the trigIn flag
  attachInterrupt(digitalPinToInterrupt(TRIGINPIN), trigInISR, RISING); 

  loadFromEEPROM();  // Load stored data into isoLog
  mainWelcome();
}

// LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP
// LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP
// LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP
void loop() {
  if (Serial.available() > 0) {
    input = Serial.readStringUntil(delimiter);
    change = true;
  }

  if (trigReceived == true) update() ;              // update TLC on external trigger 

  // TODO: implement ECHO on / off to switch all printf commands off if needed!

  if (change) {
    //----------------------------------------------------------------------------------------------------------------------------- Single line protocols
    // shouldnt this be "else ifs??????"
    if (     input.substring(0, 5) == "reset")         WRITE_RESTART(0x5FA0004);
    else if (input.substring(0, 5) == "welc")          mainWelcome();
    else if (input.substring(0, 4) == "help")          mainHelp();
    else if (input.substring(0, 4) == "leds")          mainPrintLeds();
    else if (input.substring(0, 4) == "prot")          protocolEnvironment();
    else if (input.substring(0, 5) == "chase")         chaseEnvironment();
    else if (input.substring(0, 7) == "version")       plotEaster(TextSignature, 1, TextSignature_h, TextSignature_w);       
    else if (input.substring(0, 6) == "sensei")        plotEaster(easter, 1, easter_h, easter_w);
    else if (input.substring(0, 6) == "debug1")        tlc.debugTLCflag = !tlc.debugTLCflag;
    else if (input.substring(0, 6) == "debug2")        tlc.printFramesTLCflag = !tlc.printFramesTLCflag;
    else if (input.substring(0, 5) == "print")         tlc.printCPWM();
    else if (input.substring(0, 5) == "frame")         tlc.printFrames();
    else if (input.substring(0, 6) == "wiring")        tlc.printMask();
    else if (input.substring(0, 5) == "param")    {    Serial.printf("command delimiter: %c\n",delimiter);
                                                       tlc.C_ESPWM  ? Serial.println("mode ESPWM") : Serial.println("mode PWM");
                                                       tlc.C_TMGRST ? Serial.println("timing reset on") : Serial.println("timing reset off");
                                                       tlc.C_DSPRPT ? Serial.println("display repeat on") : Serial.println("display repeat off");
                                                       tlc.C_BLANK  ? Serial.println("blanking on") : Serial.println("blanking off");
                                                       Serial.printf("Serial clock goal %1.3f MHz\n",float(tlc.GOAL_SCLK_HZ)/1000000.0);                                                       
                                                       Serial.printf("Grayscale clock goal %1.3f MHz\n",float(tlc.GOAL_GSCLK_HZ)/1000000.0);                                                       
                                                       LED.all ? Serial.printf("Rainbow selected.\n") : Serial.printf("Led%02i selected.\n",LED.curr);
                                                  }    
    else if (input.substring(0,5)  == "delim")     { if (delimiter=='*') delimiter=';'; else delimiter='*'; Serial.printf("command delimiter: %c\n",delimiter);  }
    else if (input.substring(0, 5) == "espwm")     {   tlc.C_ESPWM  = constrain(input.substring(5).toInt(),0,1) ; 
                                                       tlc.C_ESPWM  ? Serial.println("mode ESPWM") : Serial.println("mode PWM");}
    else if (input.substring(0, 5) == "blank")     {   tlc.C_BLANK  = constrain(input.substring(5).toInt(),0,1) ; 
                                                       tlc.C_BLANK  ? Serial.println("blanking on") : Serial.println("blanking off");} 
    else if (input.substring(0, 6) == "tmgrst")    {   tlc.C_TMGRST = constrain(input.substring(6).toInt(),0,1) ; 
                                                       tlc.C_TMGRST ? Serial.println("timing reset on") : Serial.println("timing reset off");} 
    else if (input.substring(0, 6) == "dsprpt")    {   tlc.C_DSPRPT = constrain(input.substring(6).toInt(),0,1) ; 
                                                       tlc.C_DSPRPT ? Serial.println("display repeat on") : Serial.println("display repeat off");} 
    else if (input.substring(0,4 ) == "sclk")      {  tlc.GOAL_SCLK_HZ  = input.substring(4).toFloat()*1000000; 
                                                      Serial.printf("Serial clock frequency goal %6.3f MHz\n",float(tlc.GOAL_SCLK_HZ)/1000000.0); }
    else if (input.substring(0,4 ) == "gclk")      {  tlc.GOAL_GSCLK_HZ = input.substring(4).toFloat()*1000000; 
                                                      tlc.setGSCLK(tlc.GOAL_GSCLK_HZ); 
                                                      Serial.printf("Grayscale clock frequency goal %6.3f MHz. See note in help.\n",float(tlc.GOAL_GSCLK_HZ)/1000000.0); }
    else if (input.substring(0,4 ) == "clks")      {  Serial.printf("Serial    clock frequency %6.3f MHz\nGrayscale clock frequency %6.3f MHz\n", float(tlc.GOAL_SCLK_HZ)/1000000.0, float(tlc.GOAL_GSCLK_HZ)/1000000.0); }

    else if (input.substring(0, 2) == "oe")        {   setOe( 1 ); Serial.println("output on" ); }
    else if (input.substring(0, 2) == "od")        {   setOe( 0 ); Serial.println("output off"); }

    else if (input.substring(0, 1) == "!")            update() ;     // the most important command
    else if (input.substring(0, 1) == "?")         {  waitTrigUpdate(100) ; } // TODO: do we need this?
    else if (input.substring(0, 4) == "wait")      {  waitTrigUpdate(constrain(input.substring(4).toInt(),0,MAXWAITMS)) ; }
    
    else if (input.substring(0, 3) == "del")          delay(constrain(input.substring(3).toInt(),0,1000));
    
    else if (input.substring(0, 4) == "trig")         trigOut(TRIGOUTPIN,1); // trigger out for 1 ms

    // control LEDS
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
    else if (input.substring(0, 3) == "set")       {   LED.all = false;
                                                       LED.curr = constrain (input.substring(3,5).toInt(),0,D_NLS);
                                                       LED.logVal = (input.substring(5, 9).toInt())/1000.0;
                                                       setLog();
                                                       Serial.printf("@log %1.3f\n",LED.logVal);
                                                       update();     // the most important command
                                                   } 

    // EEPROM
    else if (input.substring(0, 6) == "update")    {  Serial.println("update not implemented yet.") ; }     // TODO: implement as auto update (no need to use '!')
    else if (input.substring(0, 4) == "bank")      {  isoLogCurr  = constrain(input.substring(4).toInt(),0,N_ISOBANKS-1);
                                                      Serial.printf("bank selected %d", isoLogCurr);
                                                      printIsoLog();
                                                   }
    // else if (input.substring(0,4 ) == "pwlx")      {  Serial.println("pwlx not implemented yet.") ; }
    // else if (input.substring(0,4 ) == "pwly")      {  Serial.println("pwly implemented yet.") ; }
    
    else if (input.substring(0,6 ) == "eeprom")    {
      if (input.substring(6,10 ) == "peek")  { peekFromEEPROM(); }
      if (input.substring(6,10 ) == "load")  { loadFromEEPROM(); }
      if (input.substring(6,10 ) == "save")  { saveToEEPROM(); }
      if (input.substring(6,11 ) == "teser") { resetEEPROM(); }
      if (input.substring(6,11 ) == "print") { printIsoLog(); }
      if (input.substring(6,9 )  == "set")   { setDataValue(input.substring(9,11).toInt(), input.substring(11,13).toInt(), input.substring(13).toFloat()); }
    }
    else { ; } // we haven't understood, so Serial.println("?") is possible
    
    change = false;
   }
  }
  