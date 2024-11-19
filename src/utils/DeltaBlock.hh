#ifndef DELTA_BLOCK_HH
#define DELTA_BLOCK_HH

#define STATISTICS 0

#include "MemBuffer.hh"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#ifdef DEBUG
#include "sha1.hh"
#endif

namespace openmsx {

class DeltaBlock
{
public:
#if STATISTICS
	virtual ~DeltaBlock();
#else
	virtual ~DeltaBlock() = default;
#endif
	virtual void apply(std::span<uint8_t> dst) const = 0;

protected:
	DeltaBlock() = default;

#ifdef DEBUG
public:
	Sha1Sum sha1;
#endif

#if STATISTICS
protected:
	static inline size_t globalAllocSize = 0;
	size_t allocSize;
#endif
};


class DeltaBlockCopy final : public DeltaBlock
{
public:
	explicit DeltaBlockCopy(std::span<const uint8_t> data);
	void apply(std::span<uint8_t> dst) const override;
	void compress(size_t size);
	[[nodiscard]] const uint8_t* getData();

private:
	[[nodiscard]] bool compressed() const { return compressedSize != 0; }

	MemBuffer<uint8_t> block;
	size_t compressedSize = 0;
};


class DeltaBlockDiff final : public DeltaBlock
{
public:
	DeltaBlockDiff(std::shared_ptr<DeltaBlockCopy> prev_,
	               std::span<const uint8_t> data);
	void apply(std::span<uint8_t> dst) const override;
	[[nodiscard]] size_t getDeltaSize() const;

private:
	const std::shared_ptr<DeltaBlockCopy> prev;
	const std::vector<uint8_t> delta; // TODO could be tweaked to use OutputBuffer
};


class LastDeltaBlocks
{
public:
	[[nodiscard]] std::shared_ptr<DeltaBlock> createNew(
		const void* id, std::span<const uint8_t> data);
	[[nodiscard]] std::shared_ptr<DeltaBlock> createNullDiff(
		const void* id, std::span<const uint8_t> data);
	void clear();

private:
	struct Info {
		Info(const void* id_, size_t size_)
			: id(id_), size(size_) {}

		const void* id;
		size_t size;
		std::weak_ptr<DeltaBlockCopy> ref;
		std::weak_ptr<DeltaBlock> last;
		size_t accSize = 0;
	};

	std::vector<Info> infos;
};

} // namespace openmsx

#endif
