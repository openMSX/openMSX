#include "IRQHelper.hh"
#include "MSXCPU.hh"
#include "DeviceConfig.hh"

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


// class OptionalIRQ

OptionalIRQ::OptionalIRQ(MSXCPU& cpu_, const DeviceConfig& config)
	: cpu(config.getChildDataAsBool("irq_connected", true) ? &cpu_ : nullptr)
{
}

void OptionalIRQ::raise()
{
	if (cpu) cpu->raiseIRQ();
}

void OptionalIRQ::lower()
{
	if (cpu) cpu->lowerIRQ();
}

} // namespace openmsx
