#ifndef SHA1_HH
#define SHA1_HH

#include "xrange.hh"

#include <array>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

namespace openmsx {

/** This class represents the result of a sha1 calculation (a 160-bit value).
  * Objects of this class can be constructed from/converted to 40-digit hex
  * string.
  * We treat the value '000...00' (all zeros) special. This value can be used
  * to indicate a null-sha1sum value (e.g. sha1 not yet calculated, or not
  * meaningful). In theory it's possible this special value is the result of an
  * actual sha1 calculation, but this has an _extremely_ low probability.
  */
class Sha1Sum
{
public:
	struct UninitializedTag {};
	explicit Sha1Sum(UninitializedTag) {}

	// note: default copy and assign are ok
	Sha1Sum();
	/** Construct from string, throws when string is malformed. */
	explicit Sha1Sum(std::string_view hex);

	/** Parse from a 40-character long buffer.
	 * @pre 'str' points to a buffer of at least 40 characters
	 * @throws MSXException if chars are not 0-9, a-f, A-F
	 */
	void parse40(std::span<const char, 40> str);
	[[nodiscard]] std::string toString() const;

	// Test or set 'null' value.
	[[nodiscard]] bool empty() const;
	void clear();

	// gcc-10.2 miscompiles this (fixed in gcc-11),
	//  so still manually implement operator==.
	//[[nodiscard]] constexpr bool operator==(const Sha1Sum&) const = default;
	[[nodiscard]] bool operator==(const Sha1Sum& other) const {
		for (int i : xrange(5)) {
			if (a[i] != other.a[i]) return false;
		}
		return true;
	}
	[[nodiscard]] constexpr auto operator<=>(const Sha1Sum& other) const {
		for (int i : xrange(5 - 1)) {
			if (auto cmp = a[i] <=> other.a[i]; cmp != 0) return cmp;
		}
		return a[5 - 1] <=> other.a[5 - 1];
	}

	friend std::ostream& operator<<(std::ostream& os, const Sha1Sum& sum) {
		os << sum.toString();
		return os;
	}

private:
	std::array<uint32_t, 5> a;
	friend class SHA1;
};


/** Helper class to perform a sha1 calculation.
  * Basic usage:
  *  - construct a SHA1 object
  *  - repeatedly call update()
  *  - call digest() to get the result
  * Alternatively, use calc() if all data can be passed at once (IOW when there
  * would be exactly one call to update() in the recipe above).
  */
class SHA1
{
public:
	SHA1();

	/** Incrementally calculate the hash value. */
	void update(std::span<const uint8_t> data);

	/** Get the final hash. After this method is called, calls to update()
	  * are invalid. */
	[[nodiscard]] Sha1Sum digest();

	/** Easier to use interface, if you can pass all data in one go. */
	[[nodiscard]] static Sha1Sum calc(std::span<const uint8_t> data);

private:
	void transform(std::span<const uint8_t, 64> buffer);
	void finalize();

private:
	uint64_t m_count = 0; // in bytes (sha1 reference implementation counts in bits)
	Sha1Sum m_state;
	std::array<uint8_t, 64> m_buffer;
	bool m_finalized = false;
};

} // namespace openmsx

#endif
