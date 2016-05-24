#include "Renderer.hh"

namespace openmsx {

/*
TMS99X8A palette.
Source: TMS9918/28/29 Data Book, page 2-17.
        http://en.wikipedia.org/wiki/List_of_8-bit_computer_hardware_palettes

  Originally we used the conversion for MESS by R. Nabet, though those colors
  were too saturated, especially color 8 (medium red) was very different
  compared to the real TMS9918 palette.

Now we use the palette found on wikipedia. (Though it was still some work to
figure out how to get from the voltage levels specified in the TMS9918
datasheet to the wikipedia RGB values).

Color            Y   R-Y  B-Y     Y     Pb    Pr     Y     Pb    Pr     R    G    B
0 Transparent
1 Black         0.00 0.47 0.47   0.00  0.00  0.00   0.00  0.00  0.00   0.00 0.00 0.00
2 Medium green  0.53 0.07 0.20   0.53 -0.25 -0.38   0.53 -0.14 -0.20   0.24 0.72 0.29
3 Light green   0.67 0.17 0.27   0.67 -0.19 -0.28   0.67 -0.10 -0.15   0.46 0.81 0.49
4 Dark blue     0.40 0.40 1.00   0.40  0.50 -0.07   0.40  0.27 -0.04   0.35 0.33 0.88
5 Light blue    0.53 0.43 0.93   0.53  0.43 -0.04   0.53  0.23 -0.02   0.50 0.46 0.95
6 Dark red      0.47 0.83 0.30   0.47 -0.16  0.34   0.47 -0.09  0.18   0.73 0.37 0.32
7 Cyan          0.73 0.00 0.70   0.73  0.22 -0.44   0.73  0.12 -0.24   0.39 0.86 0.94
8 Medium red    0.53 0.93 0.27   0.53 -0.19  0.43   0.53 -0.10  0.23   0.86 0.40 0.35
9 Light red     0.67 0.93 0.27   0.67 -0.19  0.43   0.67 -0.10  0.23   1.00 0.54 0.49
A Dark yellow   0.73 0.57 0.07   0.73 -0.38  0.09   0.73 -0.20  0.05   0.80 0.76 0.37
B Light yellow  0.80 0.57 0.17   0.80 -0.28  0.09   0.80 -0.15  0.05   0.87 0.82 0.53
C Dark green    0.47 0.13 0.23   0.47 -0.23 -0.32   0.47 -0.12 -0.17   0.23 0.64 0.25
D Magenta       0.53 0.73 0.67   0.53  0.19  0.25   0.53  0.10  0.13   0.72 0.40 0.71
E Gray          0.80 0.47 0.47   0.80  0.00  0.00   0.80  0.00  0.00   0.80 0.80 0.80
F White         1.00 0.47 0.47   1.00  0.00  0.00   1.00  0.00  0.00   1.00 1.00 1.00

The first 3 columns are the voltage levels specified in the TMS datasheet.

Next 3 columns convert voltages to YPbPr values. R-Y,B-Y=0.47 seems to
correspond to Pb,Pr=0. Also scaled Pb,Pr so that they are in the range
[-0.5..+0.5]. Y was already in range [0..1].

Next the Pb,Pr values are scaled with a factor 0.54 (saturation level 54%),
this makes sure that in the next step the R,G,B values stay in range [0..1].

In the last 3 columns we convert YPbPr to RGB using the formula:
  |R|   | 1  0      1.402 |   |Y |
  |G| = | 1 -0.344 -0.714 | x |Pb|
  |B|   | 1  1.722  0     |   |Pr|

And finally in the code below these RGB values are scaled to the range [0..255].

TODO NTSC(TMS99X8A) and PAL(TMS9929A) seem to have slightly different colors.
     Figure out where this difference comes from and how to emulate that best.
*/
const uint8_t Renderer::TMS99X8A_PALETTE[16][3] = {
	{   0,   0,   0 },
	{   0,   0,   0 },
	{  62, 184,  73 },
	{ 116, 208, 125 },
	{  89,  85, 224 },
	{ 128, 118, 241 },
	{ 185,  94,  81 },
	{ 101, 219, 239 },
	{ 219, 101,  89 },
	{ 255, 137, 125 },
	{ 204, 195,  94 },
	{ 222, 208, 135 },
	{  58, 162,  65 },
	{ 183, 102, 181 },
	{ 204, 204, 204 },
	{ 255, 255, 255 },
};

/*
TMS9X2X VDP family palette.

This pallette was reverse engineered by FRS:
1) Convert the YPbPr values from the datasheet to digital 8bit/channel DAC values in YCbCr
2) Use the official formula from the awarded BT.601 standard to convert the YCbCr to digital 24bit RGB

                          VDP built in palette                       Auxiliary tables                   Digital conversion
                                                         Digital 8bit/ch           Analog Component
                Analog levels           Digital 5bit/ch      ITU-R BT.601               Video
Color           Y       R-Y     B-Y     Y     R-Y   B-Y    Y     Cr     Cb        Y       Pr       Pb   ITU-R BT.601
Black           0.00    0.47    0.47     0    14    14    16    121    121      0.0    -7.65    -7.65     0     8     0
Medium green    0.53    0.07    0.20    16     2     6   133     31     61    135.2  -109.65   -76.50     0   241     1
Light green     0.67    0.17    0.27    20     5     8   162     53     76    170.9   -84.15   -58.65    50   251    65
Dark blue       0.40    0.40    1.00    12    12    30   104    106    240    102.0   -25.50   127.50    67    76   255
Light blue      0.53    0.43    0.93    16    13    28   133    113    225    135.2   -17.85   109.65   112   110   255
Dark red        0.47    0.83    0.30    14    25     9   118    203     83    119.9    84.15   -51.00   238    75    28
Cyan            0.73    0.00    0.70    22     0    21   177     16    173    186.2  -127.50    51.00     9   255   255
Medium red      0.53    0.93    0.27    16    28     8   133    225     76    135.2   109.65   -58.65   255    78    31
Light red       0.67    0.93    0.27    20    28     8   162    225     76    170.9   109.65   -58.65   255   112    65
Dark yellow     0.73    0.57    0.07    22    17     2   177    143     31    186.2    17.85  -109.65   211   213     0
Light yellow    0.80    0.57    0.17    24    17     5   191    143     53    204.0    17.85   -84.15   228   221    52
Dark green      0.47    0.13    0.23    14     4     7   118     46     68    119.9   -94.35   -68.85     0   209     0
Magenta         0.53    0.73    0.67    16    22    20   133    180    165    135.2    58.65    43.35   219    79   211
Gray            0.80    0.47    0.47    24    14    14   191    121    121    204.0    -7.65    -7.65   193   212   190
White           1.00    0.47    0.47    30    14    14   235    121    121    255.0    -7.65    -7.65   244   255   241


The first 3 columns are the voltage levels specified in the TMS datasheet.

Many subtleties contained in the datasheet had to be considered:

- Internal digital palette was omitted and had to be inferred
- Omitted DAC value 1,03 volts is reserved as headroom. It is shown in the
  LM1889 datasheet, recommended by Texas.

This means that:

- The BT.601 conversion must fit the TMS9918 headroom value inside the BT.601
  headroom range
- Proper analog conversions must saturate values of the headroom range. This
  was implicit when the datasheet mentioned that "1,00" was the max value.

So, of the above table, the next 3 columns is that value converted to digital
levels using this inferred table:

Step    5bit  Voltage Seen on
0.033   0     0.00    L&C
        1     0.03    Unused
        2     0.07    C
        3     0.10    Unused
        4     0.13    C
        5     0.17    C
        6     0.20    C
        7     0.23    C
        8     0.27    C
        9     0.30    C
        10    0.33    Unused
        11    0.37    Unused
        12    0.40    L&C
        13    0.43    C
        14    0.47    C
        15    0.50    Unused
        16    0.53    L
        17    0.57    C
        18    0.60    Unused
        19    0.63    Unused
        20    0.67    L&C
        21    0.70    C
        22    0.73    L&C
        23    0.77    Unused
        24    0.80    L
        25    0.83    C
        26    0.87    Unused
        27    0.90    Unused
        28    0.93    C
        29    0.97    Unused
        30    1.00    L&C       <-Artificial max level
        31    1.03    Unused    <-Headroom

The formulas to calculate the RGB values are these:

R=255/219(Y-16)+255/112(1-Kr)(Cr-128)
G=255/219(Y-16)-255/112(1-Kb)Kb/Kg(Cb-128)-255/112(1-Kr)Kr/Kg(Cr-128)
B=255/219(Y-16)+255/112(1-Kb)*(Cb-128)

The results must be clipped between 0 and 255.

Kr = 0,299
Kg = 0,587
Kb = 0,114



Wikipedia has a simplified version of the formula:
https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
*/

const uint8_t Renderer::TMS9X2XABT601_PALETTE[16][3] = {
	{   0,   0,   0 },
	{   0,   8,   0 },
	{   0, 241,   1 },
	{  50, 251,  65 },
	{  67,  76, 255 },
	{ 112, 110, 255 },
	{ 238,  75,  28 },
	{   9, 255, 255 },
	{ 255,  78,  31 },
	{ 255, 112,  65 },
	{ 211, 213,   0 },
	{ 228, 221,  52 },
	{   0, 209,   0 },
	{ 219,  79, 211 },
	{ 193, 212, 190 },
	{ 244, 255, 241 },
};

/*
 * Roughly measured RGB values in volts.
 * Voltages were in range of 1.12-5.04, and had 2 digits accuracy (it seems
 * minimum difference was 0.04 V).
 * Blue component of color 5 and red component of color 9 were higher than
 * the components for white... There are several methods to handle this...
 * 1) clip to values of white
 * 2) scale all colors by min/max of that component (means white is not 3x 255)
 * 3) scale per color if components for that color are beyond those of white
 * 4) assume the analog values are output by a DA converter, derive the digital
 *    values and scale that to the range 0-255 (thanks to FRS for this idea).
 *    This also results in white not being 3x 255, of course.
 *
 * Method 4 results in the table below and seems the most accurate (so far).
 *
 * Thanks to Tiago Valença and Carlos Mansur for measuring on a T7937A.
 */
const uint8_t Renderer::TOSHIBA_PALETTE[16][3] = {
	{   0,   0,   0 },
	{   0,   0,   0 },
	{ 102, 204, 102 },
	{ 136, 238, 136 },
	{  68,  68, 221 },
	{ 119, 119, 255 },
	{ 187,  85,  85 },
	{ 119, 221, 221 },
	{ 221, 102, 102 },
	{ 255, 119, 119 },
	{ 204, 204,  85 },
	{ 238, 238, 136 },
	{  85, 170,  85 },
	{ 187,  85, 187 },
	{ 204, 204, 204 },
	{ 238, 238, 238 },
};

/*
How come the FM-X has a distinct palette while it clearly has a TMS9928 VDP?
Because it has an additional circuit that rework the palette for the same one
used in the Fujitsu FM-7. It's encoded in 3-bit RGB.

This seems to be the 24-bit RGB equivalent to the palette output by the FM-X on its RGB conector:
*/
const uint8_t Renderer::FUJITSUFMX_PALETTE[16][3] = {
	{   0,   0,   0 },
	{   0,   0,   0 },
	{   0, 255,   0 },
	{   0, 255,   0 },
	{   0,   0, 255 },
	{   0,   0, 255 },
	{ 255,   0,   0 },
	{   0, 255, 255 },
	{ 255,   0,   0 },
	{ 255,   0,   0 },
	{ 255, 255,   0 },
	{ 255, 255,   0 },
	{   0, 255,   0 },
	{ 255,   0, 255 },
	{ 255, 255, 255 },
	{ 255, 255, 255 },
};

/*
Sprite palette in Graphic 7 mode.
See page 98 of the V9938 data book.
*/
const uint16_t Renderer::GRAPHIC7_SPRITE_PALETTE[16] = {
	0x000, 0x002, 0x030, 0x032, 0x300, 0x302, 0x330, 0x332,
	0x472, 0x007, 0x070, 0x077, 0x700, 0x707, 0x770, 0x777
};

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
}

} // namespace openmsx
