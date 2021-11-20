#include "MSXTurboRPCM.hh"
#include "MSXMotherBoard.hh"
#include "MSXMixer.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

MSXTurboRPCM::MSXTurboRPCM(const DeviceConfig& config)
	: MSXDevice(config)
	, mixer(getMotherBoard().getMSXMixer())
	, connector(getPluggingController(), "pcminput")
	, dac("PCM", "Turbo-R PCM", config)
	, reference(getCurrentTime())
	, hwMute(false)
{
	reset(getCurrentTime());
}

MSXTurboRPCM::~MSXTurboRPCM()
{
	hardwareMute(false);
}

void MSXTurboRPCM::reset(EmuTime::param time)
{
	reference.reset(time);
	status = 0;
	DValue = 0x80; // TODO correct initial value?
	hold = 0x80; // avoid UMR
	dac.reset(time);
	hardwareMute(false);
}

byte MSXTurboRPCM::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MSXTurboRPCM::peekIO(word port, EmuTime::param time) const
{
	switch (port & 0x01) {
	case 0:
		// bit 0-1  15.75kHz counter
		// bit 2-7  not used
		return reference.getTicksTill(time) & 0x03;
	case 1:
		// bit 0   BUFF  0->D/A    TODO check this bit
		//               1->A/D
		// bit 1   MUTE  mute ALL sound  0->muted
		// bit 2   FILT  filter  0->standard signal
		//                       1->filtered signal
		// bit 3   SEL   select 0->D/A
		//                      1->Mic/Jack
		// bit 4   SMPL  sample/hold  0->sample
		//                            1->hold
		// bit 5-6       not used
		// bit 7   COMP  comparator result 0->greater
		//                                 1->smaller
		return (getComp(time) ? 0x80 : 0x00) | (status & 0x1F);
	default: // unreachable, avoid warning
		UNREACHABLE; return 0;
	}
}

void MSXTurboRPCM::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0x01) {
	case 0:
		// While playing: sample value
		//       recording: compare value
		// Resets counter
		reference.advance(time);
		DValue = value;
		if (status & 0x02) {
			dac.writeDAC(DValue, time);
		}
		break;

	case 1:
		// bit 0   BUFF
		// bit 1   MUTE  mute _all_ sound  0->no sound
		// bit 2   FILT  filter  1->filter on
		// bit 3   SEL   select  1->Mic/Jack  0->D/A
		// bit 4   SMPL  sample/hold  1->hold
		// bit 5-7  not used
		byte change = status ^ value;
		status = value;

		if ((change & 0x01) && ((status & 0x01) == 0)) {
			dac.writeDAC(DValue, time);
		}
		// TODO status & 0x08
		if ((change & 0x10) && (status & 0x10)) {
			hold = getSample(time);
		}
		hardwareMute(!(status & 0x02));
		break;
	}
}

byte MSXTurboRPCM::getSample(EmuTime::param time) const
{
	return (status & 0x04)
		? (connector.readSample(time) / 256) + 0x80
		: 0x80; // TODO check
}

bool MSXTurboRPCM::getComp(EmuTime::param time) const
{
	// TODO also when D/A ??
	byte sample = (status & 0x10) ? hold : getSample(time);
	return sample >= DValue;
}

void MSXTurboRPCM::hardwareMute(bool mute)
{
	if (mute == hwMute) return;

	hwMute = mute;
	if (hwMute) {
		mixer.mute();
	} else {
		mixer.unmute();
	}
}


template<typename Archive>
void MSXTurboRPCM::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("audioConnector", connector,
	             "reference",      reference,
	             "status",         status,
	             "DValue",         DValue,
	             "hold",           hold,
	             "DAC",            dac);

	hardwareMute(!(status & 0x02));  // restore hwMute
}
INSTANTIATE_SERIALIZE_METHODS(MSXTurboRPCM);
REGISTER_MSXDEVICE(MSXTurboRPCM, "TurboR-PCM");

} // namespace openmsx
