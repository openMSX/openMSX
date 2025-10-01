#include "MappedFile.hh"

#include "FileException.hh"
#include "LocalFile.hh"

#ifdef _WIN32
#  include <Windows.h>
#  include <io.h>  // _get_osfhandle
#else
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include <bit>
#include <cstdlib>

namespace openmsx {

static size_t getPageSize()
{
#ifdef _WIN32
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return static_cast<size_t>(sysInfo.dwPageSize);
#else
	return static_cast<size_t>(sysconf(_SC_PAGE_SIZE));
#endif
}

MappedFileImpl::MappedFileImpl(LocalFile& file, size_t extra, bool is_const)
{
	auto fileSize = file.getSize();
	sz = fileSize + extra;

#if !defined(_WIN32) && !defined(HAVE_MMAP)
	// Fallback, OS without mmap()
	ptr = malloc(sz);
	alloc = true;
	file.read({static_cast<uint8_t*>(ptr), fileSize});
	memset(static_cast<uint8_t*>(ptr) + fileSize, 0, extra);
	return;
#endif

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

	// Step 2: Try to extend with anonymous pages (Unix only)
#if !defined(_WIN32) && defined(HAVE_MMAP)
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
#endif

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
	if (mapped) {
		mapped = false;
		unmapFile(ptr, sz);
	}
	if (alloc) {
		alloc = false;
		free(ptr);
	}
	ptr = nullptr;
	sz = 0;
}

void MappedFileImpl::mapFile(LocalFile& file, bool is_const)
{
#ifdef _WIN32
	int fd = file.getFD();
	auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
	if (handle == INVALID_HANDLE_VALUE) {
		throw FileException("_get_osfhandle failed");
	}

	auto protect = is_const ? PAGE_READONLY : PAGE_WRITECOPY;
	mappingHandle = CreateFileMapping(handle, nullptr, protect, 0, 0, nullptr);
	if (!mappingHandle) {
		throw FileException("CreateFileMapping failed: ", GetLastError());
	}

	auto access = is_const ? FILE_MAP_READ : FILE_MAP_COPY;
	ptr = MapViewOfFile(mappingHandle, access, 0, 0, 0);
	if (!ptr) {
		auto err = GetLastError();
		CloseHandle(mappingHandle);
		throw FileException("MapViewOfFile failed: ", err);
	}

#elifdef HAVE_MMAP
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
#endif

	mapped = true;
}

void MappedFileImpl::unmapFile(void* p, size_t size)
{
#ifdef _WIN32
	UnmapViewOfFile(p);
	CloseHandle(mappingHandle);
	mappingHandle = nullptr;
#elifdef HAVE_MMAP
	munmap(p, size);
#endif
}

} // namespace openmsx
