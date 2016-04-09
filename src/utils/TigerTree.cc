#include "TigerTree.hh"
#include "Math.hh"
#include <map>
#include <cstring>
#include <cassert>

namespace openmsx {

static const size_t BLOCK_SIZE = 1024;

struct TTCacheEntry
{
	TTCacheEntry() : time(-1) {}

	MemBuffer<TigerHash> hash;
	MemBuffer<bool> valid;
	size_t numNodes;
	time_t time;
	size_t numNodesValid;
};
// Typically contains 0 or 1 element, and only rarely 2 or more. But we need
// the address of existing elements to remain stable when new elements are
// inserted. So still use std::map instead of std::vector.
static std::map<std::pair<size_t, std::string>, TTCacheEntry> ttCache;

static size_t calcNumNodes(size_t dataSize)
{
	auto numBlocks = (dataSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
	return (numBlocks == 0) ? 1 : 2 * numBlocks - 1;
}

static TTCacheEntry& getCacheEntry(
	TTData& data, size_t dataSize, const std::string& name)
{
	auto& result = ttCache[std::make_pair(dataSize, name)];
	if (!data.isCacheStillValid(result.time)) { // note: has side effect
		size_t numNodes = calcNumNodes(dataSize);
		result.hash .resize(numNodes);
		result.valid.resize(numNodes);
		result.numNodes = numNodes;
		memset(result.valid.data(), 0, numNodes); // all invalid
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

const TigerHash& TigerTree::calcHash(std::function<void(size_t, size_t)> progressCallback)
{
	return calcHash(getTop(), progressCallback);
}

void TigerTree::notifyChange(size_t offset, size_t len, time_t time)
{
	entry.time = time;

	assert((offset + len) <= dataSize);
	if (len == 0) return;

	if (entry.valid[getTop().n]) {
		entry.valid[getTop().n] = false; // set sentinel
		entry.numNodesValid--;
	}
	auto first = offset / BLOCK_SIZE;
	auto last = (offset + len - 1) / BLOCK_SIZE;
	assert(first <= last); // requires len != 0
	do {
		auto node = getLeaf(first);
		while (entry.valid[node.n]) {
			entry.valid[node.n] = false;
			entry.numNodesValid--;
			node = getParent(node);
		}
	} while (++first <= last);
}

const TigerHash& TigerTree::calcHash(Node node, std::function<void(size_t, size_t)> progressCallback)
{
	auto n = node.n;
	if (!entry.valid[n]) {
		if (n & 1) {
			// interior node
			auto left  = getLeftChild (node);
			auto right = getRightChild(node);
			auto& h1 = calcHash(left, progressCallback);
			auto& h2 = calcHash(right, progressCallback);
			tiger_int(h1, h2, entry.hash[n]);
		} else {
			// leaf node
			size_t b = n * (BLOCK_SIZE / 2);
			size_t l = dataSize - b;

			if (l >= BLOCK_SIZE) {
				auto* d = data.getData(b, BLOCK_SIZE);
				tiger_leaf(d, entry.hash[n]);
			} else {
				// partial last block
				auto* d = data.getData(b, l);
				auto backup = d[-1];
				d[-1] = 0;
				tiger(d - 1, l + 1, entry.hash[n]);
				d[-1] = backup;
			}
		}
		entry.valid[n] = true;
		entry.numNodesValid++;
		if (progressCallback) {
			progressCallback(entry.numNodesValid, entry.numNodes);
		}
	}
	return entry.hash[n];
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
	auto n = Math::floodRight(entry.numNodes / 2);
	return Node(n, n + 1);
}

TigerTree::Node TigerTree::getLeaf(size_t block) const
{
	assert((2 * block) < entry.numNodes);
	return Node(2 * block, 1);
}

TigerTree::Node TigerTree::getParent(Node node) const
{
	assert(node.n < entry.numNodes);
	do {
		node.n = (node.n & ~(2 * node.l)) + node.l;
		node.l *= 2;
	} while (node.n >= entry.numNodes);
	return node;
}

TigerTree::Node TigerTree::getLeftChild(Node node) const
{
	assert(node.n < entry.numNodes);
	assert(node.l > 1);
	node.l /= 2;
	node.n -= node.l;
	return node;
}

TigerTree::Node TigerTree::getRightChild(Node node) const
{
	assert(node.n < entry.numNodes);
	while (1) {
		assert(node.l > 1);
		node.l /= 2;
		auto r = node.n + node.l;
		if (r < entry.numNodes) return Node(r, node.l);
	}
}

} // namespace openmsx


#if 0

// Unittest

class TTTestData : public openmsx::TTData
{
public:
	uint8_t* getData(size_t offset, size_t size) override
	{
		return buffer + offset;
	}
	uint8_t* buffer;
};

int main()
{
	uint8_t buffer_[8192 + 1];
	uint8_t* buffer = buffer_ + 1;
	TTTestData data;
	data.buffer = buffer;

	// zero sized buffer
	openmsx::TigerTree tt0(data, 0);
	assert(tt0.calcHash().toString() ==
	       "LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ");

	// size less than one block
	openmsx::TigerTree tt1(data, 100);
	memset(buffer, 0, 100);
	assert(tt1.calcHash().toString() ==
	       "EOIEKIQO6BSNCNRX2UB2MB466INV6LICZ6MPUWQ");
	memset(buffer + 20, 1, 10);
	tt1.notifyChange(20, 10);
	assert(tt1.calcHash().toString() ==
	       "GOTZVYW3WIE37XFCDOY66PLLXWGP6DPN3CQRHWA");

	// 3 full and one partial block
	openmsx::TigerTree tt2(data, 4000);
	memset(buffer, 0, 4000);
	assert(tt2.calcHash().toString() ==
	       "YC44NFWFCN3QWFRSS6ICGUJDLH7F654RCKVT7VY");
	memset(buffer + 1500, 1, 10);
	tt2.notifyChange(1500, 10); // change a single block
	assert(tt2.calcHash().toString() ==
	       "JU5RYR446PVZSPMOJML4IQ2FXLDDKE522CEYIBA");
	memset(buffer + 2000, 1, 100);
	tt2.notifyChange(2000, 100); // change two blocks
	assert(tt2.calcHash().toString() ==
	       "IPV53CDVB2I63HXIXVK2OUPNS26YB7V7G2Y7XIA");

	// 7 full blocks (unbalanced internal binary tree)
	openmsx::TigerTree tt3(data, 7 * 1024);
	memset(buffer, 0, 7 * 1024);
	assert(tt3.calcHash().toString() ==
	       "FPSZ35773WS4WGBVXM255KWNETQZXMTEJGFMLTA");
	memset(buffer + 512, 1, 512);
	tt3.notifyChange(512, 512); // part of block-0
	assert(tt3.calcHash().toString() ==
	       "Z32BC2WSHPW5DYUSNSZGLDIFTEIP3DBFJ7MG2MQ");
	memset(buffer + 3*1024, 1, 4*1024);
	tt3.notifyChange(3*1024, 4*1024); // blocks 3-6
	assert(tt3.calcHash().toString() ==
	       "SJUYB3QVIJXNKZMSQZGIMHA7GA2MYU2UECDA26A");
}

#endif
