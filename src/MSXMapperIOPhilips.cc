// $Id$

#include <cassert>
#include "MSXMapperIOPhilips.hh"

MSXMapperIOPhilips::MSXMapperIOPhilips(MSXConfig::Device *config, const EmuTime &time)
	: MSXMapperIO(config, time)
{
	largest = 0;
	mask = 255;
}

// unused bits read always "1"
byte MSXMapperIOPhilips::convert(byte value)
{
	return value|mask;
}

void MSXMapperIOPhilips::registerMapper(int blocks)
{
	if (blocks > largest) {
		largest = blocks;
		int bits = log2RoundedUp(blocks);
		mask = (256-(1<<bits))&255;
	}
}

/* 
 * Returns the 2-logarithm of "num" rounded up, this is the number of bits
 * needed to represent nummers smaller than "num"
 *         num  =   1  ==> 0
 *         num  =   2  ==> 1
 *    3 <= num <=   4  ==> 2 
 *    5 <= num <=   8  ==> 3 
 *    9 <= num <=  16  ==> 4 
 *   17 <= num <=  32  ==> 5 
 *   33 <= num <=  64  ==> 6 
 *   65 <= num <= 128  ==> 7 
 *  129 <= num <= 256  ==> 8
 */
int MSXMapperIOPhilips::log2RoundedUp(int num)
{
	assert((1<=num)&&(num<=256));
	int foo = 128; int res = 8;
	while (num <= foo) {
		foo /= 2; res--;
	} 
	return res;
}

