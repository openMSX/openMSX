// $Id$

#include "emutime.hh"

std::ostream &operator<<(std::ostream &os, const Emutime &et)
{
	os << et._emutime << " " << et._scale;
	return os;
}

#if 0
int main (int argc, char** argv)
{
	assert (sizeof(uint64)==8);

	Emutime e1 (3579545);
	Emutime e2 (3579545*2, 2);

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
