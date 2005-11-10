// $Id$

#include "GZFileAdapter.hh"
#include "FileException.hh"
#include <cassert>
#include <zlib.h>

namespace openmsx {

const byte ASCII_FLAG   = 0x01; // bit 0 set: file probably ascii text
const byte HEAD_CRC     = 0x02; // bit 1 set: header CRC present
const byte EXTRA_FIELD  = 0x04; // bit 2 set: extra field present
const byte ORIG_NAME    = 0x08; // bit 3 set: original file name present
const byte COMMENT      = 0x10; // bit 4 set: file comment present
const byte RESERVED     = 0xE0; // bits 5..7: reserved


GZFileAdapter::GZFileAdapter(std::auto_ptr<FileBase> file_)
	: CompressedFileAdapter(file_)
{
}

static byte getByte(z_stream &s)
{
	assert(s.avail_in);
	--s.avail_in;
	return *(s.next_in++);
}

static bool skipHeader(z_stream& s, std::string& originalName)
{
	// check magic bytes
	if ((getByte(s) != 0x1F) || (getByte(s) != 0x8B)) {
		return false;
	}

	byte method = getByte(s);
	byte flags = getByte(s);
	if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
		return false;
	}

	// Discard time, xflags and OS code:
	for (int i = 0; i < 6; ++i) {
		getByte(s);
	}

	if ((flags & EXTRA_FIELD) != 0) {
		// skip the extra field
		int len  = getByte(s);
		len += getByte(s) << 8;
		while (len--) {
			getByte(s);
		}
	}
	if ((flags & ORIG_NAME) != 0) {
		// skip the original file name
		while (char c = getByte(s)) {
			originalName.push_back(c);
		}
	}
	if ((flags & COMMENT) != 0) {
		// skip the .gz file comment
		while (getByte(s));
	}
	if ((flags & HEAD_CRC) != 0) {
		// skip the header crc
		getByte(s);
		getByte(s);
	}
	return true;
}

void GZFileAdapter::decompress()
{
	int inputSize = file->getSize();
	byte* inputBuf = file->mmap();

	z_stream s;
	s.zalloc = 0;
	s.zfree = 0;
	s.opaque = 0;
	s.next_in  = inputBuf;
	s.avail_in = inputSize;
	if (!skipHeader(s, originalName)) {
		throw FileException("Not a gzip header");
	}

	inflateInit2(&s, -MAX_WBITS);

	size = 64 * 1024;	// initial buffer size
	buf = (byte*)malloc(size);
	while (true) {
		s.next_out = buf + s.total_out;
		s.avail_out = size - s.total_out;
		int err = inflate(&s, Z_NO_FLUSH);
		if (err == Z_STREAM_END) {
			break;
		}
		if (err != Z_OK) {
			free(buf);
			buf = 0;
			throw FileException("Error decompressing gzip");
		}
		size *= 2;	// double buffer size
		buf = (byte*)realloc(buf, size);
	}
	size = s.total_out;
	buf = (byte*)realloc(buf, size);	// free unused space in buf

	inflateEnd(&s);

	file->munmap();
}

} // namespace openmsx
