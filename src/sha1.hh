// $Id$

#ifndef __SHA1_HH__
#define __SHA1_HH__

#include "openmsx.hh"
#include <string>

using std::string;

namespace openmsx {

class SHA1
{
public:
	SHA1();
	~SHA1();

	/** Reset this SHA1 object to compute a new hash.
	  */
	void reset();

	/** Update the hash value.
	  */
	void update(const byte* data, unsigned int len);

	/** Finalize the hash computation.
	  */
	void finalize();

	/** Get the final hash as a pre-formatted string.
	  */
	string hex_digest();

private:
	/** SHA-1 transformation.
	  */
	void transform(const byte buffer[64]);
	
	uint32 m_state[5];
	uint64 m_count;
	byte m_buffer[64];
	byte m_digest[20];

};

} // namespace openmsx

#endif
