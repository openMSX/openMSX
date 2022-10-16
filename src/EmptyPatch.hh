#ifndef EMPTYPATCH_HH
#define EMPTYPATCH_HH

#include "PatchInterface.hh"
#include <span>

namespace openmsx {

class EmptyPatch final : public PatchInterface
{
public:
	EmptyPatch(std::span<const byte> block_)
		: block(block_) {}

	void copyBlock(size_t src, byte* dst, size_t num) const override;
	[[nodiscard]] size_t getSize() const override { return block.size(); }
	[[nodiscard]] std::vector<Filename> getFilenames() const override { return {}; }
	[[nodiscard]] bool isEmptyPatch() const override { return true; }

private:
	std::span<const byte> block;
};

} // namespace openmsx

#endif
