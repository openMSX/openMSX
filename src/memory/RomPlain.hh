#ifndef ROMPLAIN_HH
#define ROMPLAIN_HH

#include "RomBlocks.hh"
#include "RomTypes.hh"

namespace openmsx {

class RomPlain final : public Rom8kBBlocks
{
public:
	RomPlain(const DeviceConfig& config, Rom&& rom, RomType type);
	[[nodiscard]] unsigned getBaseSizeAlignment() const override;

private:
	void guessHelper(unsigned offset, int* pages);
	[[nodiscard]] unsigned guessLocation(unsigned windowBase, unsigned windowSize);
};

} // namespace openmsx

#endif
