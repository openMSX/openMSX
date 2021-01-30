#ifndef SIMPLEHASHSET_HH
#define SIMPLEHASHSET_HH

#include "Math.hh"
#include <algorithm>
#include <cassert>
#include <type_traits>

#if __has_cpp_attribute(no_unique_address)
#define NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define NO_UNIQUE_ADDRESS
#endif

// SimpleHashSet
//
// As the name implies this class is an implementation of a simple set based on
// hashing. Compared to std::unordered_set it has these differences/limitations:
//
// * There must be one special value that cannot be present in the set.
//   This special value is used to indicate empty positions in the internal
//   table. Because of this, these special values should be cheap to construct.
// * It's not a strict requirement, but the values in the set should also be
//   cheap to destruct, preferably even trivially destructible.
// * When inserting more elements in this set, the already present elements may
//   move around in memory (e.g. because the internal table has run out of
//   capacity). In other words elements in this set do not have stable
//   addresses. (Because elemnts can move around, they should preferably be
//   cheap to move (same as with std::vector)).
// * This set does not offer a fully STL-compatible interface. Though where
//   possible it does come close. For example it's not possible to iterate over
//   all elements in this set.
//
// The advantage of this class over std::unordered_set are (when the above
// limitations are fine):
// * SimpleHashSet performs better than std::unordered_set
// * SimpleHashSet uses less memory than std::unordered_set
// I verified this for simple types like 'uint32_t'.
//
// Note: some of the above limitation could be lifted, even without sacrificing
// performance (e.g. create a conforming STL interface). Though I intentionally
// kept this first implementation simple.

template<typename Value, Value InvalidValue, typename Hasher, typename Equality>
class SimpleHashSet
{
public:
	SimpleHashSet(Hasher hasher_ = {}, Equality equality_ = {})
		: hasher(hasher_), equality(equality_)
	{
	}

	SimpleHashSet(const SimpleHashSet&) = delete;
	SimpleHashSet(SimpleHashSet&&) = delete;
	SimpleHashSet& operator=(const SimpleHashSet&) = delete;
	SimpleHashSet& operator=(SimpleHashSet&&) = delete;

	~SimpleHashSet()
	{
		destructTable();
	}

	void reserve(size_t n)
	{
		if (n <= capacity()) return;
		grow(Math::ceil2(2 * n));
		assert(capacity() >= n);
	}
	[[nodiscard]] size_t capacity() const
	{
		return (mask + 1) / 2;
	}

	[[nodiscard]] size_t size() const
	{
		return num_elems;
	}
	[[nodiscard]] bool empty() const
	{
		return size() == 0;
	}

	template<typename Value2>
	bool insert(Value2&& val)
	{
		auto h = hash(val) & mask;
		if (table) {
			while (table[h] != InvalidValue) {
				if (equal(table[h], val)) {
					return false; // already present
				}
				h = (h + 1) & mask;
			}
		}

		if (size() < capacity()) {
			table[h] = std::forward<Value2>(val);
		} else {
			growAndInsert(std::forward<Value2>(val));
		}
		++num_elems;
		return true;
	}

	template<typename Value2>
	bool erase(const Value2& val)
	{
		auto pos1 = locate(val);
		if (pos1 == size_t(-1)) return false; // was not present

		// Element found, now plug the hole created at position 'pos1'.
		auto mustMove = [&](size_t p1, size_t p2, size_t p3) {
			p2 = (p2 - p1 - 1) & mask;
			p3 = (p3 - p1 - 1) & mask;
			return p2 < p3;
		};
		auto pos2 = pos1;
		while (true) {
			pos2 = (pos2 + 1) & mask;
			if (table[pos2] == InvalidValue) {
				// Found a free position. Remove 'pos1' and done.
				table[pos1] = InvalidValue;
				--num_elems;
				return true;
			}
			// The element at position 'pos2' actually wants to be
			// at position 'pos3'.
			auto pos3 = hash(table[pos2] & mask);
			if (mustMove(pos1, pos2, pos3)) {
				// If the desired position is (cyclically)
				// before or at the hole, then move the element
				// and continue with a new hole.
				table[pos1] = std::move(table[pos2]);
				pos1 = pos2;
			}
		}
	}

	template<typename Value2>
	[[nodiscard]] Value* find(const Value2& val) const
	{
		auto h = locate(val);
		return (h == size_t(-1)) ? nullptr : &table[h];
	}

	template<typename Value2>
	[[nodiscard]] bool contains(const Value2& val) const
	{
		return locate(val) != size_t(-1);
	}

private:
	template<typename Value2>
	[[nodiscard]] size_t hash(const Value2& val) const
	{
		return hasher(val);
	}

	template<typename Value2>
	[[nodiscard]] bool equal(const Value& val, const Value2& val2) const
	{
		return equality(val, val2);
	}

	template<typename Value2>
	[[nodiscard]] size_t locate(const Value2& val) const
	{
		if (table) {
			auto h = hash(val) & mask;
			while (table[h] != InvalidValue) {
				if (equal(table[h], val)) {
					return h;
				}
				h = (h + 1) & mask;
			}
		}
		return size_t(-1);
	}

	// precondition: 'val' is not yet present and there is enough capacity
	template<typename Value2>
	void insertValue(Value2&& val, Value* tab, size_t msk)
	{
		auto h = hash(val) & msk;
		while (tab[h] != InvalidValue) {
			h = (h + 1) & msk;
		}
		tab[h] = std::forward<Value2>(val);
	}

	void grow(size_t newSize)
	{
		assert(Math::ispow2(newSize));
		assert(newSize > (mask + 1));

		auto* newTable = static_cast<Value*>(malloc(newSize * sizeof(Value)));
		std::uninitialized_fill(newTable, newTable + newSize, InvalidValue);

		size_t newMask = newSize - 1;
		if (table) {
			for (size_t i = 0; i <= mask; ++i) {
				if (table[i] != InvalidValue) {
					insertValue(std::move(table[i]), newTable, newMask);
				}
			}
			destructTable();
		}

		table = newTable;
		mask = static_cast<decltype(mask)>(newMask);
	}

	template<typename Value2>
	void growAndInsert(Value2&& val)
	{
		grow(std::max<size_t>(4, 2 * (mask + 1)));
		insertValue(std::forward<Value2>(val), table, mask);
	}

	void destructTable()
	{
		if (!std::is_trivially_destructible_v<Value> && table) {
			for (size_t i = 0; i <= mask; ++i) {
				table[i].~Value();
			}
		}
		free(table);
	}

private:
	NO_UNIQUE_ADDRESS Hasher hasher;
	NO_UNIQUE_ADDRESS Equality equality;
	Value* table = nullptr;
	uint32_t mask = 0; // always one less than a power-of-2 (0, 1, 3, 7, 15, ...)
	uint32_t num_elems = 0;
};

#endif
