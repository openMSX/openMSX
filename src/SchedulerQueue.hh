#ifndef SCHEDULERQUEUE_HH
#define SCHEDULERQUEUE_HH

#include "MemBuffer.hh"
#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstdlib>

namespace openmsx {

// This is similar to a sorted vector<T>. Though this container can have spare
// capacity both at the front and at the end (vector only at the end). This
// means that when you remove the smallest element and insert a new element
// that's only slightly bigger than the smallest one, there's a very good
// chance this insert runs in O(1) (for vector it's O(N), with N the size of
// the vector). This is a scenario that occurs very often in the Scheduler
// code.
template<typename T> class SchedulerQueue
{
public:
	static constexpr int CAPACITY = 32; // initial capacity
	static constexpr int SPARE_FRONT = 1;
	SchedulerQueue()
		: storage   (CAPACITY + 1) // one extra for sentinel
		, storageEnd(storage.data() + CAPACITY)
		, useBegin  (storage.data() + SPARE_FRONT)
		, useEnd    (useBegin)
	{
	}

	[[nodiscard]] size_t capacity()   const { return storageEnd - storage.data(); }
	[[nodiscard]] size_t spareFront() const { return useBegin   - storage.data(); }
	[[nodiscard]] size_t spareBack()  const { return storageEnd - useEnd;       }
	[[nodiscard]] size_t size()  const { return useEnd -  useBegin; }
	[[nodiscard]] bool   empty() const { return useEnd == useBegin; }

	// Returns reference to the first element, This is the smallest element
	// according to the sorting criteria, see insert().
	[[nodiscard]]       T& front()       { return *useBegin; }
	[[nodiscard]] const T& front() const { return *useBegin; }

	[[nodiscard]]       T* begin()       { return useBegin; }
	[[nodiscard]] const T* begin() const { return useBegin; }
	[[nodiscard]]       T* end()         { return useEnd;   }
	[[nodiscard]] const T* end()   const { return useEnd;   }

	// Insert new element.
	// Elements are sorted according to the given LESS predicate.
	// SET_SENTINEL must set an element to its maximum value (so that
	// 'less(x, sentinel)' is true for any x).
	// (Important) two elements that are equivalent according to 'less'
	// keep their relative order, IOW newly inserted elements are inserted
	// after existing equivalent elements.
	void insert(const T& t, std::invocable<T&> auto setSentinel, std::equivalence_relation<T, T> auto less)
	{
		setSentinel(*useEnd); // put sentinel at the end
		assert(less(t, *useEnd));

		T* it = useBegin;
		while (!less(t, *it)) ++it;

		if ((it - useBegin) <= (useEnd - it)) {
			if (useBegin != storage.data()) [[likely]] {
				insertFront(it, t);
			} else if (useEnd != storageEnd) {
				insertBack(it, t);
			} else {
				insertRealloc(it, t);
			}
		} else {
			if (useEnd != storageEnd) [[likely]] {
				insertBack(it, t);
			} else if (useBegin != storage.data()) {
				insertFront(it, t);
			} else {
				insertRealloc(it, t);
			}
		}
	}

	// Remove the smallest element.
	void remove_front()
	{
		assert(!empty());
		++useBegin;
	}

	// Remove the first element for which the given predicate returns true.
	bool remove(std::predicate<T> auto p)
	{
		T* it = std::find_if(useBegin, useEnd, p);
		if (it == useEnd) return false;

		if ((it - useBegin) < (useEnd - it - 1)) [[unlikely]] {
			++useBegin;
			std::copy_backward(useBegin - 1, it, it + 1);
		} else {
			std::copy(it + 1, useEnd, it);
			--useEnd;
		}
		return true;
	}

	// Remove all elements for which the given predicate returns true.
	void remove_all(std::predicate<T> auto p)
	{
		useEnd = std::remove_if(useBegin, useEnd, p);
	}

private:
	void insertFront(T* it, const T& t)
	{
		--useBegin;
		std::copy(useBegin + 1, it, useBegin);
		--it;
		*it = t;
	}
	void insertBack(T* it, const T& t)
	{
		++useEnd;
		std::copy_backward(it, useEnd -1, useEnd);
		*it = t;
	}
	void insertRealloc(T* it, const T& t)
	{
		size_t oldSize = storageEnd - storage.data();
		size_t newSize = oldSize * 2;

		MemBuffer<T> newStorage(newSize + 1); // one extra for sentinel
		T* newUseBegin = newStorage.data() + SPARE_FRONT;
		std::copy(useBegin, it, newUseBegin);
		*(newUseBegin + (it - useBegin)) = t;
		std::copy(it, useEnd, newUseBegin + (it - useBegin) + 1);

		storage    = std::move(newStorage);
		storageEnd = storage.data() + newSize;
		useBegin   = newUseBegin;
		useEnd     = useBegin + oldSize + 1;
	}

private:
	// Invariant: storage.data() <= useBegin <= useEnd <= storageEnd
	MemBuffer<T> storage;
	T* storageEnd; // == storage.end() - 1 // for sentinel
	T* useBegin;
	T* useEnd;
};

} // namespace openmsx

#endif // SCHEDULERQUEUE_HH
