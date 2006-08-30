// $Id$

#ifndef SHARED_PTR_HH
#define SHARED_PTR_HH

/**
 * Based on boost::shared_ptr
 *   http://www.boost.org/libs/smart_ptr/shared_ptr.htm
 * The boost version is more complete and faster but at the
 * moment we don't care about that. 
 */

#include <algorithm>
#include <cassert>

template<typename T> class shared_ptr
{
public:
	explicit shared_ptr(T* p = 0)
		: ptr(p), count(new unsigned(1)) {}

	shared_ptr(const shared_ptr<T>& other)
		: ptr(other.ptr), count(other.count)
	{
		++(*count);
	}
	
	~shared_ptr()
	{
		if (--(*count) == 0) {
			delete ptr;
			delete count;
		}
	}

	void swap(shared_ptr<T>& other)
	{
		std::swap(ptr, other.ptr);
		std::swap(count, other.count);
	}

	shared_ptr& operator=(shared_ptr other) // note: pass by value
		{ swap(other); return *this; }
	void reset(T* t = 0)
		{ shared_ptr<T>(t).swap(*this); }

	T& operator* () const { assert(ptr); return *ptr; }
	T* operator->() const { assert(ptr); return  ptr; }
	T* get() const { return ptr; }

private:
	T* ptr;
	unsigned* count;
};

template<typename T, typename U>
bool operator==(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs)
{
	return lhs.get() == rhs.get();
}
template<typename T, typename U>
bool operator!=(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs)
{
	return lhs.get() != rhs.get();
}
template<typename T, typename U>
bool operator<(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs)
{
	return lhs.get() < rhs.get();
}

#endif
