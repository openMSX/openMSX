#ifndef ARRAY_REF_HH
#define ARRAY_REF_HH

#include <algorithm>
#include <iterator>
#include <vector>
#include <cassert>
#include <cstddef>

/** This class implements a subset of the proposal for std::array_ref
  * (proposed for the next c++ standard (c++1y)).
  *    http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3334.html#classstd_1_1array__ref
  *
  * It has an interface that is close to std::vector, but it does not own the
  * memory of the array. Basically it's just a wrapper around a pointer plus
  * length.
  */
template<typename T>
class array_ref
{
public:
	// types
	using value_type      =       T;
	using pointer         = const T*;
	using       reference = const T&;
	using const_reference = const T&;
	using         const_iterator = const T*;
	using               iterator = const_iterator;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	using       reverse_iterator = const_reverse_iterator;
	using size_type       = size_t;
	using difference_type = ptrdiff_t;

	// construct/copy/assign
	array_ref()
		: dat(nullptr), siz(0) {}
	array_ref(const array_ref& r)
		: dat(r.data()), siz(r.size()) {}
	array_ref(const T* array, size_t length)
		: dat(array), siz(length) {}
	template<typename ITER> array_ref(ITER first, ITER last)
		: dat(&*first), siz(std::distance(first, last)) {}
	/*implicit*/ array_ref(const std::vector<T>& v)
		: dat(v.data()), siz(v.size()) {}
	/*implicit*/ template<size_t N> array_ref(const T(&a)[N])
		: dat(a), siz(N) {}

	array_ref& operator=(const array_ref& rhs) {
		dat = rhs.data();
		siz = rhs.size();
		return *this;
	}

	// iterators
	const_iterator begin() const { return dat; }
	const_iterator end()   const { return dat + siz; }
	const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
	const_reverse_iterator rend()   const { return const_reverse_iterator(begin()); }

	// capacity
	size_type size()  const { return siz; }
	bool      empty() const { return siz == 0; }

	// element access
	const T& operator[](size_t i) const {
		assert(i < siz);
		return dat[i];
	}
	const T& front() const { return (*this)[0]; }
	const T& back()  const { return (*this)[siz - 1]; }
	const T* data()  const { return dat; }

	// mutators
	void clear() { siz = 0; } // no need to change 'dat'
	void remove_prefix(size_type n) {
		if (n <= siz) {
			dat += n;
			siz -= n;
		} else {
			clear();
		}
	}
	void remove_suffix(size_type n) {
		if (n <= siz) {
			siz -= n;
		} else {
			clear();
		}
	}
	void pop_back()  { remove_suffix(1); }
	void pop_front() { remove_prefix(1); }

	array_ref substr(size_type pos, size_type n = size_type(-1)) const {
		if (pos >= siz) return array_ref();
		return array_ref(dat + pos, std::min(n, siz - pos));
	}

private:
	const T* dat;
	size_type siz;
};

// deducing constructor wrappers
template<typename T> inline array_ref<T> make_array_ref(const T* array, size_t length)
{
	return array_ref<T>(array, length);
}

template<typename T, size_t N> inline array_ref<T> make_array_ref(const T(&a)[N])
{
	return array_ref<T>(a);
}

template<typename T> inline array_ref<T> make_array_ref(const std::vector<T>& v)
{
	return array_ref<T>(v);
}

#endif
