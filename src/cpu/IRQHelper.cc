#include "IRQHelper.hh"
#include "DeviceConfig.hh"
#include "MSXCPU.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include "unreachable.hh"

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
	: cpu(cpu_)
	, type([&] {
		auto connected = config.getChildData("irq_connected", "irq");
		if (connected == one_of("irq", "true")) {
			return Maskable;
		} else if (connected == "nmi") {
			return NonMaskable;
		} else if (connected == "false") {
			return NotConnected;
		} else {
			throw MSXException(
				"Unknown IRQ sink \"", connected, "\" in <irq_connected>");
		}
	}())
{
}

void OptionalIRQ::raise()
{
	switch (type) {
		case NotConnected:
			// nothing
			break;
		case Maskable:
			cpu.raiseIRQ();
			break;
		case NonMaskable:
			cpu.raiseNMI();
			break;
		default:
			UNREACHABLE;
	}
}

void OptionalIRQ::lower()
{
	switch (type) {
		case NotConnected:
			// nothing
			break;
		case Maskable:
			cpu.lowerIRQ();
			break;
		case NonMaskable:
			cpu.lowerNMI();
			break;
		default:
			UNREACHABLE;
	}
}

} // namespace openmsx
