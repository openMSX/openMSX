#include "MSXMatsushita.hh"
#include "MSXCPU.hh"
#include "VDP.hh"
#include "MSXCPUInterface.hh"
#include "CliComm.hh"
#include "MSXException.hh"
#include "serialize.hh"

namespace openmsx {

static const byte ID = 0x08;

MSXMatsushita::MSXMatsushita(const DeviceConfig& config)
	: MSXDevice(config)
	, MSXSwitchedDevice(getMotherBoard(), ID)
	, cpu(getCPU()) // used frequently, so cache it
	, vdp(nullptr)
	, lastTime(EmuTime::zero)
	, firmwareSwitch(config)
	, sram(config.findChild("sramname") ? make_unique<SRAM>(getName() + " SRAM", 0x800, config) : nullptr)
	, turboAvailable(config.getChildDataAsBool("hasturbo", false))
	, turboEnabled(false)
{
	// TODO find out what ports 0x41 0x45 0x46 are used for

	reset(EmuTime::dummy());
}

void MSXMatsushita::init()
{
	MSXDevice::init();

	const auto& refs = getReferences();
	vdp = !refs.empty() ? dynamic_cast<VDP*>(refs[0]) : nullptr;
	if (!vdp) {
		// No (valid) reference to the VDP found. We do allow this to
		// model MSX machines where the Matsushita device does not
		// influence the VDP timing. It's not know whether such
		// machines actually exist.
		return;
	}

	// Wrap the VDP ports.
	auto& cpuInterface = getCPUInterface();
	for (int i = 0; i < 2; ++i) {
		MSXDevice* in  = cpuInterface.wrap_IO_In (0x98 + i, this);
		if (in != vdp) {
			throw MSXException(
				"Invalid Matsushita configuration: "
				"VDP not on input ports 0x98-0x99.");
		}
	}
	for (int i = 0; i < 4; ++i) {
		MSXDevice* out = cpuInterface.wrap_IO_Out(0x98 + i, this);
		if (out != vdp) {
			throw MSXException(
				"Invalid Matsushita configuration: "
				"VDP not on output ports 0x98-0x9B.");
		}
	}
}

MSXMatsushita::~MSXMatsushita()
{
	if (!vdp) return;

	// Unwrap the VDP ports.
	auto& cpuInterface = getCPUInterface();
	for (int i = 0; i < 2; ++i) {
		cpuInterface.unwrap_IO_In (0x98 + i, vdp);
	}
	for (int i = 0; i < 4; ++i) {
		cpuInterface.unwrap_IO_Out(0x98 + i, vdp);
	}
}

void MSXMatsushita::reset(EmuTime::param /*time*/)
{
	color1 = color2 = pattern = address = 0; // TODO check this
}

byte MSXMatsushita::readSwitchedIO(word port, EmuTime::param time)
{
	// TODO: Port 7 and 8 can be read as well.
	byte result = peekSwitchedIO(port, time);
	switch (port & 0x0F) {
	case 3:
		pattern = (pattern << 2) | (pattern >> 6);
		break;
	case 9:
		address = (address + 1) & 0x1FFF;
		break;
	}
	return result;
}

byte MSXMatsushita::peekSwitchedIO(word port, EmuTime::param /*time*/) const
{
	byte result;
	switch (port & 0x0F) {
	case 0:
		result = byte(~ID);
		break;
	case 1:
		result = firmwareSwitch.getStatus() ? 0x7F : 0xFF;
		// bit 0: turbo status, 0=on
		if (turboEnabled) {
			result &= ~0x01;
		}
		// bit 2: 0 = turbo feature available
		if (turboAvailable) {
			result &= ~0x04;
		}
		break;
	case 3:
		result = (((pattern & 0x80) ? color2 : color1) << 4)
		        | ((pattern & 0x40) ? color2 : color1);
		break;
	case 9:
		if (address < 0x800 && sram) {
			result = (*sram)[address];
		} else {
			result = 0xFF;
		}
		break;
	default:
		result = 0xFF;
	}
	return result;
}

void MSXMatsushita::writeSwitchedIO(word port, byte value, EmuTime::param /*time*/)
{
	switch (port & 0x0F) {
	case 1:
		// the turboEnabled flag works even though no turbo is available
		if (value & 1) {
			// bit0 = 1 -> 3.5MHz
			if (turboAvailable) {
				getCPU().setZ80Freq(3579545);
			}
			turboEnabled = false;
		} else {
			// bit0 = 0 -> 5.3MHz
			if (turboAvailable) {
				getCPU().setZ80Freq(5369318); // 3579545 * 3/2
			}
			turboEnabled = true;
		}
		break;
	case 3:
		color2 = (value & 0xF0) >> 4;
		color1 =  value & 0x0F;
		break;
	case 4:
		pattern = value;
		break;
	case 7:
		// set address (low)
		address = (address & 0xFF00) | value;
		break;
	case 8:
		// set address (high)
		address = (address & 0x00FF) | ((value & 0x1F) << 8);
		break;
	case 9:
		// write sram
		if (address < 0x800 && sram) {
			sram->write(address, value);
		}
		address = (address + 1) & 0x1FFF;
		break;
	}
}

byte MSXMatsushita::readIO(word port, EmuTime::param time)
{
	// TODO also delay on read?
	assert(vdp);
	return vdp->readIO(port, time);
}

byte MSXMatsushita::peekIO(word port, EmuTime::param time) const
{
	assert(vdp);
	return vdp->peekIO(port, time);
}

void MSXMatsushita::writeIO(word port, byte value, EmuTime::param time)
{
	assert(vdp);
	delay(time);
	vdp->writeIO(port, value, lastTime.getTime());
}

void MSXMatsushita::delay(EmuTime::param time)
{
	if (turboAvailable && turboEnabled) {
		lastTime += 46; // 8us, like in S1990
		if (time < lastTime.getTime()) {
			cpu.wait(lastTime.getTime());
			return;
		}
	}
	lastTime.advance(time);
}

template<typename Archive>
void MSXMatsushita::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	// no need to serialize MSXSwitchedDevice base class

	if (sram) ar.serialize("SRAM", *sram);
	ar.serialize("address", address);
	ar.serialize("color1", color1);
	ar.serialize("color2", color2);
	ar.serialize("pattern", pattern);

	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("lastTime", lastTime);
		ar.serialize("turboEnabled", turboEnabled);
	} else {
		// keep 'lastTime == zero'
		// keep 'turboEnabled == false'
		getCliComm().printWarning(
			"Loading an old savestate: the timing of the CPU-VDP "
			"communication emulation has changed. This may cause "
			"synchronization problems in replay.");
	}
}

INSTANTIATE_SERIALIZE_METHODS(MSXMatsushita);
REGISTER_MSXDEVICE(MSXMatsushita, "Matsushita");

} // namespace openmsx
