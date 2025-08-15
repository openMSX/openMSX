#ifndef PATCHINTERFACE_HH
#define PATCHINTERFACE_HH

#include "Filename.hh"

#include <cstdint>
#include <span>
#include <vector>

namespace openmsx {

class PatchInterface
{
public:
	virtual ~PatchInterface() = default;

	virtual void copyBlock(size_t src, std::span<uint8_t> dst) const = 0;
	[[nodiscard]] virtual size_t getSize() const = 0;
	[[nodiscard]] virtual std::vector<Filename> getFilenames() const = 0;
	[[nodiscard]] virtual bool isEmptyPatch() const { return false; }
};

} // namespace openmsx

#endif
