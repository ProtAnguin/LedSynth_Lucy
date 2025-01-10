/* void mainHelp() {
  Serial.println("- MAIN HELP --------------------------------------------------------------------------------------");
  Serial.println("  'prot' to enter the Protocol builder");
  //Serial.println("  'galois' to enter GALOIS part");
  Serial.println();
  Serial.println("  'pwmget' to look at the current PWM values");
  //Serial.println("  'pwmsave' to SAVE the current PWM values to EEPROM");
  //Serial.println("  'pwmpeek' to PEEK at the current PWM values in EEPROM");
  //Serial.println("  'pwmload' to LOAD the current PWM values from EEPROM");
  Serial.println("  'pwmxxxxxyyyyyzzzzz...' to change pwm values (reset deletes the new values so use pwmsave");
  Serial.println("       before and pwmload each time after reset). This is a looooooong ");
  Serial.println("       command as for example, 1 must be written as 00001.");
  Serial.println("  'setXXYYYY' to set the XX led to YYYY pwm value (set050050 is the same as set0550)");
  Serial.println("      special use: set00YYYY set the zero order diode to YYYY");
  Serial.println("  'allXXXX'   to set all LEDs to XXXX pwm (0-4095)");
  Serial.println("            Special use: 'all-1' sets all LEDs to isoQ values, -2 to isoQ/2, -3 to isoQ/4 ");
  Serial.println("  'oeX' and 'zeX' to set the output enable");
  Serial.println("  'bankX' to select the index of attenuation bank to use");
  Serial.println();
  Serial.println("  'welc' to see the welcome screen");
  Serial.println("  'print' to see the PWM, DC, BC and connection data");
  Serial.println("  'dirChDrPwwmmDcrBcr to DIRectly address a single channel. example: <dir030100123127127*>");
  Serial.println("  'debug' to toggle TLC debugging output (this command does not report state)");
  Serial.println("  'frpri' to toggle TLC frame printing output (this command does not report state)");
  Serial.println();
  Serial.println("  'help' to see this again");
  Serial.println("  'reset' to reset the device");
  Serial.println("-------------------------------------------------------------------------------------------- END -");
}

//                                                                                                                                  main welcome
void mainWelcome() {

  Serial.println();
  Serial.println( LEDSYNTHNAME " (built @ " __DATE__ " " __TIME__") has " + String(D_NLS) + " LEDs.");
  Serial.println();
}
*/