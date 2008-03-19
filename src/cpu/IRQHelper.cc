// $Id$

#include "IRQHelper.hh"
#include "MSXCPU.hh"

namespace openmsx {

// class IRQSource

void IRQSource::raise()
{
	cpu.raiseIRQ();
}

void IRQSource::lower()
{
	cpu.lowerIRQ();
}


// class NMISource

void NMISource::raise()
{
	cpu.raiseNMI();
}

void NMISource::lower()
{
	cpu.lowerNMI();
}


// class DynamicSource

void DynamicSource::raise()
{
	if (type == IRQ) {
		cpu.raiseIRQ();
	} else {
		cpu.raiseNMI();
	}
}

void DynamicSource::lower()
{
	if (type == NMI) {
		cpu.lowerIRQ();
	} else {
		cpu.lowerNMI();
	}
}

} // namespace openmsx
