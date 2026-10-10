// ASCII Plus-X Terminator Laser light gun, see LightGun.cc.
//
// E.g. Dungeon Hunter blanks the screen, then shows one target per frame as
// a white block and polls the sensor during that frame: any read with light
// hits that target.

#include "TerminatorLaser.hh"

#include "serialize.hh"
#include "serialize_meta.hh"

namespace openmsx {

TerminatorLaser::TerminatorLaser(MSXMotherBoard& motherBoard_,
                                 MSXEventDistributor& eventDistributor_,
                                 StateChangeDistributor& stateChangeDistributor_,
                                 Display& display_)
	: LightGun(motherBoard_, eventDistributor_, stateChangeDistributor_,
	           display_, /*id*/ 1, {.light = RD_PIN6, .trigger = RD_PIN7, .lightHigh = true})
{
}

// Pluggable
zstring_view TerminatorLaser::getName() const
{
	return "terminatorlaser";
}

zstring_view TerminatorLaser::getDescription() const
{
	return "ASCII Plus-X Terminator Laser light gun, aimed with the mouse";
}

template<typename Archive>
void TerminatorLaser::serialize(Archive& ar, unsigned version)
{
	LightGun::serialize(ar, version);
}
INSTANTIATE_SERIALIZE_METHODS(TerminatorLaser);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, TerminatorLaser, "TerminatorLaser");

} // namespace openmsx
