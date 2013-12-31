#include "StringMap.hh"
#include "StringOp.hh"
#include "xxhash.hh"

StringMapImpl::StringMapImpl(unsigned itemSize_, unsigned initSize)
	: itemSize(itemSize_)
{
	if (initSize) {
		// If a size is specified, initialize the table with that many buckets.
		init(initSize);
	} else {
		// Otherwise, initialize it with zero buckets to avoid the allocation.
		theTable = nullptr;
		numBuckets = 0;
		numItems = 0;
		numTombstones = 0;
	}
}

void StringMapImpl::init(unsigned initSize)
{
	assert(((initSize & (initSize - 1)) == 0) &&
	       "Init Size must be a power of 2 or zero!");
	numBuckets = initSize;
	numItems = 0;
	numTombstones = 0;

	theTable = static_cast<StringMapEntryBase**>(calloc(
		numBuckets + 1,
		sizeof(StringMapEntryBase**) + sizeof(unsigned)));
	if (unlikely(!theTable)) {
		throw std::bad_alloc();
	}

	// Allocate one extra bucket, set it to look filled so the iterators
	// stop at end.
	theTable[numBuckets] = reinterpret_cast<StringMapEntryBase*>(2);
}

void StringMapImpl::rehashTable()
{
	// If the hash table is now more than 3/4 full, or if fewer than 1/8 of
	// the buckets are empty (meaning that many are filled with tombstones),
	// grow/rehash the table.
	unsigned newSize;
	if ((numItems * 4) > (numBuckets * 3)) {
		newSize = numBuckets * 2; // double size
	} else if (numBuckets - (numItems + numTombstones) < (numBuckets / 8)) {
		newSize = numBuckets; // same size, only clear tombstones
	} else {
		return;
	}

	// Allocate one extra bucket (see init()).
	auto newTableArray = static_cast<StringMapEntryBase**>(
		calloc(newSize + 1,
		       sizeof(StringMapEntryBase*) + sizeof(unsigned)));
	if (unlikely(!newTableArray)) {
		throw std::bad_alloc();
	}
	newTableArray[newSize] = reinterpret_cast<StringMapEntryBase*>(2);
	auto newHashArray = reinterpret_cast<unsigned*>(newTableArray + newSize + 1);

	// Rehash all the items into their new buckets. Luckily we already have
	// the hash values available, so we don't have to rehash any strings.
	unsigned* hashTable = getHashTable();
	for (unsigned i = 0; i != numBuckets; ++i) {
		StringMapEntryBase* bucket = theTable[i];
		if (bucket && (bucket != getTombstoneVal())) {
			unsigned fullHash = hashTable[i];
			unsigned newBucket = fullHash & (newSize - 1);
			if (!newTableArray[newBucket]) {
				// Fast case, bucket available.
				newTableArray[newBucket] = bucket;
				newHashArray [newBucket] = fullHash;
			} else {
				// Otherwise probe for a spot (quadratic).
				unsigned probeSize = 1;
				do {
					newBucket = (newBucket + probeSize++) & (newSize - 1);
				} while (newTableArray[newBucket]);
				newTableArray[newBucket] = bucket;
				newHashArray[newBucket] = fullHash;
			}
		}
	}

	free(theTable);
	theTable = newTableArray;
	numBuckets = newSize;
	numTombstones = 0;
}


template<bool CASE_SENSITIVE>
static inline uint32_t hash(string_ref key)
{
	return (CASE_SENSITIVE) ? xxhash(key) : xxhash_case(key);
}

template<bool CASE_SENSITIVE>
static inline bool equal(string_ref x, string_ref y)
{
	if (CASE_SENSITIVE) {
		return x == y;
	} else {
		StringOp::casecmp cmp;
		return cmp(x, y);
	}
}

template<bool CASE_SENSITIVE>
StringMapImpl2<CASE_SENSITIVE>::StringMapImpl2(unsigned itemSize, unsigned initSize)
	: StringMapImpl(itemSize, initSize)
{
}

template<bool CASE_SENSITIVE>
unsigned StringMapImpl2<CASE_SENSITIVE>::lookupBucketFor(string_ref name)
{
	if (numBuckets == 0) { // Hash table unallocated so far?
		init(16);
	}
	unsigned fullHashValue = hash<CASE_SENSITIVE>(name);
	unsigned bucketNo = fullHashValue & (numBuckets - 1);
	unsigned* hashTable = getHashTable();

	unsigned probeAmt = 1;
	int firstTombstone = -1;
	while (true) {
		StringMapEntryBase* bucketItem = theTable[bucketNo];
		if (!bucketItem) {
			// Empty bucket, this means the key isn't in the table
			// yet. If we found a tombstone earlier, then reuse
			// that instead of using this empty bucket.
			if (firstTombstone != -1) {
				hashTable[firstTombstone] = fullHashValue;
				return firstTombstone;
			}
			hashTable[bucketNo] = fullHashValue;
			return bucketNo;
		} else if (bucketItem == getTombstoneVal()) {
			// Skip tombstones, but remember the first one we see.
			if (firstTombstone == -1) firstTombstone = bucketNo;
		} else if (hashTable[bucketNo] == fullHashValue) {
			// If the full hash value matches, check deeply for a
			// match.  The common case here is that we are only
			// looking at the buckets (for item info being non-nullptr
			// and for the full hash value) not at the items.  This
			// is important for cache locality.
			auto itemStr = reinterpret_cast<char*>(bucketItem) + itemSize;
			if (equal<CASE_SENSITIVE>(
			      name, string_ref(itemStr, bucketItem->getKeyLength()))) {
				return bucketNo;
			}
		}

		// Use quadratic probing, it has fewer clumping artifacts than linear
		// probing and has good cache behavior in the common case.
		bucketNo = (bucketNo + probeAmt) & (numBuckets - 1);
		++probeAmt;
	}
}

template<bool CASE_SENSITIVE>
int StringMapImpl2<CASE_SENSITIVE>::findKey(string_ref key) const
{
	if (numBuckets == 0) return -1;

	unsigned fullHashValue = hash<CASE_SENSITIVE>(key);
	unsigned bucketNo = fullHashValue & (numBuckets - 1);
	unsigned* hashTable = getHashTable();

	unsigned probeAmt = 1;
	while (true) {
		StringMapEntryBase* bucketItem = theTable[bucketNo];
		if (!bucketItem) {
			// Empty bucket, key isn't in the table yet.
			return -1;
		} else if (bucketItem == getTombstoneVal()) {
			// Ignore tombstones.
		} else if (hashTable[bucketNo] == fullHashValue) {
			// Hash matches, compare full string.
			auto itemStr = reinterpret_cast<char*>(bucketItem) + itemSize;
			if (equal<CASE_SENSITIVE>(
			      key, string_ref(itemStr, bucketItem->getKeyLength()))) {
				return bucketNo;
			}
		}
		// Quadratic probing.
		bucketNo = (bucketNo + probeAmt) & (numBuckets - 1);
		++probeAmt;
	}
}

template<bool CASE_SENSITIVE>
void StringMapImpl2<CASE_SENSITIVE>::removeKey(StringMapEntryBase* v)
{
	auto vStr = reinterpret_cast<char*>(v) + itemSize;
	StringMapEntryBase* v2 = removeKey(string_ref(vStr, v->getKeyLength()));
	assert(v == v2 && "Didn't find key?"); (void)v2;
}

template<bool CASE_SENSITIVE>
StringMapEntryBase* StringMapImpl2<CASE_SENSITIVE>::removeKey(string_ref key)
{
	int bucket = findKey(key);
	if (bucket == -1) return nullptr;

	StringMapEntryBase* result = theTable[bucket];
	theTable[bucket] = getTombstoneVal();
	--numItems;
	++numTombstones;
	assert(numItems + numTombstones <= numBuckets);
	return result;
}

template class StringMapImpl2<true>;
template class StringMapImpl2<false>;
