#ifndef SERIALIZE_STL_HH
#define SERIALIZE_STL_HH

#include "serialize_core.hh"
#include "circular_buffer.hh"
#include <vector>
#include <iterator>

namespace openmsx {

template<typename T> struct serialize_as_stl_collection : std::true_type
{
	static const int size = -1; // variable size
	using value_type = typename T::value_type;
	// save
	using const_iterator = typename T::const_iterator;
	static const_iterator begin(const T& t) { return t.begin(); }
	static const_iterator end  (const T& t) { return t.end();   }
	// load
	static const bool loadInPlace = false;
	using output_iterator = typename std::insert_iterator<T>;
	static void prepare(T& t, int /*n*/) {
		t.clear();
	}
	static output_iterator output(T& t) {
		return std::inserter(t, begin(t));
	}
};

//template<typename T> struct serialize_as_collection<std::list<T>>
//	: serialize_as_stl_collection<std::list<T>> {};

//template<typename T> struct serialize_as_collection<std::set<T>>
//	: serialize_as_stl_collection<std::set<T>> {};

//template<typename T> struct serialize_as_collection<std::deque<T>>
//	: serialize_as_stl_collection<std::deque<T>> {};

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
	static const bool loadInPlace = false;
	using output_iterator =
		typename std::back_insert_iterator<std::vector<T>>;
	static void prepare(std::vector<T>& v, int n) {
		v.clear(); v.reserve(n);
	}
	static output_iterator output(std::vector<T>& v) {
		return std::back_inserter(v);
	}
};

template<typename T> struct serialize_as_collection<cb_queue<T>>
	: serialize_as_stl_collection<cb_queue<T>>
{
	using output_iterator =
		typename std::back_insert_iterator<circular_buffer<T>>;
	static void prepare(cb_queue<T>& q, int n) {
		q.clear(); q.getBuffer().set_capacity(n);
	}
	static output_iterator output(cb_queue<T>& q) {
		return std::back_inserter(q.getBuffer());
	}
};

} // namespace openmsx

#endif
