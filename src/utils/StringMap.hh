#ifndef STRINGMAP_HH
#define STRINGMAP_HH

// This is based on the StringMap class in LLVM:
//   http://llvm.org/docs/ProgrammersManual.html#dss_stringmap
// See also that page for a more detailed descrption.
//
// Very briefly: the class StringMap<T> offers a hashtable-based map. The keys
// are strings (or any byte-array with variable length) and the value can be
// any type. Both keys and values are stored in (=owned by) the map.
//
// The interface of the StringMap<T> class is very close to the STL
// std::map<std::string, T> class. Though one notable difference is the use of
// 'first'. An example:
//
//     typedef std::map<std::string, int> MySTLmap;
//     MySTLmap m1 = ...;
//     MySTLmap::const_iterator it1 = m1.find("abc"); // *1*
//     assert(it1->first == "abc");
//     int i1 = it1->second;
//
//     typedef StringMap<int> MyHashmap;
//     MyHashmap m2 = ...;
//     MyHashmap::const_iterator it = m2.find("abc"); // *2
//     assert(it2->first() == "abc");
//     int i2 = it2->second;
//
// So basically you only have to replace 'first' with 'first()'. Note that on
// the line marked with *1* a temporary std::string object needs to be
// constructed and destructed. The corresponding line *2* has no such overhead.


#include "string_ref.hh"
#include <cassert>
#include <cstring> // memcpy
#include <cstdlib> // malloc, free

template<typename T> class StringMapConstIterator;
template<typename T> class StringMapIterator;

// Non-templatized base class of StringMapEntry<T>.
class StringMapEntryBase
{
public:
	explicit StringMapEntryBase(unsigned len) : strLen(len) {}
	unsigned getKeyLength() const { return strLen; }
private:
	unsigned strLen;
};


/// Non-templatized base class of StringMap<T>.
class StringMapImpl
{
public:
	static StringMapEntryBase* getTombstoneVal() {
		return reinterpret_cast<StringMapEntryBase*>(-1);
	}

	unsigned getNumBuckets() const { return numBuckets; }
	bool empty() const { return numItems == 0; }
	unsigned size() const { return numItems; }

protected:
	explicit StringMapImpl(unsigned itemSize, unsigned initSize);

	// Look up the bucket that the specified string should end up in. If it
	// already exists as a key in the map, the Item pointer for the
	// specified bucket will be non-null. Otherwise, it will be null.  In
	// either case, the FullHashValue field of the bucket will be set to
	// the hash value of the string.
	unsigned lookupBucketFor(string_ref key);

	// Look up the bucket that contains the specified key. If it exists in
	// the map, return the bucket number of the key. Otherwise return -1.
	// This does not modify the map.
	int findKey(string_ref key) const;

	// Remove the specified StringMapEntry from the table, but do not
	// delete it. This aborts if the value isn't in the table.
	void removeKey(StringMapEntryBase *V);

	// Remove the StringMapEntry for the specified key from the table,
	// returning it. If the key is not in the table, this returns null.
	StringMapEntryBase* removeKey(string_ref key);

	// Grow the table, redistributing values into the buckets with the
	// appropriate mod-of-hashtable-size.
	void rehashTable();

private:
	void init(unsigned Size);
	unsigned* getHashTable() const {
		return reinterpret_cast<unsigned*>(theTable + numBuckets + 1);
	}

protected:
	// Array of numBuckets pointers to entries, NULL-ptrs are holes.
	// theTable[numBuckets] contains a sentinel value for easy iteration.
	// Followed by an array of the actual hash values as unsigned integers.
	StringMapEntryBase** theTable;
	unsigned numBuckets;
	unsigned numItems;
	unsigned numTombstones;

private:
	const unsigned itemSize;
};


// This is used to represent one value that is inserted into a StringMap.
// It contains the value itself and the key (string-length + string-data).
template<typename T> class StringMapEntry : public StringMapEntryBase
{
public:
	T second;

public:
	// Create a StringMapEntry for the specified key and value.
	static StringMapEntry* create(string_ref key, const T& v)
	{
		// Allocate memory.
		StringMapEntry* newItem = static_cast<StringMapEntry*>(
			malloc(sizeof(StringMapEntry) + key.size()));

		// Construct the value (using placement new).
		new (newItem) StringMapEntry(key.size(), v);

		// Copy the string data.
		char* strBuffer = const_cast<char*>(newItem->getKeyData());
		memcpy(strBuffer, key.data(), key.size());

		return newItem;
	}

	// Destroy this StringMapEntry, releasing memory.
	void destroy() {
		this->~StringMapEntry();
		free(this);
	}

	string_ref getKey() const {
		return string_ref(getKeyData(), getKeyLength());
	}
	string_ref first() const { return getKey(); }

	const T& getValue() const { return second; }
	      T& getValue()       { return second; }
	void setValue(const T& v) { second = v; }

	// Return the start of the string data that is the key for this value.
	const char* getKeyData() const {
		// The string data is stored immediately after this object.
		return reinterpret_cast<const char*>(this + 1);
	}

	// Given a value that is known to be embedded into a StringMapEntry,
	// return the StringMapEntry itself.
	static StringMapEntry& GetStringMapEntryFromValue(T& v) {
		StringMapEntry* ePtr = 0;
		char* ptr = reinterpret_cast<char*>(&v) -
			(reinterpret_cast<char*>(&ePtr->second) -
			 reinterpret_cast<char*>(ePtr));
		return *reinterpret_cast<StringMapEntry*>(ptr);
	}
	static const StringMapEntry& GetStringMapEntryFromValue(const T &v) {
		return GetStringMapEntryFromValue(const_cast<T&>(v));
	}

	// Given key data that is known to be embedded into a StringMapEntry,
	// return the StringMapEntry itself.
	static StringMapEntry& GetStringMapEntryFromKeyData(const char* keyData) {
		char* ptr = const_cast<char*>(keyData) - sizeof(StringMapEntry<T>);
		return *reinterpret_cast<StringMapEntry*>(ptr);
	}

private:
	StringMapEntry(unsigned strLen, const T& v)
		: StringMapEntryBase(strLen), second(v) {}

	~StringMapEntry() {}
};


// This is an unconventional map that is specialized for handling keys that are
// "strings", which are basically ranges of bytes. This does some funky memory
// allocation and hashing things to make it extremely efficient, storing the
// string data *after* the value in the map.
template<typename T> class StringMap : public StringMapImpl
{
public:
	typedef const char* key_type;
	typedef T mapped_type;
	typedef StringMapEntry<T> value_type;
	typedef size_t size_type;
	typedef StringMapConstIterator<T> const_iterator;
	typedef StringMapIterator<T>      iterator;

	explicit StringMap(unsigned initialSize = 0)
		: StringMapImpl(sizeof(value_type), initialSize) {}

	~StringMap() {
		clear();
		free(theTable);
	}

	iterator begin() {
		return       iterator(theTable, numBuckets != 0);
	}
	const_iterator begin() const {
		return const_iterator(theTable, numBuckets != 0);
	}
	iterator end() {
		return       iterator(theTable + numBuckets);
	}
	const_iterator end() const {
		return const_iterator(theTable + numBuckets);
	}

	iterator find(string_ref key) {
		int bucket = findKey(key);
		if (bucket == -1) return end();
		return iterator(theTable + bucket);
	}
	const_iterator find(string_ref key) const {
		int bucket = findKey(key);
		if (bucket == -1) return end();
		return const_iterator(theTable + bucket);
	}

	// Return the entry for the specified key, or a default constructed
	// value if no such entry exists.
	T lookup(string_ref key) const {
		const_iterator it = find(key);
		if (it != end()) return it->second;
		return T();
	}

	T& operator[](string_ref key) {
		return getOrCreateValue(key).second;
	}

	size_type count(string_ref key) const {
		return (find(key) == end()) ? 0 : 1;
	}

	// Insert the specified key/value pair into the map. If the key already
	// exists in the map, return false and ignore the request, otherwise
	// insert it and return true.
	bool insert(value_type* keyValue)
	{
		unsigned bucketNo = lookupBucketFor(keyValue->getKey());
		StringMapEntryBase*& bucket = theTable[bucketNo];
		if (bucket && (bucket != getTombstoneVal())) {
			return false; // Already exists in map.
		}
		if (bucket == getTombstoneVal()) {
			--numTombstones;
		}
		bucket = keyValue;
		++numItems;
		assert(numItems + numTombstones <= numBuckets);

		rehashTable();
		return true;
	}

	// Empties out the StringMap
	void clear()
	{
		if (empty()) return;

		// Zap all values, resetting the keys back to non-present (not
		// tombstone), which is safe because we're removing all
		// elements.
		for (unsigned i = 0; i != numBuckets; ++i) {
			StringMapEntryBase*& bucket = theTable[i];
			if (bucket && (bucket != getTombstoneVal())) {
				static_cast<value_type*>(bucket)->destroy();
			}
			bucket = NULL;
		}

		numItems = 0;
		numTombstones = 0;
	}

	// Look up the specified key in the table. If a value exists, return
	// it. Otherwise, default construct a value, insert it, and return.
	value_type& getOrCreateValue(string_ref key, const T& val = T())
	{
		unsigned bucketNo = lookupBucketFor(key);
		StringMapEntryBase*& bucket = theTable[bucketNo];
		if (bucket && (bucket != getTombstoneVal())) {
			return *static_cast<value_type*>(bucket);
		}

		value_type* newItem = value_type::create(key, val);

		if (bucket == getTombstoneVal()) --numTombstones;
		++numItems;
		assert(numItems + numTombstones <= numBuckets);

		// Fill in the bucket for the hash table. The FullHashValue was already
		// filled in by lookupBucketFor().
		bucket = newItem;

		rehashTable();
		return *newItem;
	}

	// Remove the specified key/value pair from the map, but do not destroy
	// it. This aborts if the key is not in the map.
	void remove(value_type* keyValue) {
		removeKey(keyValue);
	}

	void erase(iterator i) {
		value_type& v = *i;
		remove(&v);
		v.destroy();
	}

	bool erase(string_ref key) {
		iterator i = find(key);
		if (i == end()) return false;
		erase(i);
		return true;
	}

private:
	// disable copy and assign
	StringMap(const StringMap&);
	StringMap& operator=(const StringMap&);
};


template<typename T> class StringMapConstIterator
{
public:
	typedef StringMapEntry<T> value_type;

	explicit StringMapConstIterator(
			StringMapEntryBase** bucket, bool advance = false)
		: ptr(bucket)
	{
		if (advance) advancePastEmptyBuckets();
	}

	const value_type& operator*() const {
		return *static_cast<value_type*>(*ptr);
	}
	const value_type* operator->() const {
		return static_cast<value_type*>(*ptr);
	}

	bool operator==(const StringMapConstIterator& rhs) const {
		return ptr == rhs.ptr;
	}
	bool operator!=(const StringMapConstIterator& rhs) const {
		return ptr != rhs.ptr;
	}

	inline StringMapConstIterator& operator++() { // preincrement
		++ptr;
		advancePastEmptyBuckets();
		return *this;
	}
	StringMapConstIterator operator++(int) { // postincrement
		StringMapConstIterator tmp = *this;
		++*this;
		return tmp;
	}

protected:
	StringMapEntryBase** ptr;

private:
	void advancePastEmptyBuckets() {
		while ((*ptr == 0) || (*ptr == StringMapImpl::getTombstoneVal())) {
			++ptr;
		}
	}
};

template<typename T> class StringMapIterator : public StringMapConstIterator<T>
{
public:
	typedef StringMapEntry<T> value_type;

	explicit StringMapIterator(
			StringMapEntryBase** bucket, bool advance = false)
		: StringMapConstIterator<T>(bucket, advance)
	{
	}

	value_type& operator*() const {
		return *static_cast<value_type*>(*this->ptr);
	}
	value_type* operator->() const {
		return static_cast<value_type*>(*this->ptr);
	}
};

#endif
