// $Id$

#include "IRQHelper.hh"
#include "MSXCPU.hh"
#include "serialize.hh"

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


template<typename SOURCE>
template<typename Archive>
void IntHelper<SOURCE>::serialize(Archive& ar, unsigned /*version*/)
{
	bool pending = request;
	ar.serialize("pending", pending);
	if (ar.isLoader()) {
		if (pending) {
			set();
		} else {
			reset();
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(IRQHelper);

} // namespace openmsx
