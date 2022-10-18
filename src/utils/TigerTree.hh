/** Incremental Tiger-tree-hash (tth) calculation.
 *
 * This code is based on code from the tigertree project
 *    https://sourceforge.net/projects/tigertree/
 *
 * The main difference is that the original code can only perform a one-shot
 * tth calculation while this code (also) allows incremental tth calculations.
 * When the input data changes, incremental calculation allows to only redo
 * part of the full calculation, so it can be much faster.
 *
 * Here's the copyright notice of the original code (even though the code is
 * almost completely rewritten):
 *
 *     Copyright (C) 2001 Bitzi (aka Bitcollider) Inc. and Gordon Mohr
 *     Released into the public domain by same; permission is explicitly
 *     granted to copy, modify, and use freely.
 *
 *     THE WORK IS PROVIDED "AS IS," AND COMES WITH ABSOLUTELY NO WARRANTY,
 *     EXPRESS OR IMPLIED, TO THE EXTENT PERMITTED BY APPLICABLE LAW,
 *     INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *     OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 *     (PD) 2001 The Bitzi Corporation
 *     Please see file COPYING or http://bitzi.com/publicdomain
 *     for more info.
 */

#ifndef TIGERTREE_HH
#define TIGERTREE_HH

#include <string>
#include <cstdint>
#include <ctime>
#include <functional>

namespace openmsx {

struct TigerHash;

/** The TigerTree class will query the to-be-hashed data via this abstract
  * interface. This allows to e.g. fetch the data from a file.
  */
class TTData
{
public:
	/** Return the requested portion of the to-be-hashed data block.
	 * Special requirement: it should be allowed to temporarily overwrite
	 * the byte one position before the returned pointer.
	 */
	[[nodiscard]] virtual uint8_t* getData(size_t offset, size_t size) = 0;

	/** Because TTH calculation of a large file takes some time (a few
	  * 1/10s for a harddisk image) we try to cache previous calculations.
	  * This method makes sure we don't wrongly reuse the data. E.g. after
	  * it has been modified (by openmsx or even externally).
	  *
	  * Note that the current implementation of the caching is only
	  * suited for files. Refactor this if we ever need some different.
	  */
	[[nodiscard]] virtual bool isCacheStillValid(time_t& time) = 0;

protected:
	~TTData() = default;
};

struct TTCacheEntry;

/** Calculate a tiger-tree-hash.
 * Calculation can be done incrementally, so recalculating the hash after a
 * (small) modification of the input is efficient.
 */
class TigerTree
{
public:
	static constexpr size_t BLOCK_SIZE = 1024;

	/** Create TigerTree calculator for the given (abstract) data block
	 * of given size.
	 */
	TigerTree(TTData& data, size_t dataSize, const std::string& name);

	/** Calculate the hash value.
	 */
	[[nodiscard]] const TigerHash& calcHash(const std::function<void(size_t, size_t)>& progressCallback);

	/** Inform this calculator about changes in the input data. This is
	 * used to (not) skip re-calculations on future calcHash() calls. So
	 * it's crucial this calculator is informed about  _all_ changes in
	 * the input.
	 */
	void notifyChange(size_t offset, size_t len, time_t time);

private:
	// functions to navigate in binary tree
	struct Node {
		Node(size_t n_, size_t l_) : n(n_), l(l_) {}
		size_t n; // node number
		size_t l; // level number
	};
	[[nodiscard]] Node getTop() const;
	[[nodiscard]] Node getLeaf(size_t block) const;
	[[nodiscard]] Node getParent(Node node) const;
	[[nodiscard]] Node getLeftChild(Node node) const;
	[[nodiscard]] Node getRightChild(Node node) const;

	[[nodiscard]] const TigerHash& calcHash(Node node, const std::function<void(size_t, size_t)>& progressCallback);

private:
	TTData& data;
	const size_t dataSize;
	TTCacheEntry& entry;
};

} // namespace openmsx

#endif
