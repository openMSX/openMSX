#ifndef MEMORY_RESOURCE
#define MEMORY_RESOURCE

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>

// A minimal re-implementation of std::monotonic_buffer_resource.
// Because libc++ doesn't have this yet, even though it's part of c++17.

class monotonic_allocator
{
public:
	monotonic_allocator() = default;

	explicit monotonic_allocator(size_t initialSize)
		: nextSize(initialSize)
	{
		assert(initialSize != 0);
	}

	monotonic_allocator(void* buffer, size_t bufferSize)
		: current(buffer)
		, available(bufferSize)
		, nextSize(2 * bufferSize)
	{
		assert(buffer || bufferSize == 0);
	}

	monotonic_allocator(const monotonic_allocator&) = delete;
	monotonic_allocator(monotonic_allocator&&) = delete;
	monotonic_allocator& operator=(const monotonic_allocator&) = delete;
	monotonic_allocator& operator=(monotonic_allocator&&) = delete;

	~monotonic_allocator()
	{
		void* p = head;
		while (p) {
			void* next = *static_cast<void**>(p);
			free(p);
			p = next;
		}
	}

	[[nodiscard]] void* allocate(size_t bytes, size_t alignment)
	{
		assert(alignment <= alignof(max_align_t));

		if (bytes == 0) bytes = 1;
		void* p = std::align(alignment, bytes, current, available);
		if (!p) {
			newBuffer(bytes);
			p = current;
		}
		current = static_cast<char*>(current) + bytes;
		available -= bytes;
		return p;
	}

private:
	void newBuffer(size_t bytes)
	{
		size_t n = std::max(nextSize, bytes);
		void* newBuf = malloc(n + sizeof(void*));
		if (!newBuf) {
			throw std::bad_alloc();
		}

		auto** p = static_cast<void**>(newBuf);
		p[0] = head;

		current = static_cast<void*>(&p[1]);
		available = n;
		nextSize = 2 * n;
		head = newBuf;
	}

private:
	void* current = nullptr;
	size_t available = 0;
	size_t nextSize = 1024;
	void* head = nullptr;
};

#endif
