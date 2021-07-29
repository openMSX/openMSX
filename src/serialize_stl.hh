#ifndef SERIALIZE_STL_HH
#define SERIALIZE_STL_HH

#include "serialize_core.hh"
#include "circular_buffer.hh"
#include <deque>
#include <iterator>
#include <vector>

namespace openmsx {

template<typename T> struct serialize_as_stl_collection : std::true_type
{
	static constexpr int size = -1; // variable size
	using value_type = typename T::value_type;
	// save
	static auto begin(const T& t) { return t.begin(); }
	static auto end  (const T& t) { return t.end();   }
	// load
	static constexpr bool loadInPlace = false;
	static void prepare(T& t, int /*n*/) {
		t.clear();
	}
	static auto output(T& t) {
		return std::back_inserter(t);
	}
};

//template<typename T> struct serialize_as_collection<std::list<T>>
//	: serialize_as_stl_collection<std::list<T>> {};

template<typename T> struct serialize_as_collection<std::deque<T>>
	: serialize_as_stl_collection<std::deque<T>> {};

//template<typename T1, typename T2> struct serialize_as_collection<std::map<T1, T2>>
//	: serialize_as_stl_collection<std::map<T1, T2>> {};

template<typename T> struct serialize_as_collection<std::vector<T>>
	: serialize_as_stl_collection<std::vector<T>>
{
	// Override load-part from base class.
	// Don't load vectors in-place, even though it's technically possible
	// and slightly more efficient. This is done to keep the correct vector
	// size at all intermediate steps. This may be important in case an
	// exception occurs during loading.
	static constexpr bool loadInPlace = false;
	static void prepare(std::vector<T>& v, int n) {
		v.clear(); v.reserve(n);
	}
	static auto output(std::vector<T>& v) {
		return std::back_inserter(v);
	}
};

template<typename T> struct serialize_as_collection<cb_queue<T>>
	: serialize_as_stl_collection<cb_queue<T>>
{
	static void prepare(cb_queue<T>& q, int n) {
		q.clear(); q.getBuffer().set_capacity(n);
	}
	static auto output(cb_queue<T>& q) {
		return std::back_inserter(q.getBuffer());
	}
};

} // namespace openmsx

#endif
