// $Id$

#include "IPS.hh"
#include "File.hh"
#include "MSXException.hh"
#include <string.h> // memcmp

namespace openmsx {

void IPS::applyPatch(byte* dest, unsigned size, const std::string& filename)
{
	File ipsFile(filename);

	byte buf[5];
	ipsFile.read(buf, 5);
	if (memcmp(buf, "PATCH", 5) != 0) {
		throw FatalError("Invalid IPS file: " + filename);
	}
	ipsFile.read(buf, 3);
	while (memcmp(buf, "EOF", 3) != 0) {
		unsigned offset = 0x10000 * buf[0] + 0x100 * buf[1] + buf[2];
		ipsFile.read(buf, 2);
		unsigned length = 0x100 * buf[0] + buf[1];
		if (length == 0) {
			// RLE encoded
			ipsFile.read(buf, 3);
			length = 0x100 * buf[0] + buf[1];
			if ((offset + length) > size) {
				throw FatalError("Invalid IPS file: " + filename);
			}
			memset(dest + offset, buf[2], length);
		} else {
			// patch bytes
			if ((offset + length) > size) {
				throw FatalError("Invalid IPS file: " + filename);
			}
			ipsFile.read(dest + offset, length);
		}
		ipsFile.read(buf, 3);
	}
}

} // namespace openmsx

