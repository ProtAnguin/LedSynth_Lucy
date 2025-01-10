// TODO: add ESPWM settings to user interface
#define LEDSYNTHNAME          "Lucy"

#define D_nTLCs               3
#define D_NLS                 19                                    // rainbow LEDs

#define TRIGINPIN             14
#define TRIGOUTPIN            17
#define ENVELOPEPIN           15
#define INFOPIN               16

#define SERIAL_BAUD_RATE      115200
#define SERIAL_TIMEOUT        50
#define D_TRIGOUTLEN          5                                     // default trigger length [ms]

#define MIN_ATT_VALUE         -6                                    // minimal allowed attenuation value. Where int(MAX_PWM*MIN_ATT_VALUE) equals zero.

// Defines so the device can do a self reset
#define RESTART_ADDR 0xE000ED0C
#define READ_RESTART() (*(volatile uint32_t *)RESTART_ADDR)
#define WRITE_RESTART(val) ((*(volatile uint32_t *)RESTART_ADDR) = (val))

struct selectedLED {
  int8_t curr = 99;         // nonsense value, since we have 00-20
  bool wrapped = false;
  bool all = false;         // added
  float logVal = 0;         // added
} LED;

uint32_t CHmask[D_NLS] = {   // Channel mask
  //DDLLLLLLLLLLLLLLLL     binary for driver (bits 17+) and LED mask (bits 1-16)
  //001111111111111111     this means that one LED must be connected to a single TLC, which is electrically sensible
  0b001110000000000000, // LED  0    3 ch   xxx
  0b000001111110000000, // LED  1    6 ch   xxxxxx
  0b000000000001100000, // LED  2    2 ch   xx
  0b000000000000010000, // LED  3    1 ch   x
  0b000000000000001000, // LED  4    1 ch   x
  0b000000000000000100, // LED  5    1 ch   x
  0b000000000000000010, // LED  6    1 ch   x
  0b000000000000000001, // LED  7    1 ch   x
  0b011000000000000000, // LED  8    1 ch   x
  0b010100000000000000, // LED  9    1 ch   x
  0b010011110000000000, // LED 10    4 ch   xxxx
  0b010000001111000000, // LED 11    4 ch   xxxx
  0b010000000000111110, // LED 12    5 ch   xxxxx
  0b101100000000000000, // LED 13    2 ch   xx
  0b100011000000000000, // LED 14    2 ch   xx
  0b100000100000000000, // LED 15    1 ch   x
  0b100000010000000000, // LED 16    1 ch   x
  0b100000001000000000, // LED 17    1 ch   x
  0b100000000100000000  // LED 18    1 ch   x
};

// Led reference Index          0      1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18    // LED mapping
uint16_t lambdas[D_NLS] =  {  363,   372,   385,   405,   422,   435,   453,   475,   491,   517,   538,   557,   573,   589,   613,   630,   659,   669,   679};
uint16_t pwmVals[D_NLS] =  {65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535};
uint8_t    isoDC[D_NLS] =  {  127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127,   127}; 
int         mask[D_NLS] =  {    1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1};  // Mask for stimulation
int     adapMask[D_NLS] =  {    1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1};  // Mask for adaptation
uint8_t  isoBC[D_nTLCs] =  {127, 127, 127} ;                

float   Ibanks[2][D_NLS] = { 0 } ;
// we will populate this laters
uint8_t N_MAXBANK = (sizeof(Ibanks)/sizeof(Ibanks[0]))-1; // number of rows in Ibanks
uint8_t N_USEBANK = 0; // index of intensity bank (Ibanks) to be used

//  If your compiler is GCC you can use following "GNU extension" syntax: int array[1024] = {[0 ... 1023] = 5};
//  https://stackoverflow.com/questions/201101/how-to-initialize-all-members-of-an-array-to-the-same-value
// uint16_t pwmVals[D_NLS] = {[0 .. D_NLS] = 65535};
// uint8_t  isoDC[D_NLS] =   {[0 .. D_NLS] = 127} ;                  


/* float   Ibanks[][D_NLS] = {{+0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00},  // Intensity banks (for channel specific attenuation)
                           {-0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25},
                           {-4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00},
                           {+0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00},
                           {+0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00} };
*/ 

