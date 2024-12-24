#include <TLC5948.h>

// Implement the constructor
TLC5948::TLC5948(int nTLCs_in,int nLEDs_in,  uint32_t maskKey[],  int GS_PIN, int LATCH_PIN) {
  nTLCs     = nTLCs_in;
  _GS_PIN   = GS_PIN;
  _LAT_PIN  = LATCH_PIN;
  _SPIset   = SPISettings(_GOAL_SCLK_HZ, MSBFIRST, SPI_MODE0);

  _nRelevantBits = nTLCs * _bw;
  _nFrames = (_nRelevantBits + _frameSize - 1) / _frameSize; // check if this is maybe a dangerous way of getting the nFrames. Should not rely on int rounding while casting.
  _nOverflow = _nFrames * _frameSize - _nRelevantBits;

  outFrames = new uint16_t[_nFrames];
   inFrames = new uint16_t[_nFrames];

  cpwm = new uint16_t[nch * nTLCs];
  cDC  = new uint16_t[nch * nTLCs];
  cBC  = new uint16_t[nTLCs];

  for (int cD = 0; cD < nTLCs; cD++) {
    for (int cC = 0; cC < nch; cC++) {
       cpwm[cC + cD * nch] = _initPWM;
        cDC[cC + cD * nch] = _initDC;
    }
    cBC[cD] = _initBC;
  }
  
  nLEDs = nLEDs_in;
  mask = new uint32_t[nLEDs_in];

  for (int i = 0; i < nLEDs; i++) {
    mask[i] = maskKey[i];
  }
}

void TLC5948::begin() {
  SPI.begin();
  
  pinMode(_LAT_PIN, OUTPUT);
  digitalWrite(_LAT_PIN, LOW);

  pinMode(_GS_PIN, OUTPUT);
  analogWriteFrequency(_GS_PIN, _GOAL_GSCLK_HZ);
  analogWriteResolution(_ANALOG_WRITE_BIT_RES);
  analogWrite(_GS_PIN, (1 << (_ANALOG_WRITE_BIT_RES-1))); // set 50% duty cycle

  update();
}

// Implement other member functions
void TLC5948::update() {
  makeControlValues();
  sendFramesSPI();
  if (printFramesTLCflag) { printFrames(); }
  latch();
  
  makePwmValues();
  sendFramesSPI();
  if (printFramesTLCflag) { printFrames(); }
  latch();
}

void TLC5948::makePwmValues() {
  // Clean outFrames
  for (int i = 0; i < _nFrames; i++) {
    outFrames[i] = 0;
     inFrames[i] = 0;
  }

  int mP = _nOverflow + 1; // master pointer
  for (int cD = nTLCs-1; cD >= 0; cD--) {
    for (int cC = nch; cC >= 0; cC--) {
      uint16_t cV;
      int Pstep;

      if (cC == nch) {
        cV = 0;
        Pstep = 1;
      } else {
        cV = cpwm[cC + cD * nch];
        Pstep = 16;
      }

      int cP = ((mP - 1) % _frameSize) + 1;
      int cP1 = cP;
      int cP2 = cP + Pstep - 1;

      int cS1 = _frameSize - Pstep - cP1 + 1;
      int cS2 = _frameSize - ((cP2 - 1) % _frameSize + 1);

      int cF = ((mP + _frameSize - 1) / _frameSize)-1;
      
      if (debugTLCflag) {
        Serial.printf("MakePWM  Driver:%02d Ch:%02d Val:>%5d< Frame:%3d mP:%5d s1:%3d s2:%3d [%2d:%2d]  \n", cD, cC, cV, cF, mP, cS1, cS2, cP1, cP2);
      }

      if (cS1 < 0) {
        outFrames[cF] |= (cV >> abs(cS1));
      }
      else {
        outFrames[cF] |= (cV << cS1);
      }
      if (cP2 > _frameSize) {
        if (cS2 < 0) {
          outFrames[cF+1] |= (cV >> abs(cS2));
        }
        else {
          outFrames[cF+1] |= (cV << cS2);
        }
      }

      mP += Pstep;
    }
  }
}

void TLC5948::makeControlValues() {
  // Clean outFrames
  for (int i = 0; i < _nFrames; i++) {
    outFrames[i] = 0;
     inFrames[i] = 0;
  }

  int mP = _nOverflow + 1;
  for (int cD = nTLCs-1; cD >= 0; cD--) {
    for (int cC = nch + 14; cC >= 0; cC--) {
      uint16_t cV;
      int Pstep;
      switch (cC) {
        case 30:
          cV = 1; // 1 to go to CONTROL REGISTER
          Pstep = 1;
          break;
        case 29:
          cV = 0b0; // EMPTY zeros
          Pstep = 119;
          break;
        case 28:
          cV = C_PSMODE; // PSMODE
          Pstep = 3;
          break;
        case 27:
          cV = C_OLDENA; // OLDENA
          Pstep = 1;
          break;
        case 26:
          cV = C_IDMCUR; // IDMCUR
          Pstep = 2;
          break;
        case 25:
          cV = C_IDMRPT; // IDMRPT
          Pstep = 1;
          break;
        case 24:
          cV = C_IDMENA; // IDMENA
          Pstep = 1;
          break;
        case 23:
          cV = C_LATTMG; // LATTMG
          Pstep = 2;
          break;
        case 22:
          cV = C_LSDVLT; // LSDVLT
          Pstep = 2;
          break;
        case 21:
          cV = C_LODVLT; // LODVLT
          Pstep = 2;
          break;
        case 20:
          cV = C_ESPWM; // ESPWM
          Pstep = 1;
          break;
        case 19:
          cV = C_TMGRST; // TMGRST <---- use 1 so that LAT has an immediate effect
          Pstep = 1;
          break;
        case 18:
          cV = C_DSPRPT; // DSPRPT
          Pstep = 1;
          break;
        case 17:
          cV = C_BLANK; // BLANK <----
          Pstep = 1;
          break;
        case 16:
          cV = cBC[cD]; // BC
          Pstep = 7;
          break;
        default:
          cV = cDC[cC + cD * nch]; // DC for each channel
          Pstep = 7;
          break;
      }

      int cP = ((mP - 1) % _frameSize) + 1;
      int cP1 = cP;
      int cP2 = cP + Pstep - 1;

      int cS1 = _frameSize - Pstep - cP1 + 1;
      int cS2 = _frameSize - ((cP2 - 1) % _frameSize + 1);

      int cF = ((mP + _frameSize - 1) / _frameSize)-1;
      
      if (debugTLCflag) {
        Serial.printf("MakeCONT Driver:%02d Ch:%02d Val:>%5d< Frame:%3d mP:%5d s1:%3d s2:%3d [%2d:%2d]  \n", cD, cC, cV, cF, mP, cS1, cS2, cP1, cP2);
      }

      if (cS1 < 0) {
        outFrames[cF] |= (cV >> abs(cS1));
      }
      else {
        outFrames[cF] |= (cV << cS1);
      }
      if (cP2 > _frameSize) {
        if (cS2 < 0) {
          outFrames[cF+1] |= (cV >> abs(cS2));
        }
        else {
          outFrames[cF+1] |= (cV << cS2);
        }
      }

      mP += Pstep;
    }
  }
}

void TLC5948::sendFramesSPI() {
  SPI.beginTransaction(_SPIset);
  for (int i = 0; i < _nFrames; i++) {
    inFrames[i] = SPI.transfer16(outFrames[i]);
  }
  SPI.endTransaction();
}

void TLC5948::latch() {
  int latchDelay = 10; // microseconds
  delayMicroseconds(latchDelay);
  digitalWrite(_LAT_PIN, HIGH);
  delayMicroseconds(latchDelay);
  digitalWrite(_LAT_PIN, LOW);
  delayMicroseconds(latchDelay);
}

void TLC5948::printFrames() {
  for (int i = 0; i < _nFrames; i++) {
    Serial.printf("Frame %3d OUT: ", i);
    Serial.print(int16toStr(outFrames[i]));
    Serial.print(" IN: ");
    Serial.print(int16toStr( inFrames[i]));
    Serial.println();
  }
  Serial.println("----------------------------------------------------");
}

String TLC5948::int16toStr(uint16_t var) {
  String strOut = "";
  for (int i = 15; i >= 0; i--)  {
    strOut += ((var >> i) & 1) == 1 ? "1" : "0";
  }
  return strOut;
}

void TLC5948::set(uint8_t LEDn, uint32_t setValue, String setWhat) {
  // use mask and populate --> cpwm cDC and cBC

  int tDRI; // temp driver value
  uint32_t tMASK; // temp mask value

  // Check if LEDn is in usable range
  if(LEDn < 0 || LEDn >= nLEDs) return;

  const char* setWhatCStr = setWhat.c_str();

  tMASK = mask[LEDn];
  tDRI  = tMASK >> 16; // take the upper bits for driver
 
  if (strcmp(setWhatCStr, "P") == 0) {
    for (int i = 0; i < nch; i++) {
      if ((tMASK >> i) & 1) { // if bit in mask is 1
        // _PWM_MIN_VAL is used to avoid stimulus flicker at low PWM (ie set to 4 to keep the ESPWM at 4x the update freq)
        // if below the _PWM_MIN_VAL set to 0 to turn diode off
        if (setValue < _PWM_MIN_VAL) {
          setValue = 0;
        }
        cpwm[i + tDRI * nch] = constrain(setValue, (uint32_t)0, _PWM_MAX_VAL); // update pwm value
      }
    }
  } else if (strcmp(setWhatCStr, "D") == 0) {
    for (int i = 0; i < nch; i++) {
      if ((tMASK >> i) & 1) { // if bit is 1
        cDC[i + tDRI * nch] = constrain(setValue, _DC_MIN_VAL, _DC_MAX_VAL); // update Dot Correction value
      }
    }
  } else if (strcmp(setWhatCStr, "B") == 0) {
    cBC[tDRI] = constrain(setValue,  _BC_MIN_VAL,  _BC_MAX_VAL); // update Brightness Control value
  }
};

void TLC5948::printCPWM() {
  // Header
  Serial.print("Driver:  ");
  for (int cD = 0; cD < nTLCs; cD++) {
    Serial.printf("-%2d@%3dBC-    ", cD, cBC[cD]);
  }
  Serial.println();
  Serial.print("       ");
  for (int cD = 0; cD < nTLCs; cD++) {
    Serial.print("    PWM  DC   ");
  }
  Serial.println();
  // Body
  for (int cC = 0; cC < nch; cC++) {
    Serial.printf("Ch %2d  ", cC);
    for (int cD = 0; cD < nTLCs; cD++) {
      Serial.printf("%7d%4d   ", cpwm[cC + cD * nch], cDC[cC + cD * nch]);
    }
    Serial.println();
  }

  // Print mask
  uint32_t tMASK;
  int tDRI;
  int chPerLED;

  Serial.println();
  Serial.print("0->15:");
  for (int tD = 0; tD < nTLCs; tD++) { // for each driver
    Serial.printf("|<-----%2d----->| ", tD);
  }
  Serial.println();
  for(int cL = 0; cL < nLEDs; cL++) {
    chPerLED = 0;
    tMASK = mask[cL];
    tDRI  = tMASK >> 16; // take the upper bits for driver

    Serial.printf("LED%2d ", cL);
    for (int tD = 0; tD < nTLCs; tD++) { // for each driver
      for (int tC = 0; tC < nch; tC++) { // for each channel
        if ((tDRI == tD) && ((tMASK >> tC) & 1)) { // if bit is 1 and driver is correct
          Serial.print("X");
          chPerLED++;
        } else {
          Serial.print("-");
        }
      }
      Serial.print(" ");
    }
    Serial.printf("%2d ch\n", chPerLED);
  }
}

// Troubleshooting code to address each channel
void TLC5948::setChannel(uint8_t setCh, uint8_t setDr, uint16_t setPWM, uint8_t setDC, uint8_t setBC) {
  // Constrain values
  setCh  = constrain( setCh, 0,   nch-1);
  setDr  = constrain( setDr, 0, nTLCs-1);
  setPWM = constrain(setPWM, _PWM_MIN_VAL, _PWM_MAX_VAL);
  setDC  = constrain( setDC,  _DC_MIN_VAL,  _DC_MAX_VAL);
  setBC  = constrain( setBC,  _BC_MIN_VAL,  _BC_MAX_VAL);

  // Set values
  cpwm[setCh + setDr * nch] = setPWM;
   cDC[setCh + setDr * nch] = setDC;
   cBC[        setDr      ] = setBC;
  
  // Send values to TLC
  update();
  
  // report
  Serial.printf("Ch:%2d Dr:%2d PWM:%5d DC:%3d BC:%3d\n", setCh, setDr, setPWM, setDC, setBC);
}
