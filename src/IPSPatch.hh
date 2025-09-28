#ifndef IPSPATCH_HH
#define IPSPATCH_HH

#include "PatchInterface.hh"

#include "Filename.hh"

#include "MemBuffer.hh"

#include <memory>
#include <vector>

namespace openmsx {

class IPSPatch final : public PatchInterface
{
public:
	IPSPatch(Filename filename,
	         std::unique_ptr<const PatchInterface> parent);

	void copyBlock(size_t src, std::span<uint8_t> dst) const override;
	[[nodiscard]] size_t getSize() const override { return size; }
	[[nodiscard]] std::vector<Filename> getFilenames() const override;

private:
	struct Chunk {
		size_t startAddress;
		MemBuffer<uint8_t> content;

		[[nodiscard]] size_t size() const { return content.size(); }
		[[nodiscard]] size_t stopAddress() const { return startAddress + size(); }
		[[nodiscard]] auto begin() const { return content.begin(); }
		[[nodiscard]] auto end  () const { return content.end(); }
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
