
#include "E6Timer.hh"
#include <assert.h>


E6Timer::E6Timer()
{
};

E6Timer::~E6Timer()
{
};

MSXDevice* E6Timer::instantiate()
{
	return new E6Timer();
};
 
void E6Timer::init()
{
	MSXMotherBoard::instance()->register_IO_In (0xE6,this);
	MSXMotherBoard::instance()->register_IO_In (0xE7,this);
	MSXMotherBoard::instance()->register_IO_Out(0xE6,this);
};

void E6Timer::reset()
{
	reference = Emutime(255681, 0);	// 1/14 * 3.58MHz
};

byte E6Timer::readIO(byte port, Emutime &time)
{
	int counter = reference.getTicksTill(time);
	switch (port) {
	case 0xE6:
		return counter & 0xff;
	case 0xE7:
		return (counter >> 8) & 0xff;
	default:
		assert (false);
	}
};

void E6Timer::writeIO(byte port, byte value, Emutime &time)
{
	assert (port == 0xE6);
	reference = time;	// freq does not change
};

