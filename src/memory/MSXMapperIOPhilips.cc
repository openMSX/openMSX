// $Id$

#include <cassert>
#include "MSXMapperIOPhilips.hh"

namespace openmsx {

// unused bits read always "1"
byte MSXMapperIOPhilips::calcMask(const std::multiset<unsigned>& mapperSizes)
{
	unsigned largest = (mapperSizes.empty()) ? 1
	                                         : *mapperSizes.rbegin();
	return (256 - (1 << log2RoundedUp(largest))) & 255;
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
byte MSXMapperIOPhilips::log2RoundedUp(unsigned num)
{
	assert((1 <= num) && (num <= 256));
	unsigned foo = 128; byte res = 8;
	while (num <= foo) {
		foo /= 2; res--;
	} 
	return res;
}

} // namespace openmsx

