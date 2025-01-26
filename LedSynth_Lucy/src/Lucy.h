// TODO: add ESPWM settings to user interface
// TODO: explain what PWL is in the code
// TODO: add EEPROM capabilities
// TODO: patch code to use LED 1 for first rainbow led (00 reserved for zeroorderchannel)
// TODO: search for <TODO:> and take care of any


#define LEDSYNTHNAME          "Lucy"

#define D_nTLCs               3
#define D_NLS                 20                                    // rainbow LEDs and zero order
#define N_PWL                 4
#define N_ISOBANKS            4

#define TRIGINPIN             14
#define TRIGOUTPIN            17
#define ENVELOPEPIN           15
#define INFOPIN               16

#define SERIAL_BAUD_RATE      115200
#define SERIAL_TIMEOUT        20
#define D_TRIGOUTLEN          5                                     // default trigger length [ms]

#define LED00CHAR             '@'                                   // ASCII goes from 40... '@ABCD...'
#define MAXWAITMS             10000                                 // how long to wait for the trigger in


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
  0b110000000000000000, // ZeroOrder
  0b001110000000000000, // LED  1    3 ch   xxx
  0b000001111110000000, // LED  2    6 ch   xxxxxx
  0b000000000001100000, // LED  3    2 ch   xx
  0b000000000000010000, // LED  4    1 ch   x
  0b000000000000001000, // LED  5    1 ch   x
  0b000000000000000100, // LED  6    1 ch   x
  0b000000000000000010, // LED  7    1 ch   x
  0b000000000000000001, // LED  8    1 ch   x
  0b011000000000000000, // LED  9    1 ch   x
  0b010100000000000000, // LED 10    1 ch   x
  0b010011110000000000, // LED 11    4 ch   xxxx
  0b010000001111000000, // LED 12    4 ch   xxxx
  0b010000000000111110, // LED 13    5 ch   xxxxx
  0b101100000000000000, // LED 14    2 ch   xx
  0b100011000000000000, // LED 15    2 ch   xx
  0b100000100000000000, // LED 16    1 ch   x
  0b100000010000000000, // LED 17    1 ch   x
  0b100000001000000000, // LED 18    1 ch   x
  0b100000000100000000  // LED 19    1 ch   x
};

float    LogIn [N_PWL][D_NLS]  = { 0 } ;  // placeholder for desired logI values if PWL interpolation is used
float    LogOut[N_PWL][D_NLS] = { 0 } ;   // placeholder for output  logI values if PWL interpolation is used to achieve this
float    isoLog[N_ISOBANKS][D_NLS] = { 0 } ;   // placeholders for isoLog banks
int      isoLogCurr = 0 ;

// MARKO STUFF PROTOCOL
// the values should be loaded / stored to EEPROM, so using structs or 
// TODO: TO BE REMOVED, CHANGED

float   Ibanks[2][D_NLS] = { 0 } ;
uint8_t N_MAXBANK = (sizeof(Ibanks)/sizeof(Ibanks[0]))-1; // number of rows in Ibanks
uint8_t N_USEBANK = 0; // index of intensity bank (Ibanks) to be used
// QUESTION: is this actually used by protocols?
// TODO: consider using as 32 bit mask instead? A: What if we use more than 32 LEDs? Lets stick with arrays for now, C is fast.

// Led reference Index         ZO,     1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18     19 // LED mapping
uint16_t lambdas[D_NLS]   = { 999,   363,   372,   385,   405,   422,   435,   453,   475,   491,   517,   538,   557,   573,   589,   613,   630,   659,   669,   679};
int         mask[D_NLS] =  {    1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1};  // Mask for stimulation
int     adapMask[D_NLS] =  {    1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1};  // Mask for adaptation
uint8_t    isoDC[D_NLS] =  {  127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127}; 
uint8_t  isoBC[D_nTLCs] =  {  127,   127,   127} ;
#define MAX_ATT_VALUE         6                                    // minimal allowed attenuation value. Where int(MAX_PWM*MIN_ATT_VALUE) equals zero.
#define OFF_LOG_VALUE         9 // the log values to get the LED to turn off, irrespectible of number of channels

//  If your compiler is GCC you can use following "GNU extension" syntax: int array[1024] = {[0 ... 1023] = 5};
//  https://stackoverflow.com/questions/201101/how-to-initialize-all-members-of-an-array-to-the-same-value
// uint16_t isoLog[D_NLS] = {[0 .. D_NLS] = 65535};
// uint8_t  isoDC[D_NLS] =   {[0 .. D_NLS] = 127} ;                  
