// $Id$

#ifndef SHA1_HH
#define SHA1_HH

#include "openmsx.hh"
#include <string>

namespace openmsx {

class SHA1
{
public:
	SHA1();

	/** Update the hash value.
	  */
	void update(const byte* data, unsigned len);

	/** Get the final hash as a pre-formatted string.
	  * After this method is called, calls to update() are invalid.
	  */
	const std::string& hex_digest();

private:
	void transform(const byte buffer[64]);
	void finalize();

	std::string digest;
	uint64 m_count;
	uint32 m_state[5];
	byte m_buffer[64];
};

} // namespace openmsx

#endif
