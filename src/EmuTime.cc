// $Id$

#include "EmuTime.hh"

namespace openmsx {

// Do not use INFINITY because it is macro-expanded on some systems
static const uint64 INFTY = 18446744073709551615ULL; //ULLONG_MAX;

const EmuDuration EmuDuration::zero((uint64)0);
const EmuDuration EmuDuration::infinity(INFTY);
const EmuTime EmuTime::zero((uint64)0);
const EmuTime EmuTime::infinity(INFTY);


ostream &operator<<(ostream &os, const EmuTime &et)
{
	os << et.time;
	return os;
}

} // namespace openmsx

#if 0
using openmsx::EmuTime;
int main (int argc, char** argv)
{
	assert (sizeof(uint64)==8);

	EmuTime e1 (3579545);
	EmuTime e2 (3579545*2, 2);

	assert (e1 <  e2);
	assert (e1 <= e2);
	e1+=1;
	assert (e1 == e2);
	assert (e1 <= e2);
	assert (e1 >= e2);
	e1+=e2;
	assert (e1 >  e2);
	assert (e1 >= e2);
}
#endif
