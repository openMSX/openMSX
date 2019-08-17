// hash_set
//
// Written by Wouter Vermaelen, based upon HashSet/HashMap
// example code and ideas provided by Jacques Van Damme.

#ifndef HASH_SET_HH
#define HASH_SET_HH

#include "stl.hh"
#include "unreachable.hh"
#include <cassert>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <new>
#include <type_traits>
#include <utility>

namespace hash_set_impl {

// Identity operation: accepts any type (by const or non-const reference) and
// returns the exact same (reference) value.
struct Identity {
	template<typename T>
	inline T& operator()(T&& t) const { return t; }
};

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
	unsigned nextIdx;

	template<typename V>
	Element(V&& value_, unsigned hash_, unsigned nextIdx_)
		: value(std::forward<V>(value_))
		, hash(hash_)
		, nextIdx(nextIdx_)
	{
	}

	template<typename... Args>
	explicit Element(Args&&... args)
		: value(std::forward<Args>(args)...)
		// hash    left uninitialized
		// nextIdx left uninitialized
	{
	}
};


// Holds 'Element' objects. These objects can be in 2 states:
// - 'free': In that case 'nextIdx' forms a single-linked list of all
//     free objects. 'freeIdx_' is the head of this list. Index 0
//     indicates the end of the list. This also means that valid
//     indices start at 1 instead of 0.
// - 'in-use': The Element object is in use by some other data structure,
//     the Pool class doesn't interpret any of the Element fields.
template<typename Value>
class Pool {
	using Elem = Element<Value>;

public:
	Pool() = default;

	Pool(Pool&& source) noexcept
		: buf1_    (source.buf1_)
		, freeIdx_ (source.freeIdx_)
		, capacity_(source.capacity_)
	{
		source.buf1_ = adjust(nullptr);
		source.freeIdx_ = 0;
		source.capacity_ = 0;
	}

	Pool& operator=(Pool&& source) noexcept
	{
		buf1_     = source.buf1_;
		freeIdx_  = source.freeIdx_;
		capacity_ = source.capacity_;
		source.buf1_ = adjust(nullptr);
		source.freeIdx_ = 0;
		source.capacity_ = 0;
		return *this;
	}

	~Pool()
	{
		free(buf1_ + 1);
	}

	// Lookup Element by index. Valid indices start at 1 instead of 0!
	// (External code) is only allowed to call this method with indices
	// that were earlier returned from create() and have not yet been
	// passed to destroy().
	Elem& get(unsigned idx)
	{
		assert(idx != 0);
		assert(idx <= capacity_);
		return buf1_[idx];
	}
	const Elem& get(unsigned idx) const
	{
		return const_cast<Pool&>(*this).get(idx);
	}

	// - Insert a new Element in the pool (will be created with the given
	//   Element constructor parameters).
	// - Returns an index that can be used with the get() method. Never
	//   returns 0 (so 0 can be used as a 'special' value).
	// - Internally Pool keeps a list of pre-allocated objects, when this
	//   list runs out Pool will automatically allocate more objects.
	template<typename V>
	unsigned create(V&& value, unsigned hash, unsigned nextIdx)
	{
		if (freeIdx_ == 0) grow();
		unsigned idx = freeIdx_;
		auto& elem = get(idx);
		freeIdx_ = elem.nextIdx;
		new (&elem) Elem(std::forward<V>(value), hash, nextIdx);
		return idx;
	}

	// Destroys a previously created object (given by index). Index must
	// have been returned by an earlier create() call, and the same index
	// can only be destroyed once. It's not allowed to destroy index 0.
	void destroy(unsigned idx)
	{
		auto& elem = get(idx);
		elem.~Elem();
		elem.nextIdx = freeIdx_;
		freeIdx_ = idx;
	}

	// Leaves 'hash' and 'nextIdx' members uninitialized!
	template<typename... Args>
	unsigned emplace(Args&&... args)
	{
		if (freeIdx_ == 0) grow();
		unsigned idx = freeIdx_;
		auto& elem = get(idx);
		freeIdx_ = elem.nextIdx;
		new (&elem) Elem(std::forward<Args>(args)...);
		return idx;
	}

	unsigned capacity() const
	{
		return capacity_;
	}

	void reserve(unsigned count)
	{
		// note: not required to be a power-of-2
		if (capacity_ >= count) return;
		if (capacity_ != 0) growMore(count);
		else             growInitial(count);
	}

	friend void swap(Pool& x, Pool& y) noexcept
	{
		using std::swap;
		swap(x.buf1_,     y.buf1_);
		swap(x.freeIdx_,  y.freeIdx_);
		swap(x.capacity_, y.capacity_);
	}

private:
	static inline Elem* adjust(Elem* p) { return p - 1; }

	void grow()
	{
		if (capacity_ != 0) growMore(2 * capacity_);
		else             growInitial(4); // initial capacity = 4
	}

	void growMore(unsigned newCapacity)
	{
		auto* oldBuf = buf1_ + 1;
		Elem* newBuf;
		if constexpr (std::is_trivially_move_constructible_v<Elem> &&
		              std::is_trivially_copyable_v<Elem>) {
			newBuf = static_cast<Elem*>(realloc(oldBuf, newCapacity * sizeof(Elem)));
			if (!newBuf) throw std::bad_alloc();
		} else {
			newBuf = static_cast<Elem*>(malloc(newCapacity * sizeof(Elem)));
			if (!newBuf) throw std::bad_alloc();

			for (size_t i = 0; i < capacity_; ++i) {
				new (&newBuf[i]) Elem(std::move(oldBuf[i]));
				oldBuf[i].~Elem();
			}
			free(oldBuf);
		}

		for (unsigned i = capacity_; i < newCapacity - 1; ++i) {
			newBuf[i].nextIdx = i + 1 + 1;
		}
		newBuf[newCapacity - 1].nextIdx = 0;

		buf1_ = adjust(newBuf);
		freeIdx_ = capacity_ + 1;
		capacity_ = newCapacity;
	}

	void growInitial(unsigned newCapacity)
	{
		auto* newBuf = static_cast<Elem*>(malloc(newCapacity * sizeof(Elem)));
		if (!newBuf) throw std::bad_alloc();

		for (unsigned i = 0; i < newCapacity - 1; ++i) {
			newBuf[i].nextIdx = i + 1 + 1;
		}
		newBuf[newCapacity - 1].nextIdx = 0;

		buf1_ = adjust(newBuf);
		freeIdx_ = 1;
		capacity_ = newCapacity;
	}

private:
	Elem* buf1_ = adjust(nullptr); // 1 before start of buffer -> valid indices start at 1
	unsigned freeIdx_ = 0; // index of a free block, 0 means nothing is free
	unsigned capacity_ = 0;
};

// Type-alias for the resulting type of applying Extractor on Value.
template<typename Value, typename Extractor>
using ExtractedType =
	typename std::remove_cv<
		typename std::remove_reference<
			decltype(std::declval<Extractor>()(std::declval<Value>()))>
		::type>
	::type;

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
         typename Extractor = hash_set_impl::Identity,
	 typename Hasher = std::hash<hash_set_impl::ExtractedType<Value, Extractor>>,
	 typename Equal = EqualTo>
class hash_set
{
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

		bool operator==(const Iter& rhs) const {
			assert((hashSet == rhs.hashSet) || !hashSet || !rhs.hashSet);
			return elemIdx == rhs.elemIdx;
		}
		bool operator!=(const Iter& rhs) const {
			return !(*this == rhs);
		}

		Iter& operator++() {
			auto& oldElem = hashSet->pool.get(elemIdx);
			elemIdx = oldElem.nextIdx;
			if (!elemIdx) {
				unsigned tableIdx = oldElem.hash & hashSet->allocMask;
				do {
					if (tableIdx == hashSet->allocMask) break;
					elemIdx = hashSet->table[++tableIdx];
				} while (!elemIdx);
			}
			return *this;
		}
		Iter operator++(int) {
			Iter tmp = *this;
			++(*this);
			return tmp;
		}

		IValue& operator*() const {
			return hashSet->pool.get(elemIdx).value;
		}
		IValue* operator->() const {
			return &hashSet->pool.get(elemIdx).value;
		}

	private:
		friend class hash_set;

		Iter(HashSet* m, unsigned idx)
			: hashSet(m), elemIdx(idx) {}

		unsigned getElementIdx() const {
			return elemIdx;
		}

	private:
		HashSet* hashSet = nullptr;
		unsigned elemIdx = 0;
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
			for (auto idx = source.table[i]; idx; /**/) {
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
		reserve(args.size());
		for (auto a : args) insert_noCapacityCheck(a); // need duplicate check??
	}

	~hash_set()
	{
		clear();
		free(table);
	}

	hash_set& operator=(const hash_set& source)
	{
		clear();
		if (source.elemCount == 0) return *this;
		reserve(source.elemCount);

		for (unsigned i = 0; i <= source.allocMask; ++i) {
			for (auto idx = source.table[i]; idx; /**/) {
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
	bool contains(const K& key) const
	{
		return locateElement(key) != 0;
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

		unsigned hash = hasher(key);
		unsigned tableIdx = hash & allocMask;

		for (auto* prev = &table[tableIdx]; *prev; prev = &(pool.get(*prev).nextIdx)) {
			unsigned elemIdx = *prev;
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
		if (!elemIdx) {
			UNREACHABLE; // not allowed to call erase(end())
			return;
		}
		auto& elem = pool.get(elemIdx);
		unsigned tableIdx = pool.get(elemIdx).hash & allocMask;
		auto* prev = &table[tableIdx];
		assert(*prev);
		while (*prev != elemIdx) {
			prev = &(pool.get(*prev).nextIdx);
			assert(*prev);
		}
		*prev = elem.nextIdx;
		pool.destroy(elemIdx);
		--elemCount;
	}

	bool empty() const
	{
		return elemCount == 0;
	}

	unsigned size() const
	{
		return elemCount;
	}

	void clear()
	{
		if (elemCount == 0) return;

		for (unsigned i = 0; i <= allocMask; ++i) {
			for (auto elemIdx = table[i]; elemIdx; /**/) {
				auto nextIdx = pool.get(elemIdx).nextIdx;
				pool.destroy(elemIdx);
				elemIdx = nextIdx;
			}
			table[i] = 0;
		}
		elemCount = 0;
	}

	template<typename K>
	iterator find(const K& key)
	{
		return iterator(this, locateElement(key));
	}

	template<typename K>
	const_iterator find(const K& key) const
	{
		return const_iterator(this, locateElement(key));
	}

	iterator begin()
	{
		if (elemCount == 0) return end();

		for (unsigned idx = 0; idx <= allocMask; ++idx) {
			if (table[idx]) {
				return iterator(this, table[idx]);
			}
		}
		UNREACHABLE;
		return end(); // avoid warning
	}

	const_iterator begin() const
	{
		if (elemCount == 0) return end();

		for (unsigned idx = 0; idx <= allocMask; ++idx) {
			if (table[idx]) {
				return const_iterator(this, table[idx]);
			}
		}
		UNREACHABLE;
		return end(); // avoid warning
	}

	iterator end()
	{
		return iterator();
	}

	const_iterator end() const
	{
		return const_iterator();
	}

	unsigned capacity() const
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
			table = static_cast<unsigned*>(calloc(newCount, sizeof(unsigned)));
		} else {
			table = static_cast<unsigned*>(realloc(table, newCount * sizeof(unsigned)));
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

	friend auto begin(      hash_set& s) { return s.begin(); }
	friend auto begin(const hash_set& s) { return s.begin(); }
	friend auto end  (      hash_set& s) { return s.end();   }
	friend auto end  (const hash_set& s) { return s.end();   }

private:
	// Returns the smallest value that is >= x that is also a power of 2.
	// (for x=0 it returns 0)
	static inline unsigned nextPowerOf2(unsigned x)
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
	std::pair<iterator, bool> insert_impl(V&& value)
	{
		unsigned hash = hasher(extract(value));
		unsigned tableIdx = hash & allocMask;
		unsigned primary = 0;

		if (!CHECK_CAPACITY || (elemCount > 0)) {
			primary = table[tableIdx];
			if (CHECK_DUPLICATE) {
				for (auto elemIdx = primary; elemIdx; /**/) {
					auto& elem = pool.get(elemIdx);
					if ((elem.hash == hash) &&
					    equal(extract(elem.value), extract(value))) {
						// already exists
						return std::make_pair(iterator(this, elemIdx), false);
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
		unsigned idx = pool.create(std::forward<V>(value), hash, primary);
		table[tableIdx] = idx;
		return std::make_pair(iterator(this, idx), true);
	}

	template<bool CHECK_CAPACITY, bool CHECK_DUPLICATE, typename... Args>
	std::pair<iterator, bool> emplace_impl(Args&&... args)
	{
		unsigned poolIdx = pool.emplace(std::forward<Args>(args)...);
		auto& poolElem = pool.get(poolIdx);

		auto hash = unsigned(hasher(extract(poolElem.value)));
		unsigned tableIdx = hash & allocMask;
		unsigned primary = 0;

		if (!CHECK_CAPACITY || (elemCount > 0)) {
			primary = table[tableIdx];
			if (CHECK_DUPLICATE) {
				for (auto elemIdx = primary; elemIdx; ) {
					auto& elem = pool.get(elemIdx);
					if ((elem.hash == hash) &&
					    equal(extract(elem.value), extract(poolElem.value))) {
						// already exists
						pool.destroy(poolIdx);
						return std::make_pair(iterator(this, elemIdx), false);
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
		return std::make_pair(iterator(this, poolIdx), true);
	}

	void grow()
	{
		unsigned oldCount = allocMask + 1;
		if (oldCount == 0) {
			allocMask = 4 - 1; // initial size
			table = static_cast<unsigned*>(calloc(4, sizeof(unsigned)));
		} else {
			unsigned newCount = 2 * oldCount;
			allocMask = newCount - 1;
			table = static_cast<unsigned*>(realloc(table, newCount * sizeof(unsigned)));
			rehash(oldCount);
		}
	}

	void rehash(unsigned oldCount)
	{
		assert((oldCount & (oldCount - 1)) == 0); // must be a power-of-2
		for (unsigned i = 0; i < oldCount; i++) {
			auto* p0 = &table[i];
			auto* p1 = &table[i + oldCount];
			for (auto p = *p0; p; p = pool.get(p).nextIdx) {
				auto& elem = pool.get(p);
				if ((elem.hash & oldCount) == 0) {
					*p0 = p;
					p0 = &elem.nextIdx;
				} else {
					*p1 = p;
					p1 = &elem.nextIdx;
				}
			}
			*p0 = 0;
			*p1 = 0;
		}
	}

	template<typename K>
	unsigned locateElement(const K& key) const
	{
		if (elemCount == 0) return 0;

		auto hash = unsigned(hasher(key));
		unsigned tableIdx = hash & allocMask;
		for (unsigned elemIdx = table[tableIdx]; elemIdx; /**/) {
			auto& elem = pool.get(elemIdx);
			if ((elem.hash == hash) &&
			    equal(extract(elem.value), key)) {
				return elemIdx;
			}
			elemIdx = elem.nextIdx;
		}
		return 0;
	}

private:
	unsigned* table = nullptr;
	hash_set_impl::Pool<Value> pool;
	unsigned allocMask = unsigned(-1);
	unsigned elemCount = 0;
	Extractor extract;
	Hasher hasher;
	Equal equal;
};

#endif
