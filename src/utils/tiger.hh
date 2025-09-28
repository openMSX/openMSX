/**
 * The implementation of the tiger() function is based on the corresponding
 * function in the tigertree project:
 *    https://sourceforge.net/projects/tigertree/
 * Which in turn states that the code is almost a literal copy from the
 * original Tiger authors code. It can be found at:
 *    http://www.cs.technion.ac.il/~biham/Reports/Tiger
 *
 * The functions tiger_int() and tiger_leaf() are implemented by me. These are
 * built on top of the (internal) tiger_compress() function. These 2 functions
 * are faster than the generic tiger() function for the specific case of
 * tiger-tree calculations.
 */

#ifndef TIGER_HH
#define TIGER_HH

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace openmsx {

/** This struct represents the result of a tiger-hash.
 * This is a 192-bit value. It can be formatted as a 39-char base32-encoded
 * string. Similar to the output of the unix 'tthsum' tool.
 */
struct TigerHash
{
	[[nodiscard]] std::string toString() const;
	union {
		std::array<uint64_t, 3> h64;
		std::array<uint8_t, 24> h8;
	};
};

/** Generic function to calculate a tiger-hash.
 * input: start-address and size of the block
 * output: 192-bit hash value
 * The result is endianness-neutral (on big-endian systems the result is
 * converted so that, when interpreted as a byte-array, it is identical to
 * the result obtained on a little-endian system).
 */
void tiger(std::span<const uint8_t> input, TigerHash& result);

/** Use for tiger-tree internal node hash calculations.
 * Combine two earlier calculated tiger hash values in a specific way (add
 * marker/padding/length bytes before/after) and calculate a new hash value.
 * This function is not reentrant.
 */
void tiger_int(const TigerHash& h0, const TigerHash& h1, TigerHash& result);

/** Use for tiger-tree leaf node hash calculations.
 * Take a 1+1024-byte input block, add some marker/padding/length bytes
 * before/after and calculate a tiger-hash.
 * This function is not reentrant.
 * This function requires that data[0] can be (temporarily) overridden (so
 * after the function returns the data buffer is unchanged, but temporarily
 * it is changed, hence the parameter cannot be const).
 */
void tiger_leaf(std::span</*const*/uint8_t> data, TigerHash& result);

} // namespace openmsx

#endif
