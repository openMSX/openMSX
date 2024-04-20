#ifndef DYNARRAY_HH
#define DYNARRAY_HH

// dynarray -- this models a dynamically sized array.
// This falls between a std::array and a std::vector.
//
// - Like vector the size of a dynarray is set at run-time (for std::array it's
//   set during compile-time).
// - Unlike vector the size of a dynarray can no longer change (grow/shrink)
//   after the initial construction.
//
// So qua functionality dynarray can do strictly less than a vector and strictly
// more than an array.
//
// The (only?) advantage of dynarray over vector is that the size of the
// dynarray itself is slightly smaller than the size of a vector (vector
// additionally needs to store the capacity; for dynarray size and capacity are
// always the same). (Typically on a 64-bit system: sizeof(vector)=24 and
// sizeof(dynarray)=16).
//
// This class is very similar to the array-version of std::unique_ptr (in fact
// the current implementation is based on unique_ptr<T[]>). The only addition is
// that dynarray also stores the size(*).
// (*) Technically, I think, unique_ptr<T[]> already stores the size internally
//     (typically at some offset in the allocated block). Though unfortunately
//     there's no portable way to access this size. So storing the size in the
//     dynarray class is necessary but strictly speaking redundant.
//
// The name and the interface of this class is based on the std::dynarray
// proposal for c++14. Though that proposal did not make it in c++14 and seems
// to be abandoned. Here's some (old) documentation:
//     https://www.enseignement.polytechnique.fr/informatique/INF478/docs/Cpp/en/cpp/container/dynarray.html
// This is not a full implementation of that proposal, just the easy parts
// and/or the parts that we currently need. Can easily be extended in the future
// (when needed).

#include <cassert>
#include <memory>

template<typename T>
class dynarray
{
public:
	dynarray() = default;
	dynarray(const dynarray&) = delete;
	dynarray& operator=(const dynarray&) = delete;
	~dynarray() = default;

	explicit dynarray(size_t n)
	        : m_size(n), m_data(std::make_unique<T[]>(n)) {}

	dynarray(size_t n, const T& value)
	        : m_size(n), m_data(std::make_unique<T[]>(n, value)) {}

	struct construct_from_range_tag {};
	template<typename SizedRange>
	dynarray(construct_from_range_tag, SizedRange&& range)
	        : m_size(std::size(range)), m_data(std::make_unique<T[]>(m_size)) {
		size_t i = 0;
		for (const auto& e : range) {
			m_data[i] = e;
			++i;
		}
	}

	dynarray(dynarray&& other) noexcept
		: m_size(other.m_size), m_data(std::move(other.m_data)) {
		other.m_size = 0;
		assert(!other.m_data);
	}

	dynarray& operator=(dynarray&& other) noexcept {
		if (this == &other) return *this;
		m_size = other.m_size;
		m_data = std::move(other.m_data);
		assert(!other.m_data);
		return *this;
	}

	[[nodiscard]] T& operator[](size_t i) {
		assert(i < m_size);
		return m_data[i];
	}
	[[nodiscard]] const T& operator[](size_t i) const {
		assert(i < m_size);
		return m_data[i];
	}

	[[nodiscard]] T& front() {
		assert(!empty());
		return m_data[0];
	}
	[[nodiscard]] const T& front() const {
		assert(!empty());
		return m_data[0];
	}
	[[nodiscard]] T& back() {
		assert(!empty());
		return m_data[m_size - 1];
	}
	[[nodiscard]] const T& back() const {
		assert(!empty());
		return m_data[m_size - 1];
	}

	[[nodiscard]] T* data() { return m_data.get(); }
	[[nodiscard]] const T* data() const { return m_data.get(); }

	[[nodiscard]] T* begin() { return m_data.get(); }
	[[nodiscard]] const T* begin() const { return m_data.get(); }
	[[nodiscard]] T* end() { return m_data.get() + m_size; }
	[[nodiscard]] const T* end() const { return m_data.get() + m_size; }

	[[nodiscard]] bool empty() const { return m_size == 0; }
	[[nodiscard]] size_t size() const { return m_size; }

private:
	size_t m_size = 0;
	std::unique_ptr<T[]> m_data;
};

#endif
