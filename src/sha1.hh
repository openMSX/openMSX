// $Id$

#ifndef __SHA1_HH__
#define __SHA1_HH__

#include "openmsx.hh"
#include <string>

namespace openmsx {

class SHA1
{
public:
	SHA1();
	~SHA1();

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
	
	uint32 m_state[5];
	uint64 m_count;
	byte m_buffer[64];

	std::string digest;
};

} // namespace openmsx

#endif
