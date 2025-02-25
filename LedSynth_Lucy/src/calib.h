// ------------------------------------------------------------------------------------------------------------------------------- VARIABLES
uint32_t lux;

// ------------------------------------------------------------------------------------------------------------------------------- CALIB HELP
void calibHelp(String inStr) {
  if (inStr.substring(0, 4) != "help" ) { return; }
  
  Serial.println("- CALIB  PROT ----------------------------------------------------------------------------------------");
  Serial.println("  Input 'exit' to exit the Protocol builder");
  Serial.println("  Input 'help' to see this again");
  Serial.println("------------------------------------------------------------------------------------------------ END -");

}

// ------------------------------------------------------------------------------------------------------------------------------- SET CALIB FROM SERIAL
void setCalibFromSerial(String command) {
  if (command.substring(0, 3) != "set") { return; }
  
  if (command.substring(0, 6) == "set l ") {
		LED.curr = constrain (input.substring(6,8).toInt(),0,D_NLS);
		LED.all  = false;
		Serial.printf("Calib led%02i \n",LED.curr);
  }
	if (command.substring(0, 6) == "set a ") {
		LED.logVal = input.substring(6).toFloat();
		Serial.printf("@att %1.3f\n",LED.logVal);
		setLog();
  }
} // end setFromSerial

// ------------------------------------------------------------------------------------------------------------------------------- CALIB GO measure with OPT
void calibGo(String command) {
	if (command.substring(0, 2) != "go") { return; }

	tlc.update(); // this will turn on the LED

	// OPT measure and report for the first implementation
	delay(4 * 200); // give OPT time to measure
	lux = opt.getLux();
  Serial.printf("LED %02i  LOG %1.3f  LUX %7d\n", LED.curr, LED.logVal, lux);

	// turn LEDs off
	setRainbow(OFF_LOG_VALUE);
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

  protReport( "report" );

  while (command != "exit") {
    if (Serial.available() > 0) {
      command = Serial.readStringUntil('*');
      change = true;
    }

    if (change) {
      change = false;
      
      setCalibFromSerial( command );
			calibGo( command );

			calibHelp( command );
    }

    trigReceived = false;
  }
  Serial.println("Closed Protocol builder");
}
