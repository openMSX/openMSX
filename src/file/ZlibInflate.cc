#include "ZlibInflate.hh"

#include "FileException.hh"

#include "narrow.hh"
#include "xrange.hh"

#include <limits>

namespace openmsx {

ZlibInflate::ZlibInflate(std::span<const uint8_t> input)
{
	if (input.size() > std::numeric_limits<decltype(s.avail_in)>::max()) {
		throw FileException(
			"Error while decompressing: input file too big");
	}
	auto inputLen = static_cast<decltype(s.avail_in)>(input.size());

	s.zalloc = nullptr;
	s.zfree  = nullptr;
	s.opaque = nullptr;
	s.next_in  = const_cast<uint8_t*>(input.data());
	s.avail_in = inputLen;
	wasInit = false;
}

ZlibInflate::~ZlibInflate()
{
	if (wasInit) {
		inflateEnd(&s);
	}
}

void ZlibInflate::skip(size_t num)
{
	repeat(num, [&] { (void)getByte(); });
}

uint8_t ZlibInflate::getByte()
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

std::string ZlibInflate::getString(size_t len)
{
	std::string result;
	result.reserve(len);
	repeat(len, [&] { result.push_back(narrow_cast<char>(getByte())); });
	return result;
}

std::string ZlibInflate::getCString()
{
	std::string result;
	while (auto c = narrow_cast<char>(getByte())) {
		result.push_back(c);
	}
	return result;
}

MemBuffer<uint8_t> ZlibInflate::inflate(size_t sizeHint)
{
	if (int err = inflateInit2(&s, -MAX_WBITS);
	    err != Z_OK) {
		throw FileException(
			"Error initializing inflate struct: ", zError(err));
	}
	wasInit = true;

	size_t outSize = sizeHint;
	MemBuffer<uint8_t> output(outSize);
	s.avail_out = uInt(outSize); // TODO overflow?
	while (true) {
		s.next_out = output.data() + s.total_out;
		int err = ::inflate(&s, Z_NO_FLUSH);
		if (err == Z_STREAM_END) {
			break;
		}
		if (err != Z_OK) {
			throw FileException("Error decompressing gzip: ", zError(err));
		}
		auto oldSize = outSize;
		outSize = oldSize * 2; // double buffer size
		output.resize(outSize);
		s.avail_out = uInt(outSize - oldSize); // TODO overflow?
	}

	// set actual size
	output.resize(s.total_out);
	return output;
}

} // namespace openmsx
