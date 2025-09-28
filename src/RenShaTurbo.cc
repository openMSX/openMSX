#include "RenShaTurbo.hh"

#include "MSXException.hh"
#include "XMLElement.hh"

namespace openmsx {

RenShaTurbo::RenShaTurbo(MSXMotherBoard& motherBoard,
                         const XMLElement& machineConfig)
{
	if (const auto* config = machineConfig.findChild("RenShaTurbo")) {
		int min_ints = config->getChildDataAsInt("min_ints", 47);
		int max_ints = config->getChildDataAsInt("max_ints", 221);
		if ((min_ints < 1) || (min_ints > max_ints) || (max_ints > 6000)) {
			throw MSXException(
				"Error in RenShaTurbo speed settings: "
				"1 <= min_ints <= max_ints <= 6000.");
		}
		autofire.emplace(motherBoard, min_ints, max_ints,
		                 Autofire::ID::RENSHATURBO);
	}
}

bool RenShaTurbo::getSignal(EmuTime time)
{
	return autofire ? autofire->getSignal(time) : false;
}

template<typename Archive>
void RenShaTurbo::serialize(Archive& ar, unsigned /*version*/)
{
	if (autofire) ar.serialize("autofire", *autofire);
}
INSTANTIATE_SERIALIZE_METHODS(RenShaTurbo)

} // namespace openmsx
