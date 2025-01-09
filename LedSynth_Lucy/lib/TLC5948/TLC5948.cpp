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


void TLC5948::set2(uint8_t LEDn, float logi) {

  // Check if LEDn is in usable range
  if(LEDn < 0 || LEDn >= nLEDs) return;

  uint32_t tMASK = mask[LEDn];
  int tDRI  = tMASK >> 16; // take the upper bits for driver

  uint8_t thisledch = __builtin_popcount(tMASK & 0xFFFF) ;

  // override: all leds with 1 channel only; TODO: implement negative values
  thisledch=1;
  uint32_t t = logi2pwm (logi,thisledch)  ;
  
  uint8_t  out_mode   = (t>>29) & 0x0007 ;
  uint8_t  out_nchon  = (t>>24) & 0x001f ;
  uint8_t  out_dc     = (t>>16) & 0x007f ;
  uint16_t out_pwm    =  t      & 0xffff ; 

  // if (debugTLCflag) 
  { Serial.printf ("SET2 logi=%7.3f, led=%2i, nch=%2i -> on=%2i, pwm=%5i, dc=%3i, mode=%2i. ", 
                                     logi, LEDn, thisledch, out_nchon, out_pwm, out_dc, out_mode) ;
  }

  // SET PWM & DC
    for (int i = 0; i < nch; i++) {
      if ((tMASK >> i) & 1)  // if bit in mask is 1
      {
        if (out_nchon>0)
        { 
            cpwm[i + tDRI * nch] = _PWM_MAX_VAL; // max PWM
            cDC[i + tDRI * nch]  = _DC_MAX_VAL;  // max DC
            out_nchon--;
            // if (debugTLCflag) 
            Serial.printf("Max") ;
        }
        else if (out_pwm>0)
        {
            cpwm[i + tDRI * nch] = out_pwm; // update pwm value
            cDC[i + tDRI * nch]  = out_dc;  // update Dot Correction value     
            out_pwm = 0 ;
            // if (debugTLCflag) 
            Serial.printf("Set") ;
        }
        else
        {
            cpwm[i + tDRI * nch] = 0; 
            cDC[i + tDRI * nch]  = 0; 
            // if (debugTLCflag) 
            Serial.printf("Nul") ;
        }
        // not touching global BC for now
      }
    }
    // if (debugTLCflag) 
    Serial.printf("\n") ;
}

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








// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// FUNCTION TO CALCULATE PWM/DC value

/*  
    uint32_t logi2pwm (float logi, uint8_t nch )
    
    finds a good pwm (0-65535) and dc combo for TLC5948a in ESPWM mode. 
    input: logi - decadic logarithm of intensity (larger values are dimmer)
           nch  - number of ganged channels (1 or more)
    return packing of relevant values in 3-5-8-16 bits. 
         ( (out_mode  & 0x07) << 29 ) | 
         ( (out_nchon & 0x1f) << 24 ) | 
         ( (out_dc    & 0x7f) << 16 ) | 
         ( (out_pwm   & 0xffff) );
    
    For a single channel, PWM provides 16 bits, dc 7 bits, so all together there are about 23 bits to play around; 0 bits mean 1 channel at full, 
    22 bits mean very small dc and low pwm. the absolute lowest light would correspond to 22.989 bits = log2 (MAXPWM*MAXDC) of dynamic reserve. 
    The function using input nch=1 abd negative logi values will switch to "multichannel" mode 7 (-0.301 = 2 leds, -0.602 = 4 leds, -1.0 = 10 leds).
    Note 1 (decadic magnitude=logI unit) = 3.3219 bits (=octaves 2x), as per 1/log10(2)), and 2 bits = 0.301 decades.
    
    (PWMSTEP) Granularity 128 (values 128,256,384,512 ...) makes all 512 segments on, flashing produces with maximally reduced low harmonics
    GS Clock 10.0 MHz, Update frequency 150Hz, main harmonic 19.5 kHz
    GS Clock 12.8 MHz, Update frequency 195Hz, main harmonic 25.0 kHz
    GS Clock 16.0 MHz, Update frequency 244Hz, main harmonic 31.2 kHz
    Granularity N directly means that the lowest harmonic will be at the N*update frequency. So we like to have at least 4 for vision purposes.
    The code is looking for a good PWM/DC combo, including several channels being fully on (mode7), before switching off all but a single channel,
    and this is done with descending modes 6 to 1 with decreasing granularities, at the end (mode1) we give up and allow all PWM values.  
*/

#include <stdio.h>
#include <stdint.h>
#include <math.h>

// values related to TLC5948a
#define MAXPWM         65535   // MAXPWM divider is 65535, as per TLC5948A sheet, note that the last (128th) ESPWM segment has 511, not 512 bits!
#define MAXDC          127     // supposedly linear from 1 to 127, let's see 

//                                 mode 7 is used when multiple channels in use
#define BITBOUNDARY0     4      // mode 6 below this boundary 
#define BITBOUNDARY1     7      // mode 5 below this boundary 
#define BITBOUNDARY2    14      // mode 4 below this boundary (search or direct when M4SEARCH is false)
#define BITBOUNDARY3    17      // mode 3 below this boundary  
#define BITBOUNDARY4    19      // mode 2 below this boundary 
#define BITBOUNDARY5    22      // mode 1 below this boundary (last resort) 
                                // mode 0 above this boundary, all off

#define M7FORCEMAXDC    1       // when true, DC=127 is forced with multiple channels in use
#define M6FORCEMAXDC    1       // when true, DC=127 is on single channel mode6
#define M5FORCEMAXDC    0       // when true, DC=127 is on single channel mode5

#define M7STEP          128     // mode 7  PWM granularity when multiple channels are used - (128, fills all the 512 segments)
#define M6STEP          128     // mode 6  PWM granularity when a single channel is used below BITBOUNDARY0 (128) 
#define M5STEP          32      // mode 5  PWM granularity when a single channel is used below BITBOUNDARY1 (16, 32)
#define M4STEP          16      // mode 4  PWM search step  (8, 16) 
#define M3STEP          8       // mode 3  PWM search step  (4,8)   
#define M2STEP          4       // mode 2  PWM granularity  (2,4)
#define M1STEP          1       // mode 1  PWM granularity  (1,2)

// mode 4 - search incrementing, going from M4START to M4END 
#define M4SEARCH        1       // enable/disable search in range BITBOUNDARY1 ... BITBOUNDARY2, enabled: mode 4, diabled: mode 3
#define M4PWM          128      // when search in mode 4 is not on,  fix PWM to this value
#define M4START       128
#define M4END         512
#define M4MINDC         2
#define M4MAXDC       127
#define M4REDUX       0.9       // we only rewrite the next best if error is reduxed ... 1-10%?

// mode 3 - search decrementing, going from M2END to M2START
#define M3SEARCH        1     // enable/disable search in range BITBOUNDARY2 ... BITBOUNDARY3, enabled: mode 2, disabled: mode 1
#define M3START        16 
#define M3END         128+16
#define M3MINDC         1
#define M3MAXDC        16
#define M3REDUX         0.9  // we only rewrite the next best if error is reduxed for ... 1-10%?

#define MAXERR 1   // arbitrary start value for error search in lookups, should be minimally 1
                   // err = fabs (dc*pwm/MAXDC/MAXPWM - pwmfract ), as both elemenents inside fabs are [0..1]


// ----------------------------------------------------------------------------------------------------------------- FUNCTION STARTS HERE

uint32_t TLC5948::logi2pwm (float logi, uint8_t nch ) {
// calculate pwm values for an LED connected to 1-16 channels

if ((nch<1) || (nch>16)) {   // printf("error: nch should be 1..16 \n") ;
   return 0xFFFF;        }

float pwmfloat = pow(10.0,-logi) * (float)(nch);
float pwmfloor = floor(pwmfloat) ;
float pwmfract = pwmfloat - pwmfloor ;

// pwmlog2 holds the "pwm bits" that are used for the mode boundary determination
// floor of pwmlog2 is above or equal to 0 (and say below 22)
// or set to -1 if pwm fraction is exactly 0
int32_t pwmlog2 = -1;
if (pwmfract>0) {      pwmlog2  = (int32_t) floor(-log2(pwmfract)) ;     
                } 

// declare default outputs for a LED turned off
uint8_t  out_nchon = 0; 
uint8_t  out_dc = 0;
uint8_t  out_mode = 0; 
uint32_t out_pwm = 0;                        

out_nchon = (uint8_t)pwmfloor ; 

if (out_nchon>0 )                   // mode 7 (several channels) 
    {       out_mode  = 7;             
            float pwm   = (uint32_t)(round(pwmfract*MAXPWM/M7STEP)*M7STEP ) ;         
            if (pwm>MAXPWM) 
                pwm=MAXPWM ; 
            out_pwm = (uint32_t)pwm ;
            if ( M7FORCEMAXDC )  
                 { out_dc    = MAXDC ; }      
            else { out_dc    = (uint32_t)(round(MAXDC*MAXPWM*pwmfract/pwm)) ; 
                   // if (out_dc>MAXDC) out_dc=MAXDC ;
            }   
    } 

else if (pwmlog2<BITBOUNDARY0)       // mode 6 (single channel, high PWM)
    {       out_mode  = 6;          
            float pwm   = (uint32_t)(round(pwmfract*MAXPWM/M6STEP)*M6STEP ) ;         
            if (pwm>MAXPWM) 
                pwm=MAXPWM ;
            out_pwm = (uint32_t)pwm ;
            if ( M6FORCEMAXDC )  
                 { out_dc    = MAXDC ;}    
            else { out_dc    = (uint32_t)(round(MAXDC*MAXPWM*pwmfract/pwm)) ; 
                   // if (out_dc>MAXDC) out_dc=MAXDC ;
                }
    }

else if (pwmlog2<BITBOUNDARY1) 
    {       out_mode  = 5;          
            float pwm   = (uint32_t)(round(pwmfract*MAXPWM/M5STEP)*M5STEP ) ;         
            if (pwm>MAXPWM) 
                pwm=MAXPWM ;
            out_pwm = (uint32_t)pwm ;
            if ( M5FORCEMAXDC )  
                 { out_dc    = MAXDC ;}    
            else { out_dc    = (uint32_t)(round(MAXDC*MAXPWM*pwmfract/pwm)) ; 
                   // if (out_dc>MAXDC) out_dc=MAXDC ;            
                 }
    }

else if (pwmlog2<BITBOUNDARY2)               // mode 4 search
{    if (M4SEARCH) {
        float minerr=MAXERR;
        float bestdc=0; 
        float bestpwm=0;
        for (int16_t ipwm = M4START ; ipwm<=M4END ; ipwm+=M4STEP  ) {
            float pwm = (float)ipwm;
            float dc  = round (MAXDC*MAXPWM*pwmfract/pwm) ;
            if ((dc>=M4MINDC) && (dc<=M4MAXDC)) {
                float err = fabs  (dc*pwm/MAXDC/MAXPWM - pwmfract ) ;
                if (err<minerr) {
                    minerr=err*M4REDUX;
                    bestdc=dc;
                    bestpwm = pwm;
                }
            }
        }
        out_nchon = (uint8_t)pwmfloor;
        out_dc =    (uint8_t)bestdc; 
        out_pwm =   (uint32_t)bestpwm;
        out_mode =  4; 
    } else {                                    // mode 4 - direct set pwm and calculate the DC
        out_nchon = (uint8_t)pwmfloor ;
        out_pwm   = M4PWM ; 
        out_dc    = (uint8_t)(round (pwmfract * MAXDC * MAXPWM / M4PWM )) ; 
        out_mode  = 4; 
    } 
}
else if ((pwmlog2<BITBOUNDARY3) && M3SEARCH) // mode 2 search, code copied from above but the for loop direction is from End to Start
{   
        float minerr=MAXERR;
        float bestdc=0; 
        float bestpwm=0;
        for (int16_t ipwm = M3END ; ipwm>=M3START ; ipwm-=M3STEP  ) {
            float pwm = (float)ipwm;
            float dc  = round (MAXDC*MAXPWM*pwmfract/pwm) ;
            if ((dc>=M3MINDC) && (dc<=M3MAXDC)) {
                float err = fabs  (dc*pwm/MAXDC/MAXPWM - pwmfract ) ;
                if (err<minerr) {
                    minerr=err*M3REDUX;
                    bestdc=dc;
                    bestpwm = pwm;
                }
            }
        }
        out_nchon = (uint8_t)pwmfloor;
        out_dc    = (uint8_t)bestdc; 
        out_pwm   = (uint32_t)bestpwm;
        out_mode  = 3; 
}
else if (pwmlog2<BITBOUNDARY4)
{    
    out_nchon=0;
    out_pwm = (uint32_t)(round(pwmfract*MAXDC*MAXPWM/M2STEP)*M2STEP) ;
    out_dc = 1;
    out_mode=2;
}

// if none of the if clauses above were taken, then out_dc is 0 - also in the case that the two search modes have effed it up.
if ((out_dc==0) && (pwmlog2<BITBOUNDARY5))          // mode 1 last resort, we set dc to 1 and pwm accordingly; also runs if previous modes returned dc=0
{    
    out_nchon=0;
    out_pwm = (uint32_t)(round(pwmfract*MAXDC*MAXPWM/M1STEP)*M1STEP) ;
    out_dc = 1;
    out_mode=1;
}

// paranoia section :-)
   if (out_dc>0x7F)      { // printf("corr: DCMAX=%i\n",out_dc);   
                           out_dc = 0x7F; 
    }    
/* if (out_pwm>=0xFFFF)  { // printf("corr: PWMAX=%i\n",out_pwm);  
                           out_pwm= 0xFFFF; 
    }
*/

/*
  // print the values for debugging purposes 
  float power  = -log10 ( ((float)(out_dc) * (float)(out_pwm)/MAXDC/MAXPWM + (float)(out_nchon)) / nch ) ;
  float relerr = (power - logi);
  printf("logi=%+5.2f err=%+7.3f ",logi,relerr);
  printf("  %5.2f=%5.2f+%5.2f",pwmfloat,pwmfloor,pwmfract); 
  printf("pwmlog2=%+06.2f : pwr=%+5.2f ",pwmlog2, power);
  printf("mode=%1i onch=%1i dc=%03i pwm=%05i\n", out_mode,out_nchon,out_dc,out_pwm ); 
*/

// packing is 3-5-8-16 bits
return ( ( (out_mode  & 0x07) << 29 ) | 
         ( (out_nchon & 0x1f) << 24 ) | 
         ( (out_dc    & 0x7f) << 16 ) | 
         ( (out_pwm   & 0xffff) ) );
}
