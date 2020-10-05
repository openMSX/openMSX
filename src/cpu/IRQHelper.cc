#include "IRQHelper.hh"
#include "DeviceConfig.hh"
#include "MSXCPU.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include <memory>

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


// IRQ sinks (used to implement OptionalIRQ)

class NotConnectedIRQSink : public IRQSink
{
public:
	void raise() override {}
	void lower() override {}
};

class MaskableIRQSink : public IRQSink
{
public:
	MaskableIRQSink(MSXCPU& cpu_) : cpu(cpu_) {}
	void raise() override { cpu.raiseIRQ(); }
	void lower() override { cpu.lowerIRQ(); }
private:
	MSXCPU& cpu;
};

class NonMaskableIRQSink : public IRQSink
{
public:
	NonMaskableIRQSink(MSXCPU& cpu_) : cpu(cpu_) {}
	void raise() override { cpu.raiseNMI(); }
	void lower() override { cpu.lowerNMI(); }
private:
	MSXCPU& cpu;
};


// class OptionalIRQ

OptionalIRQ::OptionalIRQ(MSXCPU& cpu, const DeviceConfig& config)
{
	auto connected = config.getChildData("irq_connected", "irq");
	if (connected == one_of("irq", "true")) {
		sink = std::make_unique<MaskableIRQSink>(cpu);
	} else if (connected == "nmi") {
		sink = std::make_unique<NonMaskableIRQSink>(cpu);
	} else if (connected == "false") {
		sink = std::make_unique<NotConnectedIRQSink>();
	} else {
		throw MSXException(
			"Unknown IRQ sink \"", connected, "\" in <irq_connected>");
	}
}

} // namespace openmsx
