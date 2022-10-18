#ifndef EMPTYPATCH_HH
#define EMPTYPATCH_HH

#include "PatchInterface.hh"

namespace openmsx {

class EmptyPatch final : public PatchInterface
{
public:
	EmptyPatch(std::span<const uint8_t> block_)
		: block(block_) {}

	void copyBlock(size_t src, std::span<uint8_t> dst) const override;
	[[nodiscard]] size_t getSize() const override { return block.size(); }
	[[nodiscard]] std::vector<Filename> getFilenames() const override { return {}; }
	[[nodiscard]] bool isEmptyPatch() const override { return true; }

private:
	std::span<const uint8_t> block;
};

} // namespace openmsx

#endif
