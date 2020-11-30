#ifndef IPSPATCH_HH
#define IPSPATCH_HH

#include "PatchInterface.hh"
#include "Filename.hh"
#include <utility>
#include <vector>
#include <memory>

namespace openmsx {

class IPSPatch final : public PatchInterface
{
public:
	using PatchMap = std::vector<std::pair<size_t, std::vector<byte>>>;

	IPSPatch(Filename filename,
	         std::unique_ptr<const PatchInterface> parent);

	void copyBlock(size_t src, byte* dst, size_t num) const override;
	[[nodiscard]] size_t getSize() const override;
	[[nodiscard]] std::vector<Filename> getFilenames() const override;

private:
	const Filename filename;
	const std::unique_ptr<const PatchInterface> parent;
	PatchMap patchMap;
	size_t size;
};

} // namespace openmsx

#endif
