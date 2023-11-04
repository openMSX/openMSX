// hash_set
//
// Written by Wouter Vermaelen, based upon HashSet/HashMap
// example code and ideas provided by Jacques Van Damme.

#ifndef HASH_SET_HH
#define HASH_SET_HH

#include "narrow.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <new>
#include <type_traits>
#include <utility>

namespace hash_set_impl {

struct PoolIndex {
	unsigned idx;
	[[nodiscard]] constexpr bool operator==(const PoolIndex&) const = default;
};
static constexpr PoolIndex invalidIndex{unsigned(-1)};

// Holds the data that will be stored in the hash_set plus some extra
// administrative data.
// - value: the actual to be stored data
// - hash: always equal to hasher(extract(value))
// - nextIdx: index of next element in a single-linked-list. This is either
//     the list of colliding buckets in the hash, or the list of free objects
//     in the Pool (see below).
template<typename Value>
struct Element {
	Value    value;
	unsigned hash;
	PoolIndex nextIdx;

	template<typename V>
	constexpr Element(V&& value_, unsigned hash_, PoolIndex nextIdx_)
		: value(std::forward<V>(value_))
		, hash(hash_)
		, nextIdx(nextIdx_)
	{
	}

	template<typename... Args>
	explicit constexpr Element(Args&&... args)
		: value(std::forward<Args>(args)...)
		// hash    left uninitialized
		// nextIdx left uninitialized
	{
	}
};


// Holds 'Element' objects. These objects can be in 2 states:
// - 'free': In that case 'nextIdx' forms a single-linked list of all
//     free objects. 'freeIdx_' is the head of this list. Index 'invalidIndex'
//     indicates the end of the list.
// - 'in-use': The Element object is in use by some other data structure,
//     the Pool class doesn't interpret any of the Element fields.
template<typename Value>
class Pool {
	using Elem = Element<Value>;

public:
	Pool() = default;

	Pool(Pool&& source) noexcept
		: buf_     (source.buf_)
		, freeIdx_ (source.freeIdx_)
		, capacity_(source.capacity_)
	{
		source.buf_ = nullptr;
		source.freeIdx_ = invalidIndex;
		source.capacity_ = 0;
	}

	Pool& operator=(Pool&& source) noexcept
	{
		buf_      = source.buf_;
		freeIdx_  = source.freeIdx_;
		capacity_ = source.capacity_;
		source.buf_ = nullptr;
		source.freeIdx_ = invalidIndex;
		source.capacity_ = 0;
		return *this;
	}

	~Pool()
	{
		free(buf_);
	}

	// Lookup Element by index. (External code) is only allowed to call this
	// method with indices that were earlier returned from create() and have
	// not yet been passed to destroy().
	[[nodiscard]] Elem& get(PoolIndex idx)
	{
		assert(idx.idx < capacity_);
		return buf_[idx.idx];
	}
	[[nodiscard]] const Elem& get(PoolIndex idx) const
	{
		return const_cast<Pool&>(*this).get(idx);
	}

	// - Insert a new Element in the pool (will be created with the given
	//   Element constructor parameters).
	// - Returns an index that can be used with the get() method. Never
	//   returns 'invalidIndex' (so it can be used as a 'special' value).
	// - Internally Pool keeps a list of pre-allocated objects, when this
	//   list runs out Pool will automatically allocate more objects.
	template<typename V>
	[[nodiscard]] PoolIndex create(V&& value, unsigned hash, PoolIndex nextIdx)
	{
		if (freeIdx_ == invalidIndex) grow();
		auto idx = freeIdx_;
		auto& elem = get(idx);
		freeIdx_ = elem.nextIdx;
		new (&elem) Elem(std::forward<V>(value), hash, nextIdx);
		return idx;
	}

	// Destroys a previously created object (given by index). Index must
	// have been returned by an earlier create() call, and the same index
	// can only be destroyed once. It's not allowed to destroy 'invalidIndex'.
	void destroy(PoolIndex idx)
	{
		auto& elem = get(idx);
		elem.~Elem();
		elem.nextIdx = freeIdx_;
		freeIdx_ = idx;
	}

	// Leaves 'hash' and 'nextIdx' members uninitialized!
	template<typename... Args>
	[[nodiscard]] PoolIndex emplace(Args&&... args)
	{
		if (freeIdx_ == invalidIndex) grow();
		auto idx = freeIdx_;
		auto& elem = get(idx);
		freeIdx_ = elem.nextIdx;
		new (&elem) Elem(std::forward<Args>(args)...);
		return idx;
	}

	[[nodiscard]] unsigned capacity() const
	{
		return capacity_;
	}

	void reserve(unsigned count)
	{
		// note: not required to be a power-of-2
		if (capacity_ >= count) return;
		if (capacity_ != 0) {
			growMore(count);
		} else {
			growInitial(count);
		}
	}

	friend void swap(Pool& x, Pool& y) noexcept
	{
		using std::swap;
		swap(x.buf_,     y.buf_);
		swap(x.freeIdx_,  y.freeIdx_);
		swap(x.capacity_, y.capacity_);
	}

private:
	void grow()
	{
		if (capacity_ != 0) {
			growMore(2 * capacity_);
		} else {
			growInitial(4); // initial capacity = 4
		}
	}

	void growMore(unsigned newCapacity)
	{
		auto* oldBuf = buf_;
		Elem* newBuf;
		if constexpr (std::is_trivially_move_constructible_v<Elem> &&
		              std::is_trivially_copyable_v<Elem>) {
			newBuf = static_cast<Elem*>(realloc(oldBuf, newCapacity * sizeof(Elem)));
			if (!newBuf) throw std::bad_alloc();
		} else {
			newBuf = static_cast<Elem*>(malloc(newCapacity * sizeof(Elem)));
			if (!newBuf) throw std::bad_alloc();

			for (size_t i : xrange(capacity_)) {
				new (&newBuf[i]) Elem(std::move(oldBuf[i]));
				oldBuf[i].~Elem();
			}
			free(oldBuf);
		}

		for (auto i : xrange(capacity_, newCapacity - 1)) {
			newBuf[i].nextIdx = PoolIndex{i + 1};
		}
		newBuf[newCapacity - 1].nextIdx = invalidIndex;

		buf_ = newBuf;
		freeIdx_ = PoolIndex{capacity_};
		capacity_ = newCapacity;
	}

	void growInitial(unsigned newCapacity)
	{
		auto* newBuf = static_cast<Elem*>(malloc(newCapacity * sizeof(Elem)));
		if (!newBuf) throw std::bad_alloc();

		for (auto i : xrange(newCapacity - 1)) {
			newBuf[i].nextIdx = PoolIndex{i + 1};
		}
		newBuf[newCapacity - 1].nextIdx = invalidIndex;

		buf_ = newBuf;
		freeIdx_ = PoolIndex{0};
		capacity_ = newCapacity;
	}

private:
	Elem* buf_ = nullptr;
	PoolIndex freeIdx_ = invalidIndex; // index of a free block, 'invalidIndex' means nothing is free
	unsigned capacity_ = 0;
};

// Type-alias for the resulting type of applying Extractor on Value.
template<typename Value, typename Extractor>
using ExtractedType = typename std::remove_cvref_t<
	decltype(std::declval<Extractor>()(std::declval<Value>()))>;

} // namespace hash_set_impl


// hash_set
//
// A hash-set implementation with an STL-like interface.
//
// One important property of this implementation is that stored values are not
// the same as the keys that are used for hashing the values or looking them
// up.
//
// By default (when not specifying a 2nd template argument) the values are the
// same as the keys (so you get a classic hash-set). but it's possible to
// specify an extractor functor that extracts the key-part from the value (e.g.
// extract one member variable out of a bigger struct).
//
// If required it's also possible to specify custom hash and equality functors.
template<typename Value,
         typename Extractor = std::identity,
         typename Hasher = std::hash<hash_set_impl::ExtractedType<Value, Extractor>>,
         typename Equal = std::equal_to<>>
class hash_set
{
protected:
	using PoolIndex = hash_set_impl::PoolIndex;
	static constexpr auto invalidIndex = hash_set_impl::invalidIndex;

public:
	using value_type = Value;

	template<typename HashSet, typename IValue>
	class Iter {
	public:
		using value_type = IValue;
		using difference_type = int;
		using pointer = IValue*;
		using reference = IValue&;
		using iterator_category = std::forward_iterator_tag;

		Iter() = default;

		template<typename HashSet2, typename IValue2>
		explicit Iter(const Iter<HashSet2, IValue2>& other)
			: hashSet(other.hashSet), elemIdx(other.elemIdx) {}

		template<typename HashSet2, typename IValue2>
		Iter& operator=(const Iter<HashSet2, IValue2>& rhs) {
			hashSet = rhs.hashSet;
			elemIdx = rhs.elemIdx;
			return *this;
		}

		[[nodiscard]] bool operator==(const Iter& rhs) const {
			assert((hashSet == rhs.hashSet) || !hashSet || !rhs.hashSet);
			return elemIdx == rhs.elemIdx;
		}

		Iter& operator++() {
			auto& oldElem = hashSet->pool.get(elemIdx);
			elemIdx = oldElem.nextIdx;
			if (elemIdx == invalidIndex) {
				unsigned tableIdx = oldElem.hash & hashSet->allocMask;
				do {
					if (tableIdx == hashSet->allocMask) break;
					elemIdx = hashSet->table[++tableIdx];
				} while (elemIdx == invalidIndex);
			}
			return *this;
		}
		Iter operator++(int) {
			Iter tmp = *this;
			++(*this);
			return tmp;
		}

		[[nodiscard]] IValue& operator*() const {
			return hashSet->pool.get(elemIdx).value;
		}
		[[nodiscard]] IValue* operator->() const {
			return &hashSet->pool.get(elemIdx).value;
		}

	//internal:   should only be called from hash_set and hash_map
		Iter(HashSet* m, PoolIndex idx)
			: hashSet(m), elemIdx(idx) {}

		[[nodiscard]] PoolIndex getElementIdx() const {
			return elemIdx;
		}

	private:
		HashSet* hashSet = nullptr;
		PoolIndex elemIdx = invalidIndex;
	};

	using       iterator = Iter<      hash_set,       Value>;
	using const_iterator = Iter<const hash_set, const Value>;

public:
	explicit hash_set(unsigned initialSize = 0,
	         Extractor extract_ = Extractor(),
	         Hasher hasher_ = Hasher(),
	         Equal equal_ = Equal())
		: extract(extract_), hasher(hasher_), equal(equal_)
	{
		reserve(initialSize); // optimized away if initialSize==0
	}

	hash_set(const hash_set& source)
		: extract(source.extract), hasher(source.hasher)
		, equal(source.equal)
	{
		if (source.elemCount == 0) return;
		reserve(source.elemCount);

		for (unsigned i = 0; i <= source.allocMask; ++i) {
			for (auto idx = source.table[i]; idx != invalidIndex; /**/) {
				const auto& elem = source.pool.get(idx);
				insert_noCapacityCheck_noDuplicateCheck(elem.value);
				idx = elem.nextIdx;
			}
		}
	}

	hash_set(hash_set&& source) noexcept
		: table(source.table)
		, pool(std::move(source.pool))
		, allocMask(source.allocMask)
		, elemCount(source.elemCount)
		, extract(std::move(source.extract))
		, hasher (std::move(source.hasher))
		, equal  (std::move(source.equal))
	{
		source.table = nullptr;
		source.allocMask = -1;
		source.elemCount = 0;
	}

	explicit hash_set(std::initializer_list<Value> args)
	{
		reserve(narrow<unsigned>(args.size()));
		for (auto a : args) insert_noCapacityCheck(a); // need duplicate check??
	}

	~hash_set()
	{
		clear();
		free(table);
	}

	hash_set& operator=(const hash_set& source)
	{
		if (&source == this) return *this;
		clear();
		if (source.elemCount == 0) return *this;
		reserve(source.elemCount);

		for (unsigned i = 0; i <= source.allocMask; ++i) {
			for (auto idx = source.table[i]; idx != invalidIndex; /**/) {
				const auto& elem = source.pool.get(idx);
				insert_noCapacityCheck_noDuplicateCheck(elem.value);
				idx = elem.nextIdx;
			}
		}
		extract = source.extract;
		hasher  = source.hasher;
		equal   = source.equal;
		return *this;
	}

	hash_set& operator=(hash_set&& source) noexcept
	{
		table     = source.table;
		pool      = std::move(source.pool);
		allocMask = source.allocMask;
		elemCount = source.elemCount;
		extract   = std::move(source.extract);
		hasher    = std::move(source.hasher);
		equal     = std::move(source.equal);

		source.table = nullptr;
		source.allocMask = -1;
		source.elemCount = 0;
		return *this;
	}

	template<typename K>
	[[nodiscard]] bool contains(const K& key) const
	{
		return locateElement(key) != invalidIndex;
	}

	template<typename V>
	std::pair<iterator, bool> insert(V&& value)
	{
		return insert_impl<true, true>(std::forward<V>(value));
	}
	template<typename V>
	std::pair<iterator, bool> insert_noCapacityCheck(V&& value)
	{
		return insert_impl<false, true>(std::forward<V>(value));
	}
	template<typename V>
	iterator insert_noDuplicateCheck(V&& value)
	{
		return insert_impl<true, false>(std::forward<V>(value)).first;
	}
	template<typename V>
	iterator insert_noCapacityCheck_noDuplicateCheck(V&& value)
	{
		return insert_impl<false, false>(std::forward<V>(value)).first;
	}

	template<typename... Args>
	std::pair<iterator, bool> emplace(Args&&... args)
	{
		return emplace_impl<true, true>(std::forward<Args>(args)...);
	}
	template<typename... Args>
	std::pair<iterator, bool> emplace_noCapacityCheck(Args&&... args)
	{
		return emplace_impl<false, true>(std::forward<Args>(args)...);
	}
	template<typename... Args>
	iterator emplace_noDuplicateCheck(Args&&... args)
	{
		return emplace_impl<true, false>(std::forward<Args>(args)...).first;
	}
	template<typename... Args>
	iterator emplace_noCapacityCheck_noDuplicateCheck(Args&&... args)
	{
		return emplace_impl<false, false>(std::forward<Args>(args)...).first;
	}

	template<typename K>
	bool erase(const K& key)
	{
		if (elemCount == 0) return false;

		auto hash = unsigned(hasher(key));
		auto tableIdx = hash & allocMask;

		for (auto* prev = &table[tableIdx]; *prev != invalidIndex; prev = &(pool.get(*prev).nextIdx)) {
			auto elemIdx = *prev;
			auto& elem = pool.get(elemIdx);
			if (elem.hash != hash) continue;
			if (!equal(extract(elem.value), key)) continue;

			*prev = elem.nextIdx;
			pool.destroy(elemIdx);
			--elemCount;
			return true;
		}
		return false;
	}

	void erase(iterator it)
	{
		auto elemIdx = it.getElementIdx();
		if (elemIdx == invalidIndex) {
			UNREACHABLE; // not allowed to call erase(end())
			return;
		}
		auto& elem = pool.get(elemIdx);
		auto tableIdx = pool.get(elemIdx).hash & allocMask;
		auto* prev = &table[tableIdx];
		assert(*prev != invalidIndex);
		while (*prev != elemIdx) {
			prev = &(pool.get(*prev).nextIdx);
			assert(*prev != invalidIndex);
		}
		*prev = elem.nextIdx;
		pool.destroy(elemIdx);
		--elemCount;
	}

	[[nodiscard]] bool empty() const
	{
		return elemCount == 0;
	}

	[[nodiscard]] unsigned size() const
	{
		return elemCount;
	}

	void clear()
	{
		if (elemCount == 0) return;

		for (unsigned i = 0; i <= allocMask; ++i) {
			for (auto elemIdx = table[i]; elemIdx != invalidIndex; /**/) {
				auto nextIdx = pool.get(elemIdx).nextIdx;
				pool.destroy(elemIdx);
				elemIdx = nextIdx;
			}
			table[i] = invalidIndex;
		}
		elemCount = 0;
	}

	template<typename K>
	[[nodiscard]] iterator find(const K& key)
	{
		return iterator(this, locateElement(key));
	}

	template<typename K>
	[[nodiscard]] const_iterator find(const K& key) const
	{
		return const_iterator(this, locateElement(key));
	}

	[[nodiscard]] iterator begin()
	{
		if (elemCount == 0) return end();

		for (unsigned idx = 0; idx <= allocMask; ++idx) {
			if (table[idx] != invalidIndex) {
				return iterator(this, table[idx]);
			}
		}
		UNREACHABLE;
		return end(); // avoid warning
	}

	[[nodiscard]] const_iterator begin() const
	{
		if (elemCount == 0) return end();

		for (unsigned idx = 0; idx <= allocMask; ++idx) {
			if (table[idx] != invalidIndex) {
				return const_iterator(this, table[idx]);
			}
		}
		UNREACHABLE;
		return end(); // avoid warning
	}

	[[nodiscard]] iterator end()
	{
		return iterator();
	}

	[[nodiscard]] const_iterator end() const
	{
		return const_iterator();
	}

	[[nodiscard]] unsigned capacity() const
	{
		unsigned poolCapacity = pool.capacity();
		unsigned tableCapacity = (allocMask + 1) / 4 * 3;
		return std::min(poolCapacity, tableCapacity);
	}

	// After this call, the hash_set can at least contain 'count' number of
	// elements without requiring any additional memory allocation or rehash.
	void reserve(unsigned count)
	{
		pool.reserve(count);

		unsigned oldCount = allocMask + 1;
		unsigned newCount = nextPowerOf2((count + 2) / 3 * 4);
		if (oldCount >= newCount) return;

		allocMask = newCount - 1;
		if (oldCount == 0) {
			table = static_cast<PoolIndex*>(malloc(newCount * sizeof(PoolIndex)));
			std::fill(table, table + newCount, invalidIndex);
		} else {
			table = static_cast<PoolIndex*>(realloc(table, newCount * sizeof(PoolIndex)));
			do {
				rehash(oldCount);
				oldCount *= 2;
			} while (oldCount < newCount);
		}
	}

	friend void swap(hash_set& x, hash_set& y) noexcept
	{
		using std::swap;
		swap(x.table,     y.table);
		swap(x.pool,      y.pool);
		swap(x.allocMask, y.allocMask);
		swap(x.elemCount, y.elemCount);
		swap(x.extract  , y.extract);
		swap(x.hasher   , y.hasher);
		swap(x.equal    , y.equal);
	}

	[[nodiscard]] friend auto begin(      hash_set& s) { return s.begin(); }
	[[nodiscard]] friend auto begin(const hash_set& s) { return s.begin(); }
	[[nodiscard]] friend auto end  (      hash_set& s) { return s.end();   }
	[[nodiscard]] friend auto end  (const hash_set& s) { return s.end();   }

protected:
	// Returns the smallest value that is >= x that is also a power of 2.
	// (for x=0 it returns 0)
	[[nodiscard]] static inline unsigned nextPowerOf2(unsigned x)
	{
		static_assert(sizeof(unsigned) == sizeof(uint32_t), "only works for exactly 32 bit");
		x -= 1;
		x |= x >>  1;
		x |= x >>  2;
		x |= x >>  4;
		x |= x >>  8;
		x |= x >> 16;
		return x + 1;
	}

	template<bool CHECK_CAPACITY, bool CHECK_DUPLICATE, typename V>
	[[nodiscard]] std::pair<iterator, bool> insert_impl(V&& value)
	{
		auto hash = unsigned(hasher(extract(value)));
		auto tableIdx = hash & allocMask;
		PoolIndex primary = invalidIndex;

		if (!CHECK_CAPACITY || (elemCount > 0)) {
			primary = table[tableIdx];
			if constexpr (CHECK_DUPLICATE) {
				for (auto elemIdx = primary; elemIdx != invalidIndex; /**/) {
					auto& elem = pool.get(elemIdx);
					if ((elem.hash == hash) &&
					    equal(extract(elem.value), extract(value))) {
						// already exists
						return std::pair(iterator(this, elemIdx), false);
					}
					elemIdx = elem.nextIdx;
				}
			}
		}

		if (CHECK_CAPACITY && (elemCount >= ((allocMask + 1) / 4 * 3))) {
			grow();
			tableIdx = hash & allocMask;
			primary = table[tableIdx];
		}

		elemCount++;
		auto idx = pool.create(std::forward<V>(value), hash, primary);
		table[tableIdx] = idx;
		return std::pair(iterator(this, idx), true);
	}

	template<bool CHECK_CAPACITY, bool CHECK_DUPLICATE, typename... Args>
	[[nodiscard]] std::pair<iterator, bool> emplace_impl(Args&&... args)
	{
		auto poolIdx = pool.emplace(std::forward<Args>(args)...);
		auto& poolElem = pool.get(poolIdx);

		auto hash = unsigned(hasher(extract(poolElem.value)));
		auto tableIdx = hash & allocMask;
		PoolIndex primary = invalidIndex;

		if (!CHECK_CAPACITY || (elemCount > 0)) {
			primary = table[tableIdx];
			if constexpr (CHECK_DUPLICATE) {
				for (auto elemIdx = primary; elemIdx != invalidIndex; /**/) {
					auto& elem = pool.get(elemIdx);
					if ((elem.hash == hash) &&
					    equal(extract(elem.value), extract(poolElem.value))) {
						// already exists
						pool.destroy(poolIdx);
						return std::pair(iterator(this, elemIdx), false);
					}
					elemIdx = elem.nextIdx;
				}
			}
		}

		if (CHECK_CAPACITY && (elemCount >= ((allocMask + 1) / 4 * 3))) {
			grow();
			tableIdx = hash & allocMask;
			primary = table[tableIdx];
		}

		elemCount++;
		poolElem.hash = hash;
		poolElem.nextIdx = primary;
		table[tableIdx] = poolIdx;
		return std::pair(iterator(this, poolIdx), true);
	}

	void grow()
	{
		unsigned oldCount = allocMask + 1;
		if (oldCount == 0) {
			allocMask = 4 - 1; // initial size
			table = static_cast<PoolIndex*>(malloc(4 * sizeof(PoolIndex)));
			std::fill(table, table + 4, invalidIndex);
		} else {
			unsigned newCount = 2 * oldCount;
			allocMask = newCount - 1;
			table = static_cast<PoolIndex*>(realloc(table, newCount * sizeof(unsigned)));
			rehash(oldCount);
		}
	}

	void rehash(unsigned oldCount)
	{
		assert((oldCount & (oldCount - 1)) == 0); // must be a power-of-2
		for (auto i : xrange(oldCount)) {
			auto* p0 = &table[i];
			auto* p1 = &table[i + oldCount];
			for (auto p = *p0; p != invalidIndex; p = pool.get(p).nextIdx) {
				auto& elem = pool.get(p);
				if ((elem.hash & oldCount) == 0) {
					*p0 = p;
					p0 = &elem.nextIdx;
				} else {
					*p1 = p;
					p1 = &elem.nextIdx;
				}
			}
			*p0 = invalidIndex;
			*p1 = invalidIndex;
		}
	}

	template<typename K>
	[[nodiscard]] PoolIndex locateElement(const K& key) const
	{
		if (elemCount == 0) return invalidIndex;

		auto hash = unsigned(hasher(key));
		auto tableIdx = hash & allocMask;
		for (auto elemIdx = table[tableIdx]; elemIdx != invalidIndex; /**/) {
			auto& elem = pool.get(elemIdx);
			if ((elem.hash == hash) &&
			    equal(extract(elem.value), key)) {
				return elemIdx;
			}
			elemIdx = elem.nextIdx;
		}
		return invalidIndex;
	}

protected:
	PoolIndex* table = nullptr;
	hash_set_impl::Pool<Value> pool;
	unsigned allocMask = unsigned(-1);
	unsigned elemCount = 0;
	[[no_unique_address]] Extractor extract;
	[[no_unique_address]] Hasher hasher;
	[[no_unique_address]] Equal equal;
};

#endif
