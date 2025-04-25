#include <TLC5948.h>
#include <stdint.h>
#include <math.h>

// Implement the constructor
TLC5948::TLC5948(int nTLCs_in,int nLEDs_in,  uint32_t maskKey[],  int GS_PIN, int LATCH_PIN) {
  nTLCs     = nTLCs_in;
  _GS_PIN   = GS_PIN;
  _LAT_PIN  = LATCH_PIN;

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
  // SPI.begin();
  
  pinMode(_LAT_PIN, OUTPUT);
  digitalWrite(_LAT_PIN, LOW);

  setGSCLK (GOAL_GSCLK_HZ ) ; 
  // analogWriteFrequency(_GS_PIN, _GOAL_GSCLK_HZ);                                // TODO: make frequency user - selectable!!!!
  // analogWriteResolution(_ANALOG_WRITE_BIT_RES);
  // analogWrite(_GS_PIN, (1 << (_ANALOG_WRITE_BIT_RES-1))); // set 50% duty cycle

  // clean up the chips?
  update();
  update();
}

/* 
 NOTE that this function does not return the ACTUAL set frequency
 NOTE: the electrical calibration of LEDs DOES DEPEND on the GSCLK frequency and is different for PWM/ESPWM 
 PP tested 20250314 - 
 GCLK frequencies (Mhz) that can be set with the Teensy 4.1 running at 600 Mhz have been approximately measured with an oscilloscope
 I obtained 30.0, 25.0, [], 18.8, 16.4, 14.9, 13.7, 12.5, 11.5, 10.7, 10.0, 8.79, 7.93, 7.20, 6.47, 5.98 .... 
 
This seems to correspond to 150 MHz divided by a divisor which is in the range [5..25]
 
If this is true, then the exact frequencies are:
Clock MHz	Divisor 150/	Cycle (ms)	Update (Hz)
30.000	5	2.185	457.771
25.000	6	2.621	381.476
21.429	7	3.058	326.979
18.750	8	3.495	286.107
16.667	9	3.932	254.317
15.000	10	4.369	228.885
13.636	11	4.806	208.078
12.500	12	5.243	190.738
11.538	13	5.680	176.066
10.714	14	6.117	163.490
10.000	15	6.554	152.590
9.375	16	6.990	143.053
8.824	17	7.427	134.638
8.333	18	7.864	127.159
7.895	19	8.301	120.466
7.500	20	8.738	114.443
7.143	21	9.175	108.993
6.818	22	9.612	104.039
6.522	23	10.049	99.515
6.250	24	10.486	95.369
6.000	25	10.923	91.554
5.769	26	11.359	88.033
5.556	27	11.796	84.772
5.357	28	12.233	81.745
5.172	29	12.670	78.926
5.000	30	13.107	76.295
4.839	31	13.544	73.834
4.688	32	13.981	71.527

30 MHz transmission is iffy 
I would recommend 25 MHz for PWM and 10 for ESPWM

TODO: SHOULD REPORT THE ACTUAL FREQUENCY!!!!!!
WIRING: consider using a FLEXPWM pin as described in the code in the links

how is the frequency determined by Resolution and clock??
https://www.pjrc.com/teensy/td_pulse.html
https://forum.pjrc.com/index.php?threads/teensy-4-1-available-pwm-frequencies.63168/
https://forum.pjrc.com/index.php?threads/using-teensy-4-1-to-generate-square-wave-at-precise-frequency.75798/

*/


float TLC5948::setGSCLK( float frequency ) {
  float f = constrain ( frequency , GSCLK_MIN_HZ, GSCLK_MAX_HZ ) ;
  pinMode(_GS_PIN, OUTPUT);
  analogWrite(_GS_PIN, 0) ;
  analogWriteFrequency(_GS_PIN, f ) ;                                
  analogWriteResolution(_ANALOG_WRITE_BIT_RES);
  analogWrite(_GS_PIN, (1 << (_ANALOG_WRITE_BIT_RES-1))); // set 50% duty cycle
  return f ;                                                                   
}

void TLC5948::latch() {
  int latchDelay = LATCH_DELAY_US; // microseconds                                  
  delayMicroseconds(latchDelay);
  digitalWrite(_LAT_PIN, HIGH);
  delayMicroseconds(latchDelay);
  digitalWrite(_LAT_PIN, LOW);
  delayMicroseconds(latchDelay);
}

void TLC5948::sendFramesSPI() {
  SPI.beginTransaction( SPISettings(GOAL_SCLK_HZ, MSBFIRST, SPI_MODE0) );
  for (int i = 0; i < _nFrames; i++) {
    inFrames[i] = SPI.transfer16(outFrames[i]);
  }
  SPI.endTransaction();
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
      
      if (printFramesTLCflag) {
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
      
      if (printFramesTLCflag) {
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


void TLC5948::change(char changeWhatChar, uint8_t LEDn, uint32_t setValue) {
  // use mask and populate --> cpwm cDC and cBC

  int tDRI; // temp driver value
  uint32_t tMASK; // temp mask value

  // Check if LEDn is in usable range
  if(LEDn < 0 || LEDn >= nLEDs) return;

  tMASK = mask[LEDn];
  tDRI  = tMASK >> 16; // take the upper bits for driver
 
  switch(changeWhatChar) {
    case 'P': // Update PWM values
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
      break;

    case 'D': // Update Dot Correction values
      for (int i = 0; i < nch; i++) {
        if ((tMASK >> i) & 1) { // if bit is 1
          cDC[i + tDRI * nch] = constrain(setValue, _DC_MIN_VAL, _DC_MAX_VAL); // update Dot Correction value
        }
      }
      break;

    case 'B': // Update Brightness Control values
      cBC[tDRI] = constrain(setValue,  _BC_MIN_VAL,  _BC_MAX_VAL); // update Brightness Control value
      break;
  }
};

void TLC5948::setlog(uint8_t LEDn, float logi) {
  // Check if LEDn is in usable range
  if(LEDn < 0 || LEDn > nLEDs) return;

  uint32_t tMASK = mask[LEDn];
  int tDRI  = tMASK >> 16; // take the upper bits for driver
  int8_t goalIndex = 0;

  uint8_t thisledch = __builtin_popcount(tMASK & 0xFFFF) ; // get numer of channels

  uint32_t t = logi2pwm (logi,thisledch); // get settings for channels
  
  uint8_t  out_mode   = (t>>29) & 0x0007 ;
  uint8_t  out_nchon  = (t>>24) & 0x001f ;
  uint8_t  out_dc     = (t>>16) & 0x007f ;
  uint16_t out_pwm    =  t      & 0xffff ;

  if (debugTLCflag) {
    Serial.printf ("logi=%7.3f, led=%02i, nch=%02i -> on=%02i, pwm=%05i, dc=%03i, mode=%1i. ", 
      logi,
      LEDn,
      thisledch,
      out_nchon,
      out_pwm,
      out_dc,
      out_mode) ;
  }

  // SET PWM & DC
  for (int i = 0; i < nch; i++) {
    goalIndex = i + tDRI * nch;
    
    if ((tMASK >> i) & 1) { // if bit in mask is 1
      if (out_nchon>0) {
        out_nchon--;
        cpwm[goalIndex] = _PWM_MAX_VAL; // max PWM
          cDC[goalIndex] = _DC_MAX_VAL;  // max DC
        if (debugTLCflag) Serial.printf("2") ;
      }
      else if (out_pwm>0) {
        cpwm[goalIndex] = out_pwm; // update pwm value
          cDC[goalIndex] = out_dc;  // update Dot Correction value
        out_pwm = 0;
        if (debugTLCflag) Serial.printf("1") ;
      }
      else {
        cpwm[goalIndex] = 0;
          cDC[goalIndex] = 0;
        if (debugTLCflag) Serial.printf("0") ;
      }
      // not touching global BC for now
    }
  }
  if (debugTLCflag) Serial.printf("\n") ;
}


String TLC5948::int16toStr(uint16_t var) {
  String strOut = "";
  for (int i = 15; i >= 0; i--)  {
    strOut += ((var >> i) & 1) == 1 ? "1" : "0";
  }
  return strOut;
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

void TLC5948::printCPWM() {
  // Header
  // Serial.println();
  // Serial.print("       ");
  // for (int cD = 0; cD < nTLCs; cD++) {    Serial.print("    PWM  DC   ");  }
  // Serial.println();
  // Body
  for (int cC = 0; cC < nch; cC++) {
    Serial.printf("Ch%02d ", cC);
    for (int cD = 0; cD < nTLCs; cD++) {
      Serial.printf("%05d %03d   ", cpwm[cC + cD * nch], cDC[cC + cD * nch]);
    }
    Serial.println();
  }
  Serial.print("Driver:BC =");
  for (int cD = 0; cD < nTLCs; cD++) {
    Serial.printf("%02d:%03d  ", cD, cBC[cD]);
  }
  Serial.println();
}

void TLC5948::printMask() {   // Print mask
  uint32_t tMASK;
  int tDRI;
  int chPerLED;

  // for (int tD = 0; tD < nTLCs; tD++) { Serial.printf("|<-----%02d----->| ", tD); }
  // Serial.println();
  for(int cL = 0; cL < nLEDs; cL++) {
    chPerLED = 0;
    tMASK = mask[cL];
    tDRI  = tMASK >> 16; // take the upper bits for driver
    Serial.printf("LED%02d (%c): ", cL, ('@'+cL) );
    for (int tD = 0; tD < nTLCs; tD++) { // for each driver
      for (int tC = 0; tC < nch; tC++) { // for each channel
        if ((tDRI == tD) && ((tMASK >> tC) & 1)) { // if bit is 1 and driver is correct
          Serial.print("1");
          chPerLED++;
        } else {
          Serial.print("0");
        }
      }
      Serial.print(" ");
    }
    Serial.printf(" [%02d] LED%02d (%c)\n", chPerLED, cL, ('@'+cL));
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

void TLC5948::printLogi2pwm(float logi) {
// helper function to print the PWM/DC values, e.g. with a for loop
// printf ("#logi, out_nchon, out_pwm, out_dc, out_mode, log10(in_power), log10(out_power), err_power#\n");
// for ( double logi=LOGISTART ; logi<=LOGIEND ; logi+=LOGISTEP ) printpwm(logi) ;

#define NCHANNELS 1
#define MAXPWM 65535
#define MAXDC  127
#define MAXRELERR 0.0

        uint32_t t = logi2pwm ((float)(logi),NCHANNELS)  ;
        uint8_t  out_mode   = (t>>29) & 0x0007 ;
            uint8_t  out_nchon  = (t>>24) & 0x001f ;
            uint8_t  out_dc     = (t>>16) & 0x007f ;
            uint16_t out_pwm    =  t      & 0xffff ; 

            float    in_power   =          (pow (10, -logi) * NCHANNELS * MAXPWM * MAXDC)       ; 
            float    out_power  = (float)  ( out_pwm * out_dc ) + (out_nchon * MAXPWM * MAXDC)  ;
            float    err_power  = log10 (out_power) - log10 (in_power) ;

            if  ( ((fabs(err_power)) >=MAXRELERR) ) {
                    Serial.printf ("%7.3f, %2i, %5i, %3i, %2i, %7.5f, %7.5f, %+7.5f \n", 
                         logi,  out_nchon, out_pwm, out_dc, out_mode, log10(in_power), log10(out_power), err_power) ;                
            }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// BIG KAHOONA PRIMZI FUNCTION TO CALCULATE PWM/DC value
#include "logi2pwm.h"
