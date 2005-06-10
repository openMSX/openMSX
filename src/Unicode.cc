//$Id$

#include <cstdlib>
#include <algorithm>
#include "Unicode.hh"

using std::string;

namespace openmsx {

namespace Unicode {

/* decodes a a string possibly containing UTF-8 sequences to a 
 * string of 8-bit characters.
 * characters >= 0x100 are mapped to '?' for now
 * this implementation follows the guidelines in 
 * http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
 * except for the following:
 *  - the two byte seqence 0xC0 0x80 for '\0' is allowed
 *  - surrogates are ignored
 *
 * 
 * TODO: catch surrogates
 * TODO: respond to malformed sequences in a more appropriate way
 */

static void bad_utf(const char* s)
{
  fprintf(stdout,"error in UTF-8 encoding: %s\n",s);
}

string utf8ToAscii(const string & utf8)
{
	string res;

	for (string::const_iterator it=utf8.begin() ; it!=utf8.end() ; ) {
		char first = *it++;
		switch (first & 0xC0) {
		case 0x80:
			bad_utf("unexpected continuation byte");
			res.push_back('?');
			break;
		case 0xC0:
			char nbyte, mask, i;
			unsigned int uni;
			for (mask=0x20, first-=0xC0, nbyte=2 ; 
					 first & mask ; mask >>= 1, ++nbyte)
				first-=mask;
			if (nbyte>6) { 
				bad_utf("illegal byte");
				uni=0xFFFD;
				nbyte=0;
			}
			else {
				for (i=1, uni=first ; i<nbyte ; ++i) {
					if ((it==utf8.end()||(*it&0xC0)!=0x80)) {
						bad_utf("incomplete sequence");
						uni=0xFFFD;
						break;
					}
					else
						uni=(uni<<6)+(*it++ & 0x3F);
				}
				// check for overlong sequences
				if ( (i==nbyte)&&(uni < (1u<<(5*nbyte-(nbyte==2?3:4)))) )
					if ((uni!=0)||(nbyte!=2)) { // 2 bytes is acceptable for '\0'
						bad_utf("overlong sequence");
						uni=0xFFFD;
					}
				if ((uni==0xFFFE)||(uni==0xFFFF))
					bad_utf("illegal code");
			}
			res.push_back((uni>0x100)?'?':char(uni));
			break;
		default:
			res.push_back(first);
			break;
		}
	}
	return res;
}

} // namespace Unicode

} // namespace openmsx
