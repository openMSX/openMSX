// $Id$

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
const byte Renderer::TMS99X8A_PALETTE[16][3] = {
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
Sprite palette in Graphic 7 mode.
See page 98 of the V9938 data book.
*/
const word Renderer::GRAPHIC7_SPRITE_PALETTE[16] = {
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
