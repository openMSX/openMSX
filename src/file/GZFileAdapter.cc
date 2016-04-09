#include "GZFileAdapter.hh"
#include "ZlibInflate.hh"
#include "FileException.hh"

namespace openmsx {

const byte ASCII_FLAG   = 0x01; // bit 0 set: file probably ascii text
const byte HEAD_CRC     = 0x02; // bit 1 set: header CRC present
const byte EXTRA_FIELD  = 0x04; // bit 2 set: extra field present
const byte ORIG_NAME    = 0x08; // bit 3 set: original file name present
const byte COMMENT      = 0x10; // bit 4 set: file comment present
const byte RESERVED     = 0xE0; // bits 5..7: reserved


GZFileAdapter::GZFileAdapter(std::unique_ptr<FileBase> file_)
	: CompressedFileAdapter(std::move(file_))
{
}

static bool skipHeader(ZlibInflate& zlib, std::string& originalName)
{
	// check magic bytes
	if (zlib.get16LE() != 0x8B1F) {
		return false;
	}

	byte method = zlib.getByte();
	byte flags = zlib.getByte();
	if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
		return false;
	}

	// Discard time, xflags and OS code:
	zlib.skip(6);

	if ((flags & EXTRA_FIELD) != 0) {
		// skip the extra field
		int len  = zlib.get16LE();
		zlib.skip(len);
	}
	if ((flags & ORIG_NAME) != 0) {
		// get the original file name
		originalName = zlib.getCString();
	}
	if ((flags & COMMENT) != 0) {
		// skip the .gz file comment
		zlib.getCString();
	}
	if ((flags & HEAD_CRC) != 0) {
		// skip the header crc
		zlib.skip(2);
	}
	return true;
}

void GZFileAdapter::decompress(FileBase& f, Decompressed& d)
{
	size_t size;
	const byte* data = f.mmap(size);
	ZlibInflate zlib(data, size);
	if (!skipHeader(zlib, d.originalName)) {
		throw FileException("Not a gzip header");
	}
	d.size = zlib.inflate(d.buf);
}

} // namespace openmsx
