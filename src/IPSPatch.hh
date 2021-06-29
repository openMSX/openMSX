#ifndef IPSPATCH_HH
#define IPSPATCH_HH

#include "PatchInterface.hh"
#include "Filename.hh"
#include <vector>
#include <memory>

namespace openmsx {

class IPSPatch final : public PatchInterface
{
public:
	IPSPatch(Filename filename,
	         std::unique_ptr<const PatchInterface> parent);

	void copyBlock(size_t src, byte* dst, size_t num) const override;
	[[nodiscard]] size_t getSize() const override { return size; }
	[[nodiscard]] std::vector<Filename> getFilenames() const override;

private:
	struct Chunk {
		size_t startAddress;
		std::vector<byte> content;

		[[nodiscard]] size_t size() const { return content.size(); }
		[[nodiscard]] const byte* data() const { return content.data(); }
		[[nodiscard]] size_t stopAddress() const { return startAddress + size(); }
	};

	const Filename filename;
	const std::unique_ptr<const PatchInterface> parent;
	const std::vector<Chunk> chunks; // sorted on startAddress
	const size_t size;

private:
	// Helper functions called from constructor
	[[nodiscard]] std::vector<Chunk> parseChunks() const;
	[[nodiscard]] size_t calcSize() const;
};

} // namespace openmsx

#endif
