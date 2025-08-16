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

MappedFileImpl::MappedFileImpl(LocalFile& file, bool is_const)
	: sz(file.getSize())
{
	if (sz == 0) return;

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
	mapped = true;

#elif HAVE_MMAP
	auto prot = PROT_READ | (is_const ? 0 : PROT_WRITE);
	auto flags = MAP_PRIVATE;
	#ifndef __APPLE__
	flags |= MAP_POPULATE; // MAP_POPULATE not supported on macOS
	#endif

	int fd = file.getFD();
	ptr = mmap(nullptr, sz, prot, flags, fd, 0);
	// MAP_FAILED is #define'd using an old-style cast, we
	// have to redefine it ourselves to avoid a warning
	void* MY_MAP_FAILED = std::bit_cast<void*>(intptr_t(-1));
	if (ptr == MY_MAP_FAILED) {
		throw FileException("mmap failed");
	}
	#ifdef __APPLE__
	madvise(ptr, sz, MADV_WILLNEED); // instead of MAP_POPULATE
	#endif
	mapped = true;

#else
	ptr = malloc(sz);
	alloc = true;
	file.read({static_cast<uint8_t*>(ptr), sz});
#endif
}

MappedFileImpl::MappedFileImpl(std::span<const uint8_t> buf, bool is_const)
	: sz(buf.size())
{
	if (is_const) {
		// conversion to 'NON-const void*' is fine, we only ever
		// re-expose this as a 'const T*'
		ptr = const_cast<uint8_t*>(buf.data());
	} else {
		ptr = malloc(sz);
		alloc = true;
		memcpy(ptr, buf.data(), sz);
	}
}

void MappedFileImpl::release() noexcept
{
#ifdef _WIN32
	if (mapped) {
		mapped = false;
		UnmapViewOfFile(ptr);
		CloseHandle(mappingHandle);
		mappingHandle = nullptr;
	}
#elif HAVE_MMAP
	if (mapped) {
		mapped = false;
		munmap(ptr, sz);
	}
#endif
	if (alloc) {
		alloc = false;
		free(ptr);
	}
	ptr = nullptr;
	sz = 0;
}

} // namespace openmsx
