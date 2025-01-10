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

// values related to TLC5948a
#define MAXPWM         65535   // MAXPWM divider is 65535, as per TLC5948A sheet, note that the last (128th) ESPWM segment has 511, not 512 bits!
#define MAXDC          127     // supposedly linear from 1 to 127, let's see 
//                                // mode 7 is used when multiple channels in use and on a single channelo with DC
#define M65BITBOUNDARY     4      // mode 6 below this boundary 
#define M54BITBOUNDARY    10      // mode 5 below this boundary 
#define M43BITBOUNDARY    12      // mode 4 below this boundary (M4SUBMODE=1 search, M4SUBMODE=0 direct)
#define M32BITBOUNDARY    17      // mode 3 below this boundary  
#define M21BITBOUNDARY    19      // mode 2 below this boundary 
#define M10BITBOUNDARY    22      // mode 1 below this boundary (last resort), and mode 0 (all off) above

#define M5SUBMODE       1         // mode 5: submode 0 -> pwm fixed to M50FIXPWM; submode 1 -> staircase pwm 2^(16-log2pwm) ; submode 2 -> pwm rounded to M5STEP
#define M4SUBMODE       1         // mode 4: enable/disable search 
#define M3SUBMODE       1         // mode 3: enable/disable search

#define M7STEP          128     // mode 7  PWM granularity when multiple channels are used - (128, fills all the 512 segments)
#define M6STEP          128     // mode 6  PWM granularity when a single channel is used below M65BITBOUNDARY (128) 
#define M51STEP         128     // mode 51 PWM granularity 
#define M52STEP         128     // mode 52 PWM granularity 
#define M50FIXPWM      2048     // mode 50 PWM fixed to this value in M5SUBMODE=0
#define M40FIXPWM        64     // mode 40 PWM fixed to this value in M4SUBMODE=0
#define M4STEP          128     // mode 41 PWM search step (8-128) and boundaries
#define M4MINPWM        128
#define M4MAXPWM        128*4
#define M3STEP          16      // mode 3  PWM search step (4,8,16) and boundaries
#define M3MINPWM        16 
#define M3MAXPWM       256
#define M2STEP          4       // mode 2  PWM granularity  (2,4)
#define M1STEP          1       // mode 1  PWM granularity  (1,2)

//settings for DC
#define M7FORCEMAXDC    1       // mode 7:           1 -> DC=127 with multiple channels in use
#define M6FORCEMAXDC    1       // mode 6:           1 -> DC=127 on single channel mode6
#define M5FORCEMAXDC    0       // mode 5 submode 2: 1 -> DC=127 
#define M4MINDC         2
#define M4MAXDC       127
#define M3MINDC         1
#define M3MAXDC        16

#define M4REDUX       0.99      // we only rewrite the next best if error is reduxed for 1-10%
#define M3REDUX       0.90      // we only rewrite the next best if error is reduxed for 1-10%
#define MAXERR           1      // arbitrary start value for error search in lookups, should be minimally 1
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

if (out_nchon>0 )                   // mode 6 (several channels) 
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

else if (pwmlog2<M65BITBOUNDARY)       // mode 6 (single channel, high PWM)
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

else if (pwmlog2<M54BITBOUNDARY)        // mode 5 (single channel, medium PWM)
    {       out_mode  = 5;  
            if (M5SUBMODE==0)               // fix PWM to 128, find DC
            {
                float pwm   = M50FIXPWM ;                         
                out_pwm     = (uint32_t)pwm;
                out_dc      = (uint32_t)(round(MAXDC*MAXPWM/M50FIXPWM*pwmfract));
            }
            else if (M5SUBMODE==1)      // 
            {
                float pwm = round(pow (2, (16-pwmlog2) )/M51STEP)*M51STEP ;
                out_pwm     = (uint32_t)pwm;
                out_dc      = (uint32_t)(round(MAXDC*MAXPWM/pwm*pwmfract));
            }
            else if (M5SUBMODE==2)
            {
            float pwm   = (uint32_t)(round(pwmfract*MAXPWM/M52STEP)*M52STEP ) ;         
            if (pwm>MAXPWM) 
                pwm=MAXPWM ;
            out_pwm = (uint32_t)pwm ;
            if ( M5FORCEMAXDC )  
                 { out_dc    = MAXDC ;}    
            else { out_dc    = (uint32_t)(round(MAXDC*MAXPWM*pwmfract/pwm)) ; 
                   // if (out_dc>MAXDC) out_dc=MAXDC ;            
                 }
            }

    }

else if (pwmlog2<M43BITBOUNDARY)               // mode 4 search
{    if (M4SUBMODE) {
        float minerr=MAXERR;
        float bestdc=0; 
        float bestpwm=0;
        // for (int16_t ipwm = M4MINPWM ; ipwm<=M4MAXPWM ; ipwm+=M4STEP  ) {
        for (int16_t ipwm = M4MAXPWM ; ipwm>=M4MINPWM ; ipwm-=M4STEP  ) {
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
        out_pwm   = M40FIXPWM ; 
        out_dc    = (uint8_t)(round (pwmfract * MAXDC * MAXPWM / M40FIXPWM )) ; 
        out_mode  = 4; 
    } 
}
else if ((pwmlog2<M32BITBOUNDARY) && M3SUBMODE) // mode 2 search, code copied from above but the for loop direction is from End to Start
{   
        float minerr=MAXERR;
        float bestdc=0; 
        float bestpwm=0;
        for (int16_t ipwm = M3MAXPWM ; ipwm>=M3MINPWM ; ipwm-=M3STEP  ) {
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
else if (pwmlog2<M21BITBOUNDARY)
{    
    out_nchon=0;
    out_pwm = (uint32_t)(round(pwmfract*MAXDC*MAXPWM/M2STEP)*M2STEP) ;
    out_dc = 1;
    out_mode=2;
}

// if none of the if clauses above were taken, then out_dc is 0 - also in the case that the two search modes have effed it up.
if ((out_dc==0) && (pwmlog2<M10BITBOUNDARY))          // mode 1 last resort, we set dc to 1 and pwm accordingly; also runs if previous modes returned dc=0
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
   if (out_pwm>=0xFFFF)  { // printf("corr: PWMAX=%i\n",out_pwm);  
                           out_pwm= 0xFFFF; 
    }

// packing is 3-5-8-16 bits
return ( ( (out_mode  & 0x07) << 29 ) | 
         ( (out_nchon & 0x1f) << 24 ) | 
         ( (out_dc    & 0x7f) << 16 ) | 
         ( (out_pwm   & 0xffff) ) );
}                                