#include "SonyHBIF900.hh"

#include "ExternalAudio.hh"
#include "BooleanSetting.hh"
#include "MSXMotherBoard.hh"
#include "MSXCommandController.hh"
#include "serialize.hh"

namespace openmsx {

SonyHBIF900::SonyHBIF900(const DeviceConfig& config)
	: MSXDevice(config)
{
	reset(EmuTime::dummy());
	// The separate AV Creator routes its audio under CALL AUDIO control.
	// ExternalAudio keeps this optional for generic capture extensions.
	if (auto* setting = dynamic_cast<BooleanSetting*>(
	        getMotherBoard().getMSXCommandController().findSetting(
	            "external_audio_follow_set_video"))) {
		setting->setBoolean(true);
	}
}

void SonyHBIF900::reset(EmuTime time)
{
	portF7 = 0xFF;
	applyAudioRouting(time);
}

byte SonyHBIF900::readIO(uint16_t port, EmuTime time)
{
	return peekIO(port, time);
}

byte SonyHBIF900::peekIO(uint16_t port, EmuTime /*time*/) const
{
	// The Video Utility ROM does "IN A,(F7) / AND 80h" at boot. A low
	// detection bit tells it that the AV Creator is connected.
	return (port & 0xFF) == 0xF7 ? byte(portF7 & 0x7F) : 0xFF;
}

void SonyHBIF900::writeIO(uint16_t port, byte value, EmuTime time)
{
	if ((port & 0xFF) != 0xF7) return;
	portF7 = value;
	applyAudioRouting(time);
}

void SonyHBIF900::applyAudioRouting(EmuTime time) const
{
	if (auto* audio = dynamic_cast<MSXExternalAudio*>(
	        getMotherBoard().findDevice("External audio"))) {
		// Sony's CALL AUDIO writes the inverted two-bit channel mask to F7.
		audio->setRouting((portF7 & 0x02) == 0, (portF7 & 0x01) == 0, time);
	}
}

template<typename Archive>
void SonyHBIF900::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("portF7", portF7);
	if constexpr (Archive::IS_LOADER) applyAudioRouting(getCurrentTime());
}
INSTANTIATE_SERIALIZE_METHODS(SonyHBIF900);
REGISTER_MSXDEVICE(SonyHBIF900, "SonyHBIF900");

} // namespace openmsx
