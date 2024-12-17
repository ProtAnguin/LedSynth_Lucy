#define LEDSYNTHNAME          "Lucy"

#define D_nTLCs               3
#define D_NLS                 19                                    // rainbow LEDs

#define TRIGINPIN             14
#define TRIGOUTPIN            17
#define ENVELOPEPIN           15

#define SERIAL_BAUD_RATE      115200
#define SERIAL_TIMEOUT        50
#define D_TRIGOUTLEN          5                                     // default trigger length [ms]

#define MIN_ATT_VALUE         -5                                    // minimal allowed attenuation value. Where int(MAX_PWM*MIN_ATT_VALUE) equals zero.

// Defines so the device can do a self reset
#define RESTART_ADDR 0xE000ED0C
#define READ_RESTART() (*(volatile uint32_t *)RESTART_ADDR)
#define WRITE_RESTART(val) ((*(volatile uint32_t *)RESTART_ADDR) = (val))

uint32_t CHmask[D_NLS] = {   // Channel mask
  //DDLLLLLLLLLLLLLLLL     binary for driver (bits 17+) and LED mask (bits 1-16)
  0b001110000000000000, // LED  0
  0b000001111110000000, // LED  1
  0b000000000001100000, // LED  2+
  0b000000000000010000, // LED  3
  0b000000000000001000, // LED  4
  0b000000000000000100, // LED  5
  0b000000000000000010, // LED  6
  0b000000000000000001, // LED  7+
  0b011000000000000000, // LED  8
  0b010100000000000000, // LED  9
  0b010011110000000000, // LED 10+
  0b010000001111000000, // LED 11+
  0b010000000000111110, // LED 12+
  0b101100000000000000, // LED 13
  0b100011000000000000, // LED 14
  0b100000100000000000, // LED 15
  0b100000010000000000, // LED 16
  0b100000001000000000, // LED 17
  0b100000000100000000  // LED 18
};

uint8_t isoDC[D_NLS] = {
  127,  // LED  0
   58,  // LED  1
   26,  // LED  2+
   95,  // LED  3
   56,  // LED  4
   86,  // LED  5
   41,  // LED  6
   42,  // LED  7+
   34,  // LED  8
   52,  // LED  9
   32,  // LED 10+
   38,  // LED 11+
   24,  // LED 12+
   28,  // LED 13
   23,  // LED 14
   18,  // LED 15
   15,  // LED 16
   20,  // LED 17
   50   // LED 18
};

uint8_t isoBC[D_nTLCs] = {
  127, // TLC 0b00
  127, // TLC 0b01
  127  // TLC 0b10
};

// Led reference Index          0      1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18    // LED mapping
uint16_t pwmVals[D_NLS] =  {60303, 57638, 60704, 62806, 61087, 64483, 60358, 59394, 58787, 56649, 60527, 61221, 63463, 58230, 59984, 64105, 65535, 63789, 61139};
uint16_t lambdas[D_NLS] =  {  363,   372,   385,   405,   422,   435,   453,   475,   491,   517,   538,   557,   573,   589,   613,   630,   659,   669,   679};
int         mask[D_NLS] =  {    1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1};  // Mask for stimulation
int     adapMask[D_NLS] =  {    1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,     1};  // Mask for adaptation
float   Ibanks[][D_NLS] = {{+0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00},  // Intensity banks (for channel specific attenuation)
                           {-0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25, -0.25},
                           {-4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00, -4.00},
                           {+0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00},
                           {+0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00} };
uint8_t N_MAXBANK = (sizeof(Ibanks)/sizeof(Ibanks[0]))-1; // number of rows in Ibanks
uint8_t N_USEBANK = 0; // index of intensity bank (Ibanks) to be used

struct selectedLED {
  int8_t curr = 1;
  bool wrapped = false;
} LED;
