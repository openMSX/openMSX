#ifndef OBJECTPOOL_HH
#define OBJECTPOOL_HH

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include "narrow.hh"
#include "unreachable.hh"
#include "xrange.hh"

// ObjectPool
//
// This is a bit like a traditional pool-allocator, but with these differences:
// - Instead of only allocating and deallocating memory, this class also
//   constructs/destructs the objects contained in the pool. (But see the
//   remark below about destroying the ObjectPool itself).
// - The objects in this pool are not accessed via a pointer but instead via an
//   index. That is inserting a object into this pool returns an index. And that
//   index can later be used to retrieve the actual object or to remove the
//   object again. (As an optimization the insert operation additionally also
//   returns a pointer to the object).
//
// The choice of identifying objects via a (32-bit) index rather than via a
// pointer involves some trade-offs:
// - On a 64-bit system, storing (many) indices only takes half as much space
//   as storing pointers.
// - Accessing an object via an index is a bit slower than via a pointer.
// - Removing an object (via an index) is about the same speed as accessing the
//   object. Removing via pointer is not possible.
//
// Objects in this pool have stable addresses. IOW once you retrieve a pointer
// to an object that pointer remains valid. Also when more elements are inserted
// later. Even if (internally) the pool needs to allocate more memory.
//
// Memory is allocated in chunks (in the current implementation a chunk has
// room for 256 objects). Typically two objects that are inserted one after the
// other will be next to each other in memory (good for cache-locality). Though
// this is not a guarantee. When objects are removed, those free locations are
// re-filled first before allocating new memory.
//
// Inserting an object is done via the emplace() function. This will in-place
// constructs the object in the pool. So you cannot request uninitialized
// memory from the pool. Removing an object from the pool will call the
// destructor on that object. However when the ObjectPool itself is destructed
// it will NOT destruct all objects that are still present in the pool.
// Depending on the type of object and the state of your program this may or
// may not be a problem. E.g. maybe the object has a trivial destructor, or
// maybe it's fine to leak memory on exit.
//
// It's not possible to loop over all the objects that are present in the pool.

template<typename T>
class ObjectPool
{
public:
	using Index = uint32_t;

	struct EmplaceResult {
		Index idx;
		T* ptr;
	};

private:
	union Element {
		Element() {}
		~Element() {}

		Index next;
		T t;
	};
	using Element256 = std::array<Element, 256>;

public:
	template<typename... Args>
	[[nodiscard]] EmplaceResult emplace(Args&& ...args) {
		Index idx;
		if (free != Index(-1)) {
			idx = free;
			free = get(idx).next;
		} else {
			if (cntr == 0) {
				pool.push_back(std::make_unique<Element256>());
			}
			idx = Index(((pool.size() - 1) << 8) + cntr);
			++cntr;
		}
		T* ptr = &get(idx).t;
		new(ptr) T(std::forward<Args>(args)...);
		return {idx, ptr};
	}

	[[nodiscard]] const T& operator[](Index idx) const { return get(idx).t; }
	[[nodiscard]]       T& operator[](Index idx)       { return get(idx).t; }

	void remove(Index idx) {
		auto& elem = get(idx);
		elem.t.~T();
		elem.next = free;
		free = idx;
	}

	void remove(const T* ptr) {
		remove(ptr2Index(ptr));
	}

	Index ptr2Index(const T* ptr) const {
		for (auto i : xrange(pool.size())) {
			const auto& elem256 = *pool[i];
			const T* begin = &elem256[0].t;
			const T* end = begin + 256;
			if ((begin <= ptr) && (ptr < end)) {
				return Index((i << 8) + (ptr - begin));
			}
		}
		UNREACHABLE;
	}

	// for unittest
	[[nodiscard]] Index capacity() const {
		return narrow<Index>(256 * pool.size());
	}

private:
	[[nodiscard]] const Element& get(Index idx) const { return (*pool[idx >> 8])[idx & 255]; }
	[[nodiscard]]       Element& get(Index idx)       { return (*pool[idx >> 8])[idx & 255]; }

private:
	std::vector<std::unique_ptr<Element256>> pool;
	Index free = Index(-1);
	uint8_t cntr = 0;
};

#endif
