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
	template<typename Pair> [[nodiscard]] auto& operator()(Pair&& p) const { return p.first; }
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
         typename Equal = std::equal_to<>>
class hash_map : public hash_set<std::pair<Key, Value>, hash_set_impl::ExtractFirst, Hasher, Equal>
{
	using BaseType = hash_set<std::pair<Key, Value>, hash_set_impl::ExtractFirst, Hasher, Equal>;
public:
	using key_type    = Key;
	using mapped_type = Value;
	using value_type  = std::pair<Key, Value>;
	using       iterator = typename BaseType::      iterator;
	using const_iterator = typename BaseType::const_iterator;

	explicit hash_map(unsigned initialSize = 0,
	         Hasher hasher_ = Hasher(),
	         Equal equal_ = Equal())
		: BaseType(initialSize, hash_set_impl::ExtractFirst(), hasher_, equal_)
	{
	}

	hash_map(std::initializer_list<std::pair<Key, Value>> list)
		: BaseType(list)
	{
	}

	template<typename K>
	[[nodiscard]] Value& operator[](K&& key)
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
			return std::pair(it, false);
		}
	}

	template<typename K>
	[[nodiscard]] bool contains(const K& k) const
	{
		return this->find(k) != this->end();
	}
};


template<typename Key, typename Value, typename Hasher, typename Equal, typename Key2>
[[nodiscard]] const Value* lookup(const hash_map<Key, Value, Hasher, Equal>& map, const Key2& key)
{
	auto it = map.find(key);
	return (it != map.end()) ? &it->second : nullptr;
}

template<typename Key, typename Value, typename Hasher, typename Equal, typename Key2>
[[nodiscard]] Value* lookup(hash_map<Key, Value, Hasher, Equal>& map, const Key2& key)
{
	auto it = map.find(key);
	return (it != map.end()) ? &it->second : nullptr;
}

#endif
