// $Id$

#ifndef SHARED_PTR_HH
#define SHARED_PTR_HH

/** checked_delete utility copied from boost::checked_delete
  *   http://www.boost.org/doc/libs/1_36_0/libs/utility/checked_delete.html
  * Is like a normal delete, but given a compilation error (as opposed to a
  * warning in gcc) when the to be deleted type is an incomplete type.
  * Deleting an incomplete type with a non-trivial destructor is undefined
  * behaviour in c++.
  */
template<typename T> void checked_delete(T* t)
{
	typedef char type_must_be_complete[sizeof(T) ? 1 : -1];
	(void)sizeof(type_must_be_complete);
	delete t;
}

/**
 * Based on boost::shared_ptr
 *   http://www.boost.org/libs/smart_ptr/shared_ptr.htm
 * The boost version is more complete and faster but at the
 * moment we don't care about that.
 */

#include <algorithm>
#include <cassert>

// The code below looks unnecessarily complex, but it's structured like this
// to move the delete (checked_delete) operation out of the shared_ptr class.
// This allows to instantiate shared_ptr with an incomplete type. Only the
// constructor that creates a shared_ptr from a raw pointer needs a complete
// type (but it anyway already needs it to be able to do 'new T').

struct shared_ptr_impl_base
{
	shared_ptr_impl_base() : count(1) {}
	virtual ~shared_ptr_impl_base() { assert(count == 0); }
	unsigned count;
};

template<typename T> struct shared_ptr_impl : shared_ptr_impl_base
{
	shared_ptr_impl(T* t_) : t(t_) {}
	virtual ~shared_ptr_impl() { checked_delete(t); }
private:
	T* t;
};

template<typename T> class shared_ptr
{
public:
	explicit shared_ptr(T* p = nullptr)
		: ptr(p), impl(new shared_ptr_impl<T>(p)) {}

	shared_ptr(const shared_ptr<T>& other)
		: ptr(other.ptr), impl(other.impl)
	{
		++(impl->count);
	}

	~shared_ptr()
	{
		if (--(impl->count) == 0) {
			delete impl;
		}
	}

	void swap(shared_ptr<T>& other)
	{
		std::swap(ptr, other.ptr);
		std::swap(impl, other.impl);
	}

	shared_ptr& operator=(shared_ptr other) // note: pass by value
		{ swap(other); return *this; }
	void reset(T* t = nullptr)
		{ shared_ptr<T>(t).swap(*this); }

	T& operator* () const { assert(ptr); return *ptr; }
	T* operator->() const { assert(ptr); return  ptr; }
	T* get() const { return ptr; }

	bool unique() const { return use_count() == 1; }
	unsigned use_count() const { return impl->count; }

private:
	T* ptr;
	shared_ptr_impl_base* impl;
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
