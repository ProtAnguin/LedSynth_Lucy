// ------------------------------------------------------------------------------------------------------------------------------- VARIABLES
uint32_t OPTwhite;

float calibFrom = 0.0;
float calibStep = 0.1;
float calibTo   = 4.0;
int calibMeasureTime_ms = 400;

int beginLed = 1;
int endLed = D_NLS-1;

// sensitivity values of OPT4048
float OPTcalibValues[D_NLS] = {
  1.000000,
  1.963336,
  1.381317,
  1.445040,
  1.136919,
  1.072260,
  1.000000,
  1.017518,
  1.040798,
  1.050615,
  1.103912,
  1.232097,
  1.354838,
  1.524902,
  1.765031,
  1.981606,
  2.170910
};

// ------------------------------------------------------------------------------------------------------------------------------- CALIB HELP
void calibHelp(String inStr) {
  if (inStr.substring(0, 4) != "help" ) { return; }
  
  Serial.println("- CALIB  PROT -----------------------------------------------------------------------------------------------------------------------------------");
  Serial.println("  'exit' to exit the Protocol builder");
  Serial.println("  'help' to see this again");
  Serial.println("");
  Serial.printf ("  'set l X' to set the LED number                                                 Current value: %d\n", LED.curr);
  Serial.printf ("  'set a X.XX' to set the log attenuation (0 = 100%, 1 = 10%, 2 = 1%, 3=0.1%)     Current value: %1.4f log\n", LED.logVal);
  Serial.println("  'go' to set the LED at attenuation and measure in 100 ms intervals until the measurement time is reached");
  Serial.println("");
  Serial.printf ("  'set f X.XX' set the intensity to start from (positive decimal values)          Current value: %1.4f log\n", calibFrom);
  Serial.printf ("  'set s X.XX' set the intensity step (positive decimal values)                   Current value: %1.4f log\n", calibStep);
  Serial.printf ("  'set t X.XX' set the intensity to go to (positive decimal values)               Current value: %1.4f log\n", calibTo);
  Serial.printf ("  'set m X'    set the measurement time of the OPT sensor [ms]                    Current value: %d ms\n", calibMeasureTime_ms);
  Serial.printf ("  'set b X'    start with the LED#                                                Current value: %d \n", beginLed);
  Serial.printf ("  'set e X'    end with the LED#                                                  Current value: %d \n", endLed);
  Serial.println("  'run' the protocol with every LED at attenuation steps, produces CSV (sensitivity factors of the OPT are incorporated in the result)");
  Serial.println("  'print'      prints the log intensity to PWM/DC conversion table in the range set by (f,s,t)");
  Serial.println("------------------------------------------------------------------------------------------------------------------------------------------- END -");
}

// ------------------------------------------------------------------------------------------------------------------------------- SET CALIB FROM SERIAL
void setCalibFromSerial(String command) {
  if (command.substring(0, 3) != "set") { return; }
  
  if (command.substring(0, 6) == "set l ") {
		LED.curr = constrain (command.substring(6,8).toInt(),0,D_NLS);
		LED.all  = false;
		Serial.printf("Calib led%02i \n",LED.curr);
  }
	if (command.substring(0, 6) == "set a ") {
		LED.logVal = command.substring(6).toFloat();
		Serial.printf("@att %1.3f\n",LED.logVal);
		setLog();
  }
  if (command.substring(0, 6) == "set f ") {
		calibFrom = command.substring(6).toFloat();
		Serial.printf("Calib from: %1.3f\n",calibFrom);
  }
  if (command.substring(0, 6) == "set s ") {
		calibStep = command.substring(6).toFloat();
		Serial.printf("Calib step: %1.3f\n",calibStep);
  }
  if (command.substring(0, 6) == "set t ") {
		calibTo   = command.substring(6).toFloat();
		Serial.printf("Calib  to : %1.3f\n",calibTo);
  }
  if (command.substring(0, 6) == "set m ") {
		calibMeasureTime_ms = command.substring(6).toInt();
		Serial.printf("Calib measure time se to %d ms\n", calibMeasureTime_ms);
  }
  if (command.substring(0, 6) == "set b ") {
    beginLed = constrain(command.substring(6).toInt(),0,D_NLS-1) ;
		Serial.printf("Begin with led %d",beginLed);
  }
  if (command.substring(0,6) == "set e ") {
    endLed = constrain(command.substring(6).toInt(),0,D_NLS-1) ;
		Serial.printf("End with led %d",endLed);
  }
} // end setFromSerial

// ------------------------------------------------------------------------------------------------------------------------------- CALIB GO measure with OPT
void calibGo(String command) {
	if (command.substring(0, 2) != "go") { return; }

  setRainbow(OFF_LOG_VALUE);
  tlc.setlog( LED.curr, LED.logVal );

  Serial.printf("%2d,", LED.curr);
  setOe(1);
	// OPT measure and report for the first implementation
	for(int i = 0; i<=calibMeasureTime_ms; i+=100) {
    delay(100); // give OPT time to measure
	  OPTwhite = opt.getADCCh3();
    Serial.printf("%11d,", OPTwhite);
  }
	setOe(0);

  Serial.printf(" LED %02i  LOG %1.3f  rawOPTch3 %11d OPTcorrFac for this LED %1.4f result %16.4f\n",
    LED.curr,
    LED.logVal,
    OPTwhite,
    OPTcalibValues[LED.curr], 
    float(OPTwhite) * OPTcalibValues[LED.curr]);
}

void calibRun(String command) {
	if (command.substring(0, 3) != "run") { return; }
  
  setRainbow(OFF_LOG_VALUE);
  setOe(1);
  for (float j = calibFrom; j <= calibTo; j+=calibStep) { // dekaLOG values (will be divided by 10.0 later)
    LED.logVal = j;
    Serial.printf("%1.3f", LED.logVal);

    for (int i = beginLed; i <= endLed; i++) { // curr led index                                                  // PP TODO check <=, <
      LED.curr = i;

      tlc.setlog( LED.curr, LED.logVal );
      tlc.update();
      delay(calibMeasureTime_ms);
      OPTwhite = opt.getADCCh3();
      setRainbow(OFF_LOG_VALUE);
      if (OPTwhite < 1)
        { OPTwhite = 1 ; }

      // Serial.printf("%16.4f,", float(OPTwhite) * OPTcalibValues[LED.curr]);
      Serial.printf(",%1.5f", log10(float(OPTwhite)));
      // Serial.printf("%12d, ", OPTwhite);

      if (Serial.available() > 0) {
        command = Serial.readStringUntil('*');
        j = 200;
        break;
      }
    }

    Serial.println(); // go to new line boy!
  }
  setOe(0);
}

// will print the logi2pwm table
void calibPrint(String command) {
  if (command.substring(0,5) != "print") 
    { return ; }
  Serial.println("%% Table for converting logi to PWM/DC values (calculated with pwm2logi)");
  Serial.println("%% logi, fullch, pwm, dc, mode, log10(powerin), log10(powerout), error");
  for (float j = calibFrom ; j<= calibTo; j+=calibStep) 
        tlc.printLogi2pwm(j);
}

// calib function


// ------------------------------------------------------------------------------------------------------------------------------- CALIB ENVIRONMENT MAIN
// ------------------------------------------------------------------------------------------------------------------------------- CALIB ENVIRONMENT MAIN
// ------------------------------------------------------------------------------------------------------------------------------- CALIB ENVIRONMENT MAIN
// ------------------------------------------------------------------------------------------------------------------------------- CALIB ENVIRONMENT MAIN
// ------------------------------------------------------------------------------------------------------------------------------- CALIB ENVIRONMENT MAIN
void calibEnvironment() {
	Serial.println(" CALIB environment (using OPT4048)");
	Serial.println("  Input 'help' for instructions and 'exit' to exit");

  if(!opt.begin()) {
    Serial.println(" ERROR calib not possible, OPT sensor not detected! Connect and run calib again (no need to reset the LedSynth).");
    return;
  }
  opt.setBasicSetup();
  opt.setRange(RANGE_AUTO);
  opt.setConversionTime(CONVERSION_TIME_100MS);
  opt.setOperationMode(OPERATION_MODE_CONTINUOUS);
  
  // House cleaning
  command = "";
  change = false;
  setOe(0);
  setRainbow(OFF_LOG_VALUE); // update this name

  while (command != "exit") {
    if (Serial.available() > 0) {
      command = Serial.readStringUntil('*');
      change = true;
    }

    if (change) {
      change = false;
      
      setCalibFromSerial( command );
			calibGo( command );
      calibRun( command );
      calibPrint ( command );
			calibHelp( command );

    }

    trigReceived = false;
  }
  Serial.println("Closed CALIB environment");
}
