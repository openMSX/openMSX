// $Id$

/**
 * 100% free public domain implementation of the SHA-1
 * algorithm by Dominik Reichl <Dominik.Reichl@tiscali.de>
 * 
 * === Test Vectors (from FIPS PUB 180-1) ===
 *
 * "abc"
 * A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
 *
 * "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
 * 84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
 *
 * A million repetitions of "a"
 * 34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
 */

#ifndef __SHA1_HH__
#define __SHA1_HH__

#include <string>

using namespace std;

class SHA1
{
	public:
		// Constructor and Destructor
		SHA1();
		~SHA1();

		void reset();

		// Update the hash value
		void update(const unsigned char* data, unsigned int len);

		// Finalize hash and report
		void finalize();
		string hex_digest();

	private:
		// Private SHA-1 transformation
		void transform(unsigned long state[5],
		               const unsigned char buffer[64]);
		
		unsigned long m_state[5];
		unsigned long m_count[2];
		unsigned char m_buffer[64];
		unsigned char m_digest[20];

		typedef union {
			unsigned char c[64];
			unsigned long l[16];
		} SHA1_WORKSPACE_BLOCK;
};

#endif
