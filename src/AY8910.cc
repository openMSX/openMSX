
#include "AY8910.hh"
#include <assert.h>


AY8910::AY8910(AY8910Interface &interf) : interface(interf)
{
	//TODO register as sound generator
	reset();
}

AY8910::~AY8910()
{
}

void AY8910::reset()
{
	for (int i=0; i<=15; i++) {
		writeRegister(i, 0);
	}
}

byte AY8910::readRegister(int reg)
{
	assert ((0<=reg)&&(reg<=15));
	switch (reg) {
	case AY_PORTA:
		if (!(regs[AY_ENABLE] & PORT_A_DIRECTION))
			//input
			regs[reg] = interface.readA();
		break;
	case AY_PORTB:
		if (!(regs[AY_ENABLE] & PORT_B_DIRECTION))
			//input
			regs[reg] = interface.readB();
		break;
	}
	return regs[reg];
}

void AY8910::writeRegister(int reg, byte value)
{
	assert ((0<=reg)&&(reg<=15));
	regs[reg] = value;
	switch (reg) {
	//TODO
	case AY_PORTA:
		if (regs[AY_ENABLE] & PORT_A_DIRECTION)
			//output
			interface.writeA(value);
		break;
	case AY_PORTB:
		if (regs[AY_ENABLE] & PORT_B_DIRECTION)
			//output
			interface.writeB(value);
		break;
	}
}

//SoundDevice
void AY8910::setVolume(int newVolume)
{
	//TODO
}
