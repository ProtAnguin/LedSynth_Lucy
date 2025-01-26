/* TODO
- control functions with limiters in the calls (ESPWM can only accept 3 bits and such)
- add a description of class, that it was tested on Teensy 4.1 only ...
*/

#ifndef TLC5948_LIB_H
#define TLC5948_LIB_H
#include <SPI.h>
#include <math.h>

#define LATCHDELAYUS 10

// Define the class
class TLC5948 {
  public:
    TLC5948(int nTLCs_in, int nLEDs_in, uint32_t maskKey[], int GS_PIN = 1, int LATCH_PIN = 5);

    const int nch = 16;
    int nLEDs;
    int nTLCs;

    uint16_t MAX_PWM = 65535;
    uint8_t  MAX_DC  = 127;
    uint8_t  MAX_BC  = 127;

    uint16_t *outFrames; // Dynamically allocated array
    uint16_t *inFrames; // Dynamically allocated array
    uint16_t *cpwm;
    uint16_t *cDC;
    uint16_t *cBC;
    uint32_t *mask; // dinamically allocated array

    uint8_t C_PSMODE = 0; // 3 bits
    uint8_t C_OLDENA = 0; // 1 bit
    uint8_t C_IDMCUR = 2; // 2 bits
    uint8_t C_IDMRPT = 0; // 1 bit
    uint8_t C_IDMENA = 0; // 1 bit
    uint8_t C_LATTMG = 3; // 2 bits
    uint8_t C_LSDVLT = 1; // 2 bits
    uint8_t C_LODVLT = 1; // 2 bits
    uint8_t C_ESPWM  = 1; // 1 bit
    uint8_t C_TMGRST = 1; // 1 bit
    uint8_t C_DSPRPT = 1; // 1 bit
    uint8_t C_BLANK  = 1; // 1 bit
    
    bool debugTLCflag = false;
    bool printFramesTLCflag = false;

    // Other member functions
    void update();
    void begin();
    float setGSCLK( float frequency );
    void makePwmValues();
    void makeControlValues();
    void latch();
    void sendFramesSPI();
    void printFrames();
    String int16toStr(uint16_t var);
    void change(char changeWhatChar, uint8_t LEDn, uint32_t setValue);
    void setlog(uint8_t LEDn, float logi);
    void printCPWM();
    void printMask();
    void setChannel(uint8_t setCh, uint8_t setDr, uint16_t setPWM, uint8_t setDC, uint8_t setBC);
    uint32_t logi2pwm (float logi, uint8_t nch );
    void printLogi2pwm(float logi);

  private:
    const uint16_t _initPWM     = 0;
    const uint16_t _initDC      = 0;
    const uint16_t _initBC      = 127;

    int _nRelevantBits;
    int _nFrames;
    int _nOverflow;
    int _GS_PIN;
    int _LAT_PIN;
    uint _ANALOG_WRITE_BIT_RES  = 2;
    int _GOAL_SCLK_HZ           = 10000000; // Serial clock speed in Hz
    int _frameSize              = 16; // sending with transfer16 (hardcoded and has to be 16 for now)
    int _bw                     = 257; // width of bits per driver
    uint _totnch;
    
    uint16_t _PWM_MIN_VAL = 0;
    uint16_t _PWM_MAX_VAL = MAX_PWM;
    uint8_t   _DC_MIN_VAL = 0;
    uint8_t   _DC_MAX_VAL = MAX_DC;
    uint8_t   _BC_MIN_VAL = 0;
    uint8_t   _BC_MAX_VAL = MAX_BC;

    float _GOAL_GSCLK_MHZ = 10; // Gray scale closk speed in MHz (10 MHz clock gives 152 Hz repeat, with PWM > 4 it brings it up to 600 Hz)
    float _GSCLK_MIN_MHZ = 1 ;
    float _GSCLK_MAX_MHZ = 14.2 ;                  // PROLLY NUT NECESSARY TO HAVE THESE AS VARIABLES?

    SPISettings _SPIset;
};

#endif // ifndef TLC5948_LIB_H
