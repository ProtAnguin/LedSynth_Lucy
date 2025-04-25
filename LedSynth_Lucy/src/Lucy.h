// TODO: label the BNC pins on the front plate
// TODO: write up an user manual 
// TODO: search for <TODO:> and take care of any
// TODO: make a command for reporting all settings
// TODO: empty todo, just to make a new commit and learn syncing

#define LEDSYNTHNAME          "Lucy"

#define D_nTLCs               1
#define D_NLS                 11                                    // rainbow LEDs and zero order
#define N_PWL                 4
#define N_ISOBANKS            10

#define TRIGINPIN             14
#define TRIGOUTPIN            17
#define ENVELOPEPIN           15
#define INFOPIN               16

#define SERIAL_BAUD_RATE      115200
#define SERIAL_TIMEOUT        20
#define D_TRIGOUTLEN          5                                     // default trigger length [ms]

#define LED00CHAR             '@'                                   // ASCII goes from 40... '@ABCD...'
#define MAXWAITMS             10000                                 // how long to wait for the trigger in used only in waitTrigUpdate for debugging the trigger

// Defines so the device can do a self reset
#define RESTART_ADDR 0xE000ED0C
#define READ_RESTART() (*(volatile uint32_t *)RESTART_ADDR)
#define WRITE_RESTART(val) ((*(volatile uint32_t *)RESTART_ADDR) = (val))


struct selectedLED {
  int8_t   curr    = 0;           // 
  float    logVal  = 0;           // added
  uint16_t pwmVal  = 0;           // added ;                         TODO: check if all pwmVal are typed as uint16_t
  uint8_t  dcVal   = 0;           // added ;                         TODO; check if all dcVal  are typed as uint8_t   or int8_t (can be signed as max=127)
  uint8_t  bcVal   = 0;           // added ;                         TODO; check if all dcVal  are typed as uint8_t   or int8_t (can be signed as max=127)  
  bool     wrapped = false;
  bool     all     = false;      // replacement for "all" implementation of the rainbow
  // bool     autoupdate = true; // perhaps as update1 update0
  // bool     usemask = false; 
  // uint32_t mask    = 0 ;      // if we implement the possibility to have the mask used, then this could replace and expand the rainbow option, e.g.
                                 // mask1111000000000000 = first 4 leds should be parsed as ABCD (without the bloody asterisks!)
} LED;

uint32_t CHmask[D_NLS] = {   // Channel mask
  //DDLLLLLLLLLLLLLLLL     binary for driver (bits 17+) and LED mask (bits 1-16) this means that one LED must be connected to a single TLC, which is electrically sensible
  0b0000000000000000000, // ZeroOrder              
  0b0000000000011111111, // LED  1
  0b0001111111100000000, // LED  2
  0b0010000000011111111, // LED  3
  0b0011111111100000000, // LED  4
  0b0100000000011111111, // LED  5
  0b0101111111100000000, // LED  6
  0b0110000000011111111, // LED  7
  0b0111111111100000000, // LED  8
  0b1000000000011111111, // LED  9
  0b1001111111100000000, // LED 10
};

float    LogIn [N_PWL][D_NLS]  = { 0 } ;  // placeholder for desired logI values if PWL interpolation is used
float    LogOut[N_PWL][D_NLS] = { 0 } ;   // placeholder for output  logI values if PWL interpolation is used to achieve this
float    isoLog[N_ISOBANKS][D_NLS];  // placeholders for isoLog banks, it will be populated in Setup by loadFromEEPROM
int      isoLogCurr = 0; // which line of isoLog to read values from

// QUESTION: is this actually used by protocols?

// Led reference Index         ZO,     1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18     19 // LED mapping
uint16_t lambdas[D_NLS]   = { 999,   363,   372,   385,   405,   422,   435,   453,   475,   491,   517};
int         mask[D_NLS] =  {    1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1};  // Mask for stimulation
int     adapMask[D_NLS] =  {    1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1};  // Mask for adaptation
uint8_t    isoDC[D_NLS] =  {  127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127}; 
uint8_t  isoBC[D_nTLCs] =  {  127 } ;
#define MAX_ATT_VALUE         6 // minimal allowed attenuation value. Where int(MAX_PWM*MIN_ATT_VALUE) equals zero.
#define OFF_LOG_VALUE         9 // the log values to get the LED to turn off, irrespectible of number of channels
