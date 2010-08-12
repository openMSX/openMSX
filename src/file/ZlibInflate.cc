// $Id$

#include "ZlibInflate.hh"
#include "FileException.hh"
#include "Math.hh"
#include "MemBuffer.hh"
#include "StringOp.hh"
#include <cassert>

namespace openmsx {

ZlibInflate::ZlibInflate(const byte* input, unsigned inputLen)
{
	s.zalloc = 0;
	s.zfree = 0;
	s.opaque = 0;
	s.next_in  = const_cast<byte*>(input);
	s.avail_in = inputLen;
	wasInit = false;
}

ZlibInflate::~ZlibInflate()
{
	if (wasInit) {
		inflateEnd(&s);
	}
}

void ZlibInflate::skip(unsigned num)
{
	for (unsigned i = 0; i < num; ++i) {
		getByte();
	}
}

byte ZlibInflate::getByte()
{
	if (s.avail_in <= 0) {
		throw FileException(
			"Error while decompressing: unexpected end of file.");
	}
	--s.avail_in;
	return *(s.next_in++);
}

unsigned ZlibInflate::get16LE()
{
	unsigned result = getByte();
	result += getByte() << 8;
	return result;
}

unsigned ZlibInflate::get32LE()
{
	unsigned result = getByte();
	result += getByte() <<  8;
	result += getByte() << 16;
	result += getByte() << 24;
	return result;
}

std::string ZlibInflate::getString(unsigned len)
{
	std::string result;
	for (unsigned i = 0; i < len; ++i) {
		result.push_back(getByte());
	}
	return result;
}

std::string ZlibInflate::getCString()
{
	std::string result;
	while (char c = getByte()) {
		result.push_back(c);
	}
	return result;
}

void ZlibInflate::inflate(MemBuffer<byte>& output, unsigned sizeHint)
{
	int initErr = inflateInit2(&s, -MAX_WBITS);
	if (initErr != Z_OK) {
		throw FileException(StringOp::Builder()
			<< "Error initializing inflate struct: " << zError(initErr));
	}
	wasInit = true;

	output.resize(sizeHint);
	s.avail_out = output.size();
	while (true) {
		s.next_out = output.data() + s.total_out;
		int err = ::inflate(&s, Z_NO_FLUSH);
		if (err == Z_STREAM_END) {
			break;
		}
		if (err != Z_OK) {
			throw FileException(StringOp::Builder()
				<< "Error decompressing gzip: " << zError(err));
		}
		unsigned oldSize = output.size();
		output.resize(oldSize * 2); // double buffer size
		s.avail_out = output.size() - oldSize;
	}

	// set actual size
	output.resize(s.total_out);
}

} // namespace openmsx
