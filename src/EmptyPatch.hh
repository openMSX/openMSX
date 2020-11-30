#ifndef EMPTYPATCH_HH
#define EMPTYPATCH_HH

#include "PatchInterface.hh"

namespace openmsx {

class EmptyPatch final : public PatchInterface
{
public:
	EmptyPatch(const byte* block, size_t size);

	void copyBlock(size_t src, byte* dst, size_t num) const override;
	[[nodiscard]] size_t getSize() const override;
	[[nodiscard]] std::vector<Filename> getFilenames() const override;
	[[nodiscard]] bool isEmptyPatch() const override { return true; }

private:
	const byte* block;
	const size_t size;
};

} // namespace openmsx

#endif
