#include "TigerTree.hh"

#include "tiger.hh"
#include "Math.hh"
#include "MemBuffer.hh"
#include "ScopedAssign.hh"

#include <cassert>
#include <map>
#include <span>

namespace openmsx {

struct TTCacheEntry
{
	struct Info {
		TigerHash hash;
		bool valid;
	};
	MemBuffer<Info> nodes;
	time_t time = -1;
	size_t numNodesValid;
};
// Typically contains 0 or 1 element, and only rarely 2 or more. But we need
// the address of existing elements to remain stable when new elements are
// inserted. So still use std::map instead of std::vector.
static std::map<std::pair<size_t, std::string>, TTCacheEntry> ttCache;

[[nodiscard]] static constexpr size_t calcNumNodes(size_t dataSize)
{
	auto numBlocks = (dataSize + TigerTree::BLOCK_SIZE - 1) / TigerTree::BLOCK_SIZE;
	return (numBlocks == 0) ? 1 : 2 * numBlocks - 1;
}

[[nodiscard]] static TTCacheEntry& getCacheEntry(
	TTData& data, size_t dataSize, const std::string& name)
{
	auto& result = ttCache[std::pair(dataSize, name)];
	if (!data.isCacheStillValid(result.time)) { // note: has side effect
		size_t numNodes = calcNumNodes(dataSize);
		result.nodes.resize(numNodes);
		for (auto& i : result.nodes) i.valid = false; // all invalid
		result.numNodesValid = 0;
	}
	return result;
}

TigerTree::TigerTree(TTData& data_, size_t dataSize_, const std::string& name)
	: data(data_)
	, dataSize(dataSize_)
	, entry(getCacheEntry(data, dataSize, name))
{
}

const TigerHash& TigerTree::calcHash(const std::function<void(size_t, size_t)>& progressCallback)
{
	return calcHash(getTop(), progressCallback);
}

void TigerTree::notifyChange(size_t offset, size_t len, time_t time)
{
	entry.time = time;

	assert((offset + len) <= dataSize);
	if (len == 0) return;

	if (entry.nodes[getTop().n].valid) {
		entry.nodes[getTop().n].valid = false; // set sentinel
		entry.numNodesValid--;
	}
	auto first = offset / BLOCK_SIZE;
	auto last = (offset + len - 1) / BLOCK_SIZE;
	assert(first <= last); // requires len != 0
	do {
		auto node = getLeaf(first);
		while (entry.nodes[node.n].valid) {
			entry.nodes[node.n].valid = false;
			entry.numNodesValid--;
			node = getParent(node);
		}
	} while (++first <= last);
}

const TigerHash& TigerTree::calcHash(Node node, const std::function<void(size_t, size_t)>& progressCallback)
{
	auto n = node.n;
	auto& nod = entry.nodes[n];
	if (!nod.valid) {
		if (n & 1) {
			// interior node
			auto left  = getLeftChild (node);
			auto right = getRightChild(node);
			const auto& h1 = calcHash(left, progressCallback);
			const auto& h2 = calcHash(right, progressCallback);
			tiger_int(h1, h2, nod.hash);
		} else {
			// leaf node
			size_t b = n * (BLOCK_SIZE / 2);
			size_t l = dataSize - b;

			if (l >= BLOCK_SIZE) {
				auto* d = data.getData(b, BLOCK_SIZE);
				tiger_leaf(std::span{d, BLOCK_SIZE}, nod.hash);
			} else {
				// partial last block
				auto* d = data.getData(b, l);
				auto sa = ScopedAssign(d[-1], uint8_t(0));
				tiger(std::span{d - 1, l + 1}, nod.hash);
			}
		}
		nod.valid = true;
		entry.numNodesValid++;
		if (progressCallback) {
			progressCallback(entry.numNodesValid, entry.nodes.size());
		}
	}
	return nod.hash;
}


// The TigerTree::nodes member variable stores a linearized binary tree. The
// linearization is done like in this example:
//
//                   7              (level = 8)
//             ----/   \----        .
//           3              11      (level = 4)
//         /   \           /  \     .
//       1       5       9     |    (level = 2)
//      / \     / \     / \    |    .
//     0   2   4   6   8  10  12    (level = 1)
//
// All leaf nodes are given even node values (0, 2, 4, .., 12). Leaf nodes have
// level=1. At the next level (level=2) leaf nodes are pair-wise combined in
// internal nodes. So nodes 0 and 2 are combined in node 1, 4+6->5 and 8+10->9.
// Leaf-node 12 cannot be paired (there is no leaf-node 14), so there's no
// corresponding node at level=2. The next level is level=4 (level values
// double for each higher level). At level=4 node 3 is the combination of 1 and
// 5 and 9+12->11. Note that 11 is a combination of two nodes from a different
// (lower) level. And finally, node 7 at level=8 combines 3+11.
//
// The following methods navigate in this tree.

TigerTree::Node TigerTree::getTop() const
{
	auto n = Math::floodRight(entry.nodes.size() / 2);
	return {n, n + 1};
}

TigerTree::Node TigerTree::getLeaf(size_t block) const
{
	assert((2 * block) < entry.nodes.size());
	return {2 * block, 1};
}

TigerTree::Node TigerTree::getParent(Node node) const
{
	assert(node.n < entry.nodes.size());
	do {
		node.n = (node.n & ~(2 * node.l)) + node.l;
		node.l *= 2;
	} while (node.n >= entry.nodes.size());
	return node;
}

TigerTree::Node TigerTree::getLeftChild(Node node) const
{
	assert(node.n < entry.nodes.size());
	assert(node.l > 1);
	node.l /= 2;
	node.n -= node.l;
	return node;
}

TigerTree::Node TigerTree::getRightChild(Node node) const
{
	assert(node.n < entry.nodes.size());
	while (true) {
		assert(node.l > 1);
		node.l /= 2;
		auto r = node.n + node.l;
		if (r < entry.nodes.size()) return {r, node.l};
	}
}

} // namespace openmsx
