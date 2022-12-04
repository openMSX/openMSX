#include "MSXPPI.hh"
#include "LedStatus.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "CassettePort.hh"
#include "RenShaTurbo.hh"
#include "GlobalSettings.hh"
#include "serialize.hh"

namespace openmsx {

MSXPPI::MSXPPI(const DeviceConfig& config)
	: MSXDevice(config)
	, cassettePort(getMotherBoard().getCassettePort())
	, renshaTurbo(getMotherBoard().getRenShaTurbo())
	, i8255(*this, getCurrentTime(), config.getGlobalSettings().getInvalidPpiModeSetting())
	, click(config)
	, keyboard(
		config.getMotherBoard(),
		config.getMotherBoard().getScheduler(),
		config.getMotherBoard().getCommandController(),
		config.getMotherBoard().getReactor().getEventDistributor(),
		config.getMotherBoard().getMSXEventDistributor(),
		config.getMotherBoard().getStateChangeDistributor(),
		Keyboard::MATRIX_MSX, config)
{
	reset(getCurrentTime());
}

MSXPPI::~MSXPPI()
{
	powerDown(EmuTime::dummy());
}

void MSXPPI::reset(EmuTime::param time)
{
	i8255.reset(time);
	click.reset(time);
}

void MSXPPI::powerDown(EmuTime::param /*time*/)
{
	getLedStatus().setLed(LedStatus::CAPS, false);
}

byte MSXPPI::readIO(word port, EmuTime::param time)
{
	return i8255.read(port & 0x03, time);
}

byte MSXPPI::peekIO(word port, EmuTime::param time) const
{
	return i8255.peek(port & 0x03, time);
}

void MSXPPI::writeIO(word port, byte value, EmuTime::param time)
{
	i8255.write(port & 0x03, value, time);
}


// I8255Interface

byte MSXPPI::readA(EmuTime::param time)
{
	return peekA(time);
}
byte MSXPPI::peekA(EmuTime::param /*time*/) const
{
	// port A is normally an output on MSX, reading from an output port
	// is handled internally in the 8255
	// TODO check this on a real MSX
	// TODO returning 0 fixes the 'get_selected_slot' script right after
	//      reset (when PPI directions are not yet set). For now this
	//      solution is good enough.
	return 0;
}
void MSXPPI::writeA(byte value, EmuTime::param /*time*/)
{
	getCPUInterface().setPrimarySlots(value);
}

byte MSXPPI::readB(EmuTime::param time)
{
	return peekB(time);
}
byte MSXPPI::peekB(EmuTime::param time) const
{
	auto& keyb = const_cast<Keyboard&>(keyboard);
	if (selectedRow != 8) {
		return keyb.getKeys()[selectedRow];
	} else {
		return keyb.getKeys()[8] | (renshaTurbo.getSignal(time) ? 1:0);
	}
}
void MSXPPI::writeB(byte /*value*/, EmuTime::param /*time*/)
{
	// probably nothing happens on a real MSX
}

nibble MSXPPI::readC1(EmuTime::param time)
{
	return peekC1(time);
}
nibble MSXPPI::peekC1(EmuTime::param /*time*/) const
{
	return 15; // TODO check this
}
nibble MSXPPI::readC0(EmuTime::param time)
{
	return peekC0(time);
}
nibble MSXPPI::peekC0(EmuTime::param /*time*/) const
{
	return 15; // TODO check this
}
void MSXPPI::writeC1(nibble value, EmuTime::param time)
{
	if ((prevBits ^ value) & 1) {
		cassettePort.setMotor((value & 1) == 0, time); // 0=0n, 1=Off
	}
	if ((prevBits ^ value) & 2) {
		cassettePort.cassetteOut((value & 2) != 0, time);
	}
	if ((prevBits ^ value) & 4) {
		getLedStatus().setLed(LedStatus::CAPS, (value & 4) == 0);
	}
	if ((prevBits ^ value) & 8) {
		click.setClick((value & 8) != 0, time);
	}
	prevBits = value;
}
void MSXPPI::writeC0(nibble value, EmuTime::param /*time*/)
{
	selectedRow = value;
}


template<typename Archive>
void MSXPPI::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("i8255", i8255);

	// merge prevBits and selectedRow into one byte
	auto portC = byte((prevBits << 4) | (selectedRow << 0));
	ar.serialize("portC", portC);
	if constexpr (Archive::IS_LOADER) {
		selectedRow = (portC >> 0) & 0xF;
		nibble bits = (portC >> 4) & 0xF;
		writeC1(bits, getCurrentTime());
	}
	ar.serialize("keyboard", keyboard);
}
INSTANTIATE_SERIALIZE_METHODS(MSXPPI);
REGISTER_MSXDEVICE(MSXPPI, "PPI");

} // namespace openmsx
