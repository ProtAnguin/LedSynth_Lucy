// ------------------------------------------------------------------------------------------------------------------------------- VARIABLES
uint32_t OPTwhite;

// ------------------------------------------------------------------------------------------------------------------------------- CALIB HELP
void calibHelp(String inStr) {
  if (inStr.substring(0, 4) != "help" ) { return; }
  
  Serial.println("- CALIB  PROT ----------------------------------------------------------------------------------------");
  Serial.println("  'exit' to exit the Protocol builder");
  Serial.println("  'help' to see this again");
  Serial.println("");
  Serial.println("  'set l X' to set the LED");
  Serial.println("  'set a X.XX' to set the attenuation (1 = 10%)");
  Serial.println("");
  Serial.println("  'go' to set the LED at attenuation");
  Serial.println("  'run' predefined protocol with every LED at attenuation steps, produces CSV");
  Serial.println("------------------------------------------------------------------------------------------------ END -");
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
} // end setFromSerial

// ------------------------------------------------------------------------------------------------------------------------------- CALIB GO measure with OPT
void calibGo(String command) {
	if (command.substring(0, 2) != "go") { return; }

  setRainbow(OFF_LOG_VALUE);
  tlc.setlog( LED.curr, LED.logVal );

  Serial.printf("%2d,", LED.curr);
  setOe(1);
	// OPT measure and report for the first implementation
	for(int i = 0; i<6; i++) {
    delay(200); // give OPT time to measure
	  OPTwhite = opt.getADCCh3();
    Serial.printf("%11d,", OPTwhite);
  }
	setOe(0);

  Serial.printf(" LED %02i  LOG %1.3f  WHITE %11d\n", LED.curr, LED.logVal, OPTwhite);
}

void calibRun(String command) {
	if (command.substring(0, 3) != "run") { return; }
  
  setRainbow(OFF_LOG_VALUE);
  setOe(1);
  for (int j = 0; j < 10; j++) { // dekaLOG values (will be divided by 10.0 later)
    LED.logVal = j/10.0;
    Serial.printf("%1.3f,", LED.logVal);

    for (int i = 1; i < D_NLS; i++) { // curr led index
      LED.curr = i;

      tlc.setlog( LED.curr, LED.logVal );
      tlc.update();
      delay(1000);
      OPTwhite = opt.getADCCh3();
      setRainbow(OFF_LOG_VALUE);
      
      Serial.printf("%11d,", OPTwhite);
    }

    Serial.println(); // go to new line boy!
  }
  setOe(0);
}

// ------------------------------------------------------------------------------------------------------------------------------- CALIB ENVIRONMENT MAIN
// ------------------------------------------------------------------------------------------------------------------------------- CALIB ENVIRONMENT MAIN
// ------------------------------------------------------------------------------------------------------------------------------- CALIB ENVIRONMENT MAIN
// ------------------------------------------------------------------------------------------------------------------------------- CALIB ENVIRONMENT MAIN
// ------------------------------------------------------------------------------------------------------------------------------- CALIB ENVIRONMENT MAIN
void calibEnvironment() {
	Serial.println(" CALIB environment (using OPT4048)");
	Serial.println("  Input 'help' for instructions and 'exit' to exit the Protocol builder");
  
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

			calibHelp( command );
    }

    trigReceived = false;
  }
  Serial.println("Closed CALIB environment");
}
