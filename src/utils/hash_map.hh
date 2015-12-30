// hash_map
//
// Written by Wouter Vermaelen, based upon HashSet/HashMap
// example code and ideas provided by Jacques Van Damme.

#ifndef HASH_MAP_HH
#define HASH_MAP_HH

#include "hash_set.hh"

namespace hash_set_impl {

// Takes any (const or non-const) pair reference and returns a reference to
// the first element of the pair.
struct ExtractFirst {
	// c++14:  template<typename Pair> auto& operator()(Pair&& p) const { return p.first; }

	template<typename First, typename Second>
	inline       First& operator()(      std::pair<First, Second>& p) const { return p.first; }

	template<typename First, typename Second>
	inline const First& operator()(const std::pair<First, Second>& p) const { return p.first; }
};

} // namespace hash_set_impl


// hash_map
//
// A hash-map implementation with an STL-like interface.
//
// It builds upon hash-set, see there for more details.
template<typename Key,
         typename Value,
         typename Hasher = std::hash<Key>,
         typename Equal = EqualTo>
class hash_map : public hash_set<std::pair<Key, Value>, hash_set_impl::ExtractFirst, Hasher, Equal>
{
	using BaseType = hash_set<std::pair<Key, Value>, hash_set_impl::ExtractFirst, Hasher, Equal>;
public:
	using key_type    = Key;
	using mapped_type = Value;
	using value_type  = std::pair<Key, Value>;
	using       iterator = typename BaseType::      iterator;
	using const_iterator = typename BaseType::const_iterator;

	hash_map(unsigned initialSize = 0,
	         Hasher hasher_ = Hasher(),
	         Equal equal_ = Equal())
		: BaseType(initialSize, hash_set_impl::ExtractFirst(), hasher_, equal_)
	{
	}

	template<typename K>
	Value& operator[](K&& key)
	{
		auto it = this->find(key);
		if (it == this->end()) {
			auto p = this->insert(value_type(std::forward<K>(key), Value()));
			it = p.first;
		}
		return it->second;
	}

	// Proposed for C++17. Is equivalent to 'operator[](key) = value', but:
	// - also works for non-default-constructible types
	// - returns more information
	// - is slightly more efficient
	template<typename K, typename V>
	std::pair<iterator, bool> insert_or_assign(K&& key, V&& value)
	{
		auto it = this->find(key);
		if (it == this->end()) {
			// insert, return pair<iterator, true>
			return this->insert(value_type(std::forward<K>(key), std::forward<V>(value)));
		} else {
			// assign, return pair<iterator, false>
			it->second = std::forward<V>(value);
			return std::make_pair(it, false);
		}
	}
};

#endif
