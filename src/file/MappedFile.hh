#ifndef MAPPED_FILE_HH
#define MAPPED_FILE_HH

#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

namespace openmsx {

class LocalFile;

/** RAII utility to memory-map a file into the process address space
 *  and expose it as a std::span<T>.
 *
 * Usage:
 * - MappedFile<const uint8_t>: read-only view of file contents.
 * - MappedFile<uint8_t>      : writable buffer, modifications are not propagated
 *                              back to the file.
 *
 * Notes:
 * - Only trivially copyable/destructible types T are allowed.
 * - If the file size is not a multiple of sizeof(T), the final partial
 *   element is excluded.
 * - The mapping is page-aligned, alignof(T) must not exceed 4096.
 */
class MappedFileImpl
{
public:
	MappedFileImpl(const MappedFileImpl&) = delete;
	MappedFileImpl& operator=(const MappedFileImpl&) = delete;

	MappedFileImpl(MappedFileImpl&& other) noexcept {
		move_from(std::move(other));
	}
	MappedFileImpl& operator=(MappedFileImpl&& other) noexcept {
		if (this != &other) {
			release();
			move_from(std::move(other));
		}
		return *this;
	}

	// A LocalFile can use mmap() (if supported by the OS).
	MappedFileImpl(LocalFile& file, bool is_const);

	// For a non-LocalFile (e.g. a compressed file), we can't use mmap().
	MappedFileImpl(std::span<const uint8_t> buf, bool is_const);

	~MappedFileImpl() { release(); }

	[[nodiscard]] void* data() const { return ptr; }
	[[nodiscard]] size_t size() const { return sz; }

private:
	void move_from(MappedFileImpl&& other) noexcept {
		ptr = std::exchange(other.ptr, nullptr);
		sz = std::exchange(other.sz, 0);
	#ifdef _WIN32
		mappingHandle std::exchange(other.mappingHandle, nullptr);
	#endif
		alloc = std::exchange(other.alloc, false);
		mapped = std::exchange(other.mapped, false);
	}

	void release() noexcept;

private:
	void* ptr = nullptr;
	size_t sz = 0;
#ifdef _WIN32
	HANDLE mappingHandle = nullptr;
#endif
	bool alloc = false;
	bool mapped = false;
};

template<typename T> class MappedFile
{
	static_assert(std::is_trivially_copyable_v<T>, "Mapped type must be trivially copyable");
	static_assert(std::is_trivially_destructible_v<T>, "Mapped type must be trivially destructible");
	static_assert(alignof(T) <= 4096, "Mapped type alignment must not exceed typical page size");

public:
	MappedFile(MappedFileImpl&& impl_) : impl(std::move(impl_)) {}

	[[nodiscard]] T* data() const { return static_cast<T*>(impl.data()); }
	[[nodiscard]] size_t size() const { return impl.size() / sizeof(T); }
	[[nodiscard]] bool empty() const { return impl.size() == 0; }
	[[nodiscard]] T* begin() const { return data(); }
	[[nodiscard]] T* end() const { return data() + size(); }
	[[nodiscard]] T& operator[](size_t i) const { assert(i < size()); return data()[i]; }
	[[nodiscard]] T& front() const { assert(!empty()); return data()[0]; }
	[[nodiscard]] T& back()  const { assert(!empty()); return data()[size() - 1]; }

private:
	MappedFileImpl impl;
};

} // namespace openmsx

#endif
