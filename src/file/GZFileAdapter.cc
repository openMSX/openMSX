#include "GZFileAdapter.hh"
#include "ZlibInflate.hh"
#include "FileException.hh"

namespace openmsx {

static constexpr uint8_t ASCII_FLAG  = 0x01; // bit 0 set: file probably ascii text
static constexpr uint8_t HEAD_CRC    = 0x02; // bit 1 set: header CRC present
static constexpr uint8_t EXTRA_FIELD = 0x04; // bit 2 set: extra field present
static constexpr uint8_t ORIG_NAME   = 0x08; // bit 3 set: original file name present
static constexpr uint8_t COMMENT     = 0x10; // bit 4 set: file comment present
static constexpr uint8_t RESERVED    = 0xE0; // bits 5..7: reserved


GZFileAdapter::GZFileAdapter(std::unique_ptr<FileBase> file_)
	: CompressedFileAdapter(std::move(file_))
{
}

[[nodiscard]] static bool skipHeader(ZlibInflate& zlib, std::string& originalName)
{
	// check magic bytes
	if (zlib.get16LE() != 0x8B1F) {
		return false;
	}

	uint8_t method = zlib.getByte();
	uint8_t flags = zlib.getByte();
	if (method != Z_DEFLATED || (flags & RESERVED) != 0) {
		return false;
	}

	// Discard time, xflags and OS code:
	zlib.skip(6);

	if ((flags & EXTRA_FIELD) != 0) {
		// skip the extra field
		zlib.skip(zlib.get16LE());
	}
	if ((flags & ORIG_NAME) != 0) {
		// get the original file name
		originalName = zlib.getCString();
	}
	if ((flags & COMMENT) != 0) {
		// skip the .gz file comment
		(void)zlib.getCString();
	}
	if ((flags & HEAD_CRC) != 0) {
		// skip the header crc
		zlib.skip(2);
	}
	return true;
}

void GZFileAdapter::decompress(FileBase& f, Decompressed& d)
{
	ZlibInflate zlib(f.mmap());
	if (!skipHeader(zlib, d.originalName)) {
		throw FileException("Not a gzip header");
	}
	d.buf = zlib.inflate();
}

} // namespace openmsx
