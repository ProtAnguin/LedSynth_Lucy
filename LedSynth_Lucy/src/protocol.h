// PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT
// PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT
// PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT PROT

bool change         = true;
bool waitForTrigIn  = false;
bool sendTrigOut    = false;
bool protActive     = false;

uint16_t p_ofs   = 500;  // Offset, duration, pause and wait times [ms] for protocol stimulation
uint16_t p_dur   = 50;
uint16_t p_pau   = 147;
uint16_t p_wait  = 107;

float    genAtt     = 0.0;  // General and adaptational atenuation [log]
float    adapAtt    = 0.0;
float    v_f        = 6.0; // V-logI <f>rom, <s>tep and <t>o values [log]
float    v_s        = 0.3;
float    v_t        = 0.0;

//--------------------------------------------------------------------------------------- COMPATIBILITY

void set(uint8_t led, int32_t pwmVal) {                                               // TODO: consider removing; this is for OLD set used by "protocols"
  // TODO constrain should be addressed in the library only?
  // led    = constrain(led,      0,   tlc.nLEDs);       
  // pwmVal = constrain(pwmVal,   0,   tlc.MAX_PWM);
  /* if (pwmVal < 0) {
    pwmVal = isoLog[led] >> (-pwmVal-1) ;   
    // use ISO values: divide by 1 for -1, divide by 2 for -2, by 4 for -3, ...
  }
  */
  tlc.change('P', led, pwmVal);
  tlc.change('D', led, isoDC[led]);
  update();                                                                         
}

void setRainbow (float logVal) {                                                      // TODO: name used by Protocols, should be setRainbowPWM
  for (uint8_t i = 0; i < D_NLS; i++) {                                                  // TODO: to be removed
    tlc.setlog(i, logVal);
  }
  update();
}

//--------------------------------------------------------------------------------------- COMPATIBILITY NEDS

int nlsUsed(int arr[]) {
  int sumVal = 0;
  for (int i = 0; i < D_NLS; i++) {
    sumVal += arr[i];
  }
  return sumVal;
} // end nlsUsed

void chooseLED(String inS) {
  if (!nlsUsed(mask)) return; // do nothing if mask is empty

  if (inS == "f" || inS == "n") { // first or next
    if (inS == "f") {
      LED.curr = -1;
      LED.wrapped = false;
    }
    do {
      LED.curr++;
      if (LED.curr >= D_NLS) {
        LED.curr = 0;
        LED.wrapped = true;
      }
    } while(mask[LED.curr] == 0);
  }
  if (inS == "l" || inS == "p") { // first or next
    if (inS == "l") {
      LED.curr = D_NLS;
      LED.wrapped = false;
    }
    do {
      LED.curr--;
      if (LED.curr < 0) {
        LED.curr = D_NLS-1;
        LED.wrapped = true;
      }
    } while (mask[LED.curr] == 0);
  }
}

void flash(uint16_t dur, bool sendTrigOut = false) {
  if (sendTrigOut) trigOut(TRIGOUTPIN, D_TRIGOUTLEN);
  setOe(1);
  envelope(1);
  delay( dur );
  setOe(0);
  envelope(0);
}

void protReport(String inStr) {
  if (inStr.substring(0, 6) != "report") { return; }
  
  Serial.println("Current settings: ");
  Serial.println("  Offset                 " + String(p_ofs) + " ms");
  Serial.println("  Duration               " + String(p_dur) + " ms");
  Serial.println("  Pause                  " + String(p_pau) + " ms");
  Serial.println("  Wait                   " + String(p_wait) + " ms");
  Serial.println("  Num of LEDs            " + String(nlsUsed(mask)));
  Serial.println("  Vlogi from:            " + String(v_f) + " log");
  Serial.println("  Vlogi step:            " + String(v_s) + " log");
  Serial.println("  Vlogi to:              " + String(v_t) + " log");
  Serial.println("  General attenuation    " + String(genAtt) + " log");
  Serial.println("  Adaptation attenuation " + String(adapAtt) + " log");
  Serial.println("  Adaptation bank " + String(N_USEBANK));
  Serial.print(  "  Wait for TTLin:        "); if (waitForTrigIn) { Serial.println("YES"); } else { Serial.println("NO"); }
  Serial.print(  "  Send TTLout:           "); if (sendTrigOut) { Serial.println("YES"); } else { Serial.println("NO"); }
  Serial.println();
} // end protReport

void protHelp(String inStr) {
  if (inStr.substring(0, 4) != "help" ) { return; }
  
  Serial.println("- HELP PROT -----------------------------------------------------------------------------------------");
  Serial.println("  Input 'report' to see current values");
  Serial.println("  Use set to change values:");
  Serial.println("    set o 100 <- changes <o>ffset   to 100 ms [from 0 to 86400000 = 1 day]");
  Serial.println("    set d 100 <- changes <d>uration to 100 ms [from 0 to 86400000 = 1 day]");
  Serial.println("    set p 100 <- changes <p>ause    to 100 ms [from 0 to 86400000 = 1 day]");
  Serial.println("    set w 100 <- changes <w>ait     to 100 ms [from 0 to 86400000 = 1 day]");
  Serial.println("    set m c 00110011... sets the <c>ustom <m>ask for LEDs used for stimulation");
  Serial.println("    set m a 00110011... sets the <a>daptation <m>ask for LEDs used for adaptation");
  Serial.println("    set v f -3.0 sets the <f>rom value of <v>logi and ramp protocols");
  Serial.println("    set v s 0.2  sets the <s>tep size  of <v>logi and ramp protocols");
  Serial.println("    set v t 0.0  sets the <t>o value   of <v>logi and ramp protocols");
  Serial.println("    set a g -1.0 sets the <g>eneral <a>ttenuation to be used in sweep, blink and adap protocols");
  Serial.println("    set a a -1.0 sets the <a>daptive <a>ttenuation to be used in adap protocol");
  Serial.println("    set a b X sets the <a>ttenuation <b>ank X to be used");
  Serial.println("    set c 3 sets the window width to be used in polycn and polycb protocols [1-11]");
  Serial.println("    set t X where X can be:");
  Serial.println("     +-Command-----TrigIn----TrigOut-+");
  Serial.println("     | set t 0       NO        NO    |");
  Serial.println("     | set t 1       YES       NO    |");
  Serial.println("     | set t 2       NO        YES   |");
  Serial.println("     | set t 3       YES       YES   |");
  Serial.println("     +-------------------------------+");
  Serial.println();
  Serial.println("Stimulus shape");
  Serial.println("               ________________               __ _ _ __             ________________           ");
  Serial.println("              |                |             |         |           |                |          ");
  Serial.println("              |                |             |                                      |<- pause  ");
  Serial.println("              |                |             |         |           |                |     +    ");                 
  Serial.println("<- offset - > | <- duration -> | <- pause -> |                                      |   wait ->");
  Serial.println("______________|                |_____________|         |__ _ _ _ __|                |__________");
  Serial.println();
  Serial.println("  Input 'sweep' to run the sweep protocol and 'stop' to finish it");
  Serial.println("  Input 'vlogi' to run the vlogi protocol and 'stop' to finish it");
  Serial.println("  Input 'ramp' to run the ramp protocol and 'stop' to finish it");
  Serial.println("  Input 'blink' to run the blink protocol and 'stop' to finish it");
  Serial.println("  Input 'adap' for selective adaptation and 'stop' to finish it");
  Serial.println("  Input 'polycX' where X can be <l>ong pass, <s>hort pass, <n>otch, <b>and pass or <a>ll of them");
  Serial.println("    polycX protocols are curently hardwired to use only leds from 1 to 12.");
  Serial.println();
  Serial.println("  Input 'matrep' for a clean report");
  Serial.println("  Input 'exit' to exit the Protocol builder");
  Serial.println("  Input 'help' to see this again");
  Serial.println("----------------------------------------------------------------------------------------------- END -");
} // end protHelp

void protMatReport(String inStr) {
  if (inStr.substring(0, 6) != "matrep") { return; }
  
  Serial.println("%-START_REPORT-");
  Serial.println("p.OFS = " + String(p_ofs) + ";");
  Serial.println("p.DUR = " + String(p_dur) + ";");
  Serial.println("p.PAU = " + String(p_pau) + ";");
  Serial.println("p.WAI = " + String(p_wait) + ";");
  Serial.println("p.NLS = " + String(nlsUsed(mask)) + ";");
  Serial.println("p.VFR = " + String(v_f) + ";");
  Serial.println("p.VST = " + String(v_s) + ";");
  Serial.println("p.VTO = " + String(v_t) + ";");
  Serial.println("p.GAT = " + String(genAtt) + ";");
  Serial.println("p.AAT = " + String(adapAtt) + ";");
  Serial.println("p.ABN = " + String(N_USEBANK) + ";");
  Serial.print(  "p.TIN = "); if (waitForTrigIn) { Serial.println(" 1;"); } else { Serial.println(" 0;"); }
  Serial.print(  "p.TOU = "); if (sendTrigOut  ) { Serial.println(" 1;"); } else { Serial.println(" 0;"); }
  
  Serial.print(  "p.PWM = [");
  for (uint8_t i=0; i < D_NLS; i++) Serial.printf("%3.1f ", isoLog[i]);
  Serial.println("];");
  
  Serial.println("%-END_REPORT-");
} // end protMatReport

void setFromSerial(String command) {
  if (command.substring(0, 3) != "set") { return; }
  
  if (command.substring(0, 7) == "set a g") {    genAtt = constrain(command.substring(7).toFloat(), 0, MAX_ATT_VALUE);         Serial.println("General attenuation set to: " + String(genAtt) + " log"); }
  if (command.substring(0, 7) == "set a a") {   adapAtt = constrain(command.substring(7).toFloat(), 0, MAX_ATT_VALUE);         Serial.println("Adaptive attenuation set to: " + String(adapAtt) + " log"); }
  if (command.substring(0, 7) == "set a b") { N_USEBANK = constrain(command.substring(7).toFloat(), 0, N_MAXBANK);         Serial.println("Attenuation bank set to: " + String(N_USEBANK)); }

  if (command.substring(0, 5) == "set o") {       p_ofs = constrain(command.substring(5).toInt(), 0, 86400000);     Serial.println("Offset   [ms]: " + String(p_ofs)); }
  if (command.substring(0, 5) == "set d") {       p_dur = constrain(command.substring(5).toInt(), 0, 86400000);     Serial.println("Duration [ms]: " + String(p_dur)); }
  if (command.substring(0, 5) == "set p") {       p_pau = constrain(command.substring(5).toInt(), 0, 86400000);     Serial.println("Pause    [ms]: " + String(p_pau)); }
  if (command.substring(0, 5) == "set w") {      p_wait = constrain(command.substring(5).toInt(), 0, 86400000);     Serial.println("Wait     [ms]: " + String(p_wait)); }

  if (command.substring(0, 6) == "set m " && !protActive) {
    String maskKey = command.substring(6, 7);
    if (maskKey == "c") { for (int i = 1; i < D_NLS; i++) mask[i] = command.substring(7+i, 8+i).toInt(); }
    if (maskKey == "a") { for (int i = 1; i < D_NLS; i++) adapMask[i] = command.substring(7+i, 8+i).toInt(); }
    Serial.println("  Number of STIMulus LEDs: " + String(nlsUsed(mask)));
    Serial.println("  Number of ADAPting LEDs: " + String(nlsUsed(adapMask)));
  }

  if (command.substring(0, 5) == "set t") {
    uint8_t tempTrigVal = constrain(command.substring(5).toInt(), 0, 3);
    waitForTrigIn = tempTrigVal & 0b01;
    sendTrigOut   = tempTrigVal & 0b10;

    protReport( "report" );
  }

  if (command.substring(0, 6) == "set v ") {
    String key = command.substring(6, 7);
    float serVal = command.substring(7).toFloat();
    if (key == "f") { if (serVal > v_t) { v_f = serVal; } else { Serial.println("Invalid number! FROM must be bigger than TO"); } } // set FROM value
    if (key == "s") { if (serVal > 0  ) { v_s = serVal; } else { Serial.println("Invalid number! STEP should be bigger than 0 (it will be subtracted from FROM to get to TO)."); } } // set STEP value
    if (key == "t") { if (serVal < v_f) { v_t = serVal; } else { Serial.println("Invalid number! TO must be smaller than FROM"); } } // set  TO  value

    protReport( "report" );
  }
} // end setFromSerial

void sweepProtocol(String inStr) {
  if (inStr.substring(0, 5) != "sweep") { return; }
  protActive = true;
  bool didOfset = false;

  Serial.println("SWEEP protocol started, input 'stop' to finish");
  if (waitForTrigIn) Serial.println("    Waiting for external trigger...");

  setRainbow(OFF_LOG_VALUE);
  chooseLED("f");
  
  if ( nlsUsed(mask) == 0) {
    Serial.println("No LEDs selected, can't run the protocol");
    command = "stop";
  }
  
  while (command.substring(0, 4) != "stop") {
    if (!waitForTrigIn || (waitForTrigIn && trigReceived)) {
      if (waitForTrigIn && !didOfset) {
        delay(p_ofs);
        didOfset = true;
      }

      tlc.setlog( LED.curr, isoLog[N_USEBANK][LED.curr]+genAtt+Ibanks[N_USEBANK][LED.curr] );
      flash(p_dur, sendTrigOut);
      tlc.setlog( LED.curr, OFF_LOG_VALUE);

      delay(p_pau);

      chooseLED("n");

      if (LED.wrapped) {
        delay(p_wait);
        LED.wrapped = false;
        trigReceived = false;
        didOfset = false;
      }
    }
    
    if (Serial.available() > 0) {
      command = Serial.readStringUntil('*');
      setFromSerial(command);
    }
  }

  protActive = false;
} // end sweepProtocol

void peewsProtocol(String inStr) {
  if (inStr.substring(0, 5) != "peews") { return; }
  protActive = true;
  bool didOfset = false;

  Serial.println("PEEWS protocol started, input 'stop' to finish");
  if (waitForTrigIn) Serial.println("    Waiting for external trigger...");

  setRainbow(OFF_LOG_VALUE);
  chooseLED("l");
  
  if ( nlsUsed(mask) == 0) {
    Serial.println("No LEDs selected, can't run the protocol");
    command = "stop";
  }
  
  while (command.substring(0, 4) != "stop") {
    if (!waitForTrigIn || (waitForTrigIn && trigReceived)) {
      if (waitForTrigIn && !didOfset) {
        delay(p_ofs);
        didOfset = true;
      }

      tlc.setlog( LED.curr, isoLog[N_USEBANK][LED.curr]+genAtt+Ibanks[N_USEBANK][LED.curr]);
      flash(p_dur, sendTrigOut);
      tlc.setlog( LED.curr, OFF_LOG_VALUE );

      delay(p_pau);

      chooseLED("p");

      if (LED.wrapped) {
        delay(p_wait);
        LED.wrapped = false;
        trigReceived = false;
        didOfset = false;
      }
    }
    
    if (Serial.available() > 0) {
      command = Serial.readStringUntil('*');
      setFromSerial(command);
    }
  }
  
  protActive = false;
} // end peewsProtocol

void vlogiProtocol(String inStr) {
  if (inStr.substring(0, 5) != "vlogi") { return; }
  
  protReport( "report" );
  Serial.println("VLOGI protocol started, input 'stop' to finish");
  if (waitForTrigIn) Serial.println("    Waiting for external trigger...");

  
  bool endVlogi = false;
  bool didOfset = false;
  float v_fac = v_f;

  setRainbow(OFF_LOG_VALUE);
  setOe(0); // this prot uses OE to flash
  
  if ( nlsUsed(mask) == 0) {
    Serial.println("No LEDs selected, can't run the protocol");
    command = "stop";
  }
  
  while (command.substring(0, 4) != "stop" && !endVlogi) {
    if ((waitForTrigIn and trigReceived) or !waitForTrigIn) {
      Serial.printf("Vlogi: %2d LEDS at %5.2f log \n", nlsUsed(mask), v_fac);
      if (waitForTrigIn && !didOfset) {
        delay(p_ofs);
        didOfset = true;
      }

      for (int i = 0; i < D_NLS; i++) {
        if (mask[i]) {
          tlc.setlog(i, v_fac+genAtt+Ibanks[N_USEBANK][i]+isoLog[N_USEBANK][i]);
        }
      }

      flash(p_dur, sendTrigOut);
      delay(p_pau);

      v_fac -= v_s;

      if (v_fac < (v_t - 0.01) ){
        v_fac = v_f;
        delay(p_wait);
        trigReceived = false;
        didOfset = false;
      }
    }
    
    if (Serial.available() > 0) {
      command = Serial.readStringUntil('*');
      setFromSerial(command);
    }
  }

  setRainbow(OFF_LOG_VALUE);
} // end vlogiProtocol

void rampProtocol(String inStr) {
  if (inStr.substring(0, 4) != "ramp") { return; }
  bool didOfset = false;

  int32_t t_ramp_log = 0; // temp pwm holder to omit sending out flashes if LED already did its best (preserve the cell)
  
  Serial.println("RAMP protocol started, input 'stop' to finish");
  if (waitForTrigIn) Serial.println("    Waiting for external trigger...");

  setOe(0);
  setRainbow(OFF_LOG_VALUE);
  float v_fac = v_f;
  
  if ( nlsUsed(mask) == 0) {
    Serial.println("No LEDs selected, can't run the protocol");
    command = "stop";
  }

  chooseLED("f");
  
  while (command.substring(0, 4) != "stop" && !LED.wrapped) {
    if ((waitForTrigIn && trigReceived) || !waitForTrigIn) {
      if (!didOfset) {
        delay(p_ofs);
        didOfset = true;
      }
      
      Serial.println("Ramp: Led " + String(LED.curr) + " at " + String(v_fac) + " log of iso intensity.");

      t_ramp_log = v_fac+genAtt+Ibanks[N_USEBANK][LED.curr]+isoLog[N_USEBANK][LED.curr]; // TODO: this will not work as it expects a pwm value, thanks LOGs!

      if (t_ramp_log < 0) t_ramp_log = OFF_LOG_VALUE; // turn blinking off as we already tested the max possible output in this ramp

      tlc.setlog(LED.curr, t_ramp_log);
      flash(p_dur);
      tlc.setlog(LED.curr, OFF_LOG_VALUE);
      delay(p_pau);

      v_fac -= v_s;
      if (v_fac < (v_t - 0.01) ) {
        trigReceived = false;
        didOfset = false;
        v_fac = v_f;
        chooseLED("n");
        delay(p_wait);
      }
    }
    
    if (Serial.available() > 0) {
      command = Serial.readStringUntil('*');
      setFromSerial(command);
    }
  }

  Serial.println("RAMP protocol finished.");
  setRainbow(OFF_LOG_VALUE);
  protReport( "report" );
} // end rampProtocol

void blinkProtocol(String inStr) {
  if (inStr.substring(0, 5) != "blink") { return; }
  
  protReport( "report" );
  Serial.println("BLINK protocol with ISO light started, input 'stop' to finish");
  if (waitForTrigIn) Serial.println("    Waiting for external trigger...");

  setOe(0);
  for (int i = 0; i < D_NLS; i++) {
    if (mask[i]) {
      tlc.setlog(i, Ibanks[N_USEBANK][i]+genAtt+isoLog[N_USEBANK][i]);
    }
  }
  
  while (command.substring(0, 4) != "stop") {
    if ((waitForTrigIn && trigReceived) || !waitForTrigIn) {
      trigReceived = false;
      
      if (waitForTrigIn) delay(p_ofs);
      flash(p_dur, sendTrigOut);
      delay(p_pau);
    }
    
    if (Serial.available() > 0) {
      command = Serial.readStringUntil('*');
      setFromSerial(command);
    }
  }

  setRainbow(OFF_LOG_VALUE);
  protReport( "report" );
} // end blinkProtocol

void adapProtocol(String inStr) {
  if (inStr.substring(0, 4) != "adap") { return; }
  
  bool didOfset = false;

  setRainbow(OFF_LOG_VALUE);
  setOe(1);
  chooseLED("f");

  for (int i = 1; i < D_NLS; i++) {
    if( adapMask[i] ){
      tlc.setlog(i, isoLog[N_USEBANK][i]+adapAtt+Ibanks[N_USEBANK][i]);
    }
  }
  
  protReport( "report" );
  Serial.println("ADDAPTATION protocol started, input 'stop' to finish");
  Serial.println("  General attenuation for stimulus LEDs is " + String(genAtt));
  Serial.println("  Adaptation attenuation for adaptation LEDs is " + String(adapAtt));
  if (waitForTrigIn) Serial.println("    Waiting for external trigger...");
  
  if ( nlsUsed(mask) == 0) {
    Serial.println("No LEDs selected, can't run the protocol");
    command = "stop";
  }
  
  while (command.substring(0, 4) != "stop") {
    if ((waitForTrigIn && trigReceived) || !waitForTrigIn) {
      if (waitForTrigIn && !didOfset) {
        delay(p_ofs);
        didOfset = true;
      }

      tlc.setlog( LED.curr, isoLog[N_USEBANK][LED.curr]+genAtt+Ibanks[N_USEBANK][LED.curr]);
      envelope(1);
      delay(p_dur);
      if ( adapMask[LED.curr] ) {
        tlc.setlog( LED.curr, isoLog[N_USEBANK][LED.curr]+adapAtt+Ibanks[N_USEBANK][LED.curr]);
      }
      else {
        tlc.setlog( LED.curr, OFF_LOG_VALUE );
      }
      
      envelope(0);
      delay(p_pau);

      chooseLED("n");

      if (LED.wrapped) {
        LED.wrapped = false;
        delay(p_wait);
        trigReceived = false;
        didOfset = false;
      }
    }
    
    if (Serial.available() > 0) {
      command = Serial.readStringUntil('*');
      setFromSerial(command);
    }
  }
  
  setRainbow(OFF_LOG_VALUE);
  setOe(0);
  protReport( "report" );
} // end adapProtocol

void protocolEnvironment() {
  Serial.println(" _______  ______    _______  _______  _______  _______  _______  ___        _______  __   __  ___   ___      ______   _______  ______   ");
  Serial.println("|       ||    _ |  |       ||       ||       ||       ||       ||   |      |  _    ||  | |  ||   | |   |    |      | |       ||    _ |  ");
  Serial.println("|    _  ||   | ||  |   _   ||_     _||   _   ||       ||   _   ||   |      | |_|   ||  | |  ||   | |   |    |  _    ||    ___||   | ||  ");
  Serial.println("|   |_| ||   |_||_ |  | |  |  |   |  |  | |  ||       ||  | |  ||   |      |       ||  |_|  ||   | |   |    | | |   ||   |___ |   |_||_ ");
  Serial.println("|    ___||    __  ||  |_|  |  |   |  |  |_|  ||      _||  |_|  ||   |___   |  _   | |       ||   | |   |___ | |_|   ||    ___||    __  |");
  Serial.println("|   |    |   |  | ||       |  |   |  |       ||     |_ |       ||       |  | |_|   ||       ||   | |       ||       ||   |___ |   |  | |");
  Serial.println("|___|    |___|  |_||_______|  |___|  |_______||_______||_______||_______|  |_______||_______||___| |_______||______| |_______||___|  |_|");
  Serial.println();
  Serial.println("  Input 'help' for instructions and 'exit' to exit the Protocol builder");
  
  // House cleaning
  command = "";
  change = false;
  setOe(0);
  setRainbow(OFF_LOG_VALUE); // update this name

  protReport( "report" );

  while (command != "exit") {
    if (Serial.available() > 0) {
      command = Serial.readStringUntil('*');
      change = true;
    }

    if (change) {
      change = false;
      
      protReport( command );
      protMatReport( command );
      protHelp( command );
      setFromSerial( command );

      sweepProtocol( command );
      peewsProtocol( command );
      vlogiProtocol( command );
      rampProtocol ( command );
      blinkProtocol( command );
      adapProtocol ( command );
    }

    trigReceived = false;
  }
  Serial.println("Closed Protocol builder");
} // end protocolEnvironment
