// $Id$

#include "IRQHelper.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "serialize.hh"

namespace openmsx {

// class IRQSource

IRQSource::IRQSource(MSXCPU& cpu_)
	: cpu(cpu_)
{
}

void IRQSource::raise()
{
	cpu.raiseIRQ();
}

void IRQSource::lower()
{
	cpu.lowerIRQ();
}


// class NMISource

NMISource::NMISource(MSXCPU& cpu_)
	: cpu(cpu_)
{
}

void NMISource::raise()
{
	cpu.raiseNMI();
}

void NMISource::lower()
{
	cpu.lowerNMI();
}


// class DynamicSource

DynamicSource::DynamicSource(MSXCPU& cpu_)
	: cpu(cpu_), type(IRQ)
{
}

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


// class IntHelper

template<typename SOURCE>
IntHelper<SOURCE>::IntHelper(MSXMotherBoard& motherboard, const std::string& name)
	: SOURCE(motherboard.getCPU())
	, request(motherboard.getDebugger(), name, "Outgoing IRQ signal.", false)
{
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

// Force template instantiation
template class IntHelper<IRQSource>;
//template class IntHelper<NMISource>;
//template class IntHelper<DynamicSource>;

} // namespace openmsx
