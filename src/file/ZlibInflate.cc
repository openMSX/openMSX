// $Id$

#include "ZlibInflate.hh"
#include "FileException.hh"
#include "StringOp.hh"

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

void ZlibInflate::inflate(std::vector<byte>& output, unsigned sizeHint)
{
	int initErr = inflateInit2(&s, -MAX_WBITS);
	if (initErr != Z_OK) {
		throw FileException(StringOp::Builder()
			<< "Error initializing inflate struct: " << zError(initErr));
	}
	wasInit = true;

	std::vector<byte> buf;
	buf.resize(sizeHint); // initial buffer size
	while (true) {
		s.next_out = &buf[0] + s.total_out;
		s.avail_out = uInt(buf.size() - s.total_out);
		int err = ::inflate(&s, Z_NO_FLUSH);
		if (err == Z_STREAM_END) {
			break;
		}
		if (err != Z_OK) {
			throw FileException(StringOp::Builder()
				<< "Error decompressing gzip: " << zError(err));
		}
		buf.resize(buf.size() * 2); // double buffer size
	}

	// assign actual size (trim excess capacity)
	output.assign(buf.begin(), buf.begin() + s.total_out);
}

} // namespace openmsx
