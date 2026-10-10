// MHT Gunstick light gun, see LightGun.cc.

#include "Gunstick.hh"

#include "serialize.hh"
#include "serialize_meta.hh"

namespace openmsx {

Gunstick::Gunstick(MSXMotherBoard& motherBoard_,
                   MSXEventDistributor& eventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   Display& display_)
	: LightGun(motherBoard_, eventDistributor_, stateChangeDistributor_,
	           display_, /*id*/ 0, {.light = RD_PIN2, .trigger = RD_PIN6, .lightHigh = false})
{
}

// Pluggable
zstring_view Gunstick::getName() const
{
	return "gunstick";
}

zstring_view Gunstick::getDescription() const
{
	return "MHT Gunstick light gun, aimed with the mouse";
}

template<typename Archive>
void Gunstick::serialize(Archive& ar, unsigned version)
{
	LightGun::serialize(ar, version);
}
INSTANTIATE_SERIALIZE_METHODS(Gunstick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Gunstick, "Gunstick");

} // namespace openmsx
