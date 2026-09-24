#include "MappedFile.hh"

#include "FileException.hh"
#include "LocalFile.hh"
#include "systemfuncs.hh"

#if HAVE_MMAP
#  include <sys/mman.h>
#  include <unistd.h>
#endif

#include <bit>
#include <cstdlib>
#include <memory>

namespace openmsx {

#if HAVE_MMAP
static size_t getPageSize()
{
	return static_cast<size_t>(sysconf(_SC_PAGE_SIZE));
}
#endif

MappedFileImpl::MappedFileImpl(LocalFile& file, size_t extra, bool is_const)
{
	auto fileSize = file.getSize();
	sz = fileSize + extra;

#if !HAVE_MMAP
	// Buffered snapshot on Windows and other platforms without mmap().
	// Unlike a Windows file mapping, this does not lock the backing file.
	(void)is_const;
	std::unique_ptr<void, decltype(&free)> buffer(malloc(sz ? sz : 1), &free);
	if (!buffer) throw std::bad_alloc();
	auto pos = file.getPos();
	file.seek(0);
	file.read({static_cast<uint8_t*>(buffer.get()), fileSize});
	file.seek(pos);
	memset(static_cast<uint8_t*>(buffer.get()) + fileSize, 0, extra);
	ptr = buffer.release();
	alloc = true;
#else

	// Step 0: empty file (cannot be mmap()'ed).
	if (fileSize == 0) {
		ptr = calloc(extra, 1);
		return;
	}

	// Step 1: mmap the file
	mapFile(file, is_const);
	if (extra == 0) {
		return; // common case: no extra bytes requested, we're done
	}

	// calculate page alignment and extra space requirements
	auto pageSize = getPageSize();
	assert((pageSize & (pageSize - 1)) == 0); // assume pageSize is a power of 2
	// for a power-of-2 pageSize the following is equivalent to:
	//   (pageSize - (fileSize % pageSize)) % pageSize
	auto bytesAvailableInPage = -fileSize & (pageSize - 1);

	if (bytesAvailableInPage >= extra) {
		// Step 1 is sufficient, file page padding gives us the extra bytes
		return;
	}

	// Step 2: Try to extend with anonymous pages
	auto additionalBytesNeeded = extra - bytesAvailableInPage;
	auto extraPagesNeeded = (additionalBytesNeeded + pageSize - 1) / pageSize; // round up
	auto extraMappingSize = extraPagesNeeded * pageSize;
	auto* extraStartAddr = static_cast<char*>(ptr) + fileSize + bytesAvailableInPage;

	auto* anonymousMap = mmap(extraStartAddr, extraMappingSize, PROT_READ | PROT_WRITE,
	                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	// MAP_FAILED is #define'd using an old-style cast, we
	// have to redefine it ourselves to avoid a warning
	void* MY_MAP_FAILED = std::bit_cast<void*>(intptr_t(-1));
	if (anonymousMap != MY_MAP_FAILED) {
		// Success! Verify mapping is at expected address
		assert(anonymousMap == extraStartAddr); // MAP_FIXED should guarantee this
		return;
	}

	// Step 3: Fall back to malloc
	auto* buffer = static_cast<uint8_t*>(malloc(sz));
	if (!buffer) {
		unmapFile(ptr, fileSize);
		throw std::runtime_error("Failed to allocate memory");
	}

	memcpy(buffer, ptr, fileSize);
	memset(buffer + fileSize, 0, extra);

	// Clean up the file mapping since we copied to malloc buffer
	unmapFile(ptr, fileSize);

	mapped = false;
	alloc = true; // we're using malloc now
	ptr = buffer;
#endif
}

MappedFileImpl::MappedFileImpl(std::span<const uint8_t> buf, size_t extra, bool is_const)
	: sz(buf.size() + extra)
{
	if (is_const && (extra == 0)) {
		// conversion to 'NON-const void*' is fine, we only ever
		// re-expose this as a 'const T*'
		ptr = const_cast<uint8_t*>(buf.data());
	} else {
		ptr = malloc(sz);
		alloc = true;
		memcpy(ptr, buf.data(), buf.size());
		memset(static_cast<uint8_t*>(ptr) + buf.size(), 0, extra);
	}
}

void MappedFileImpl::release() noexcept
{
#if HAVE_MMAP
	if (mapped) {
		mapped = false;
		unmapFile(ptr, sz);
	}
#endif
	if (alloc) {
		alloc = false;
		free(ptr);
	}
	ptr = nullptr;
	sz = 0;
}

#if HAVE_MMAP
void MappedFileImpl::mapFile(LocalFile& file, bool is_const)
{
	auto prot = PROT_READ | (is_const ? 0 : PROT_WRITE);
	auto flags = MAP_PRIVATE;
	#ifndef __APPLE__
	flags |= MAP_POPULATE; // MAP_POPULATE not supported on macOS
	#endif

	int fd = file.getFD();
	ptr = mmap(nullptr, sz, prot, flags, fd, 0);
	void* MY_MAP_FAILED = std::bit_cast<void*>(intptr_t(-1)); // see above
	if (ptr == MY_MAP_FAILED) {
		throw FileException("mmap failed");
	}
	#ifdef __APPLE__
	madvise(ptr, sz, MADV_WILLNEED); // instead of MAP_POPULATE
	#endif

	mapped = true;
}

void MappedFileImpl::unmapFile(void* p, size_t size)
{
	munmap(p, size);
}
#endif

} // namespace openmsx
