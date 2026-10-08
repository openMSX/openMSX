#include "ZipFileAdapter.hh"

#include "FileException.hh"
#include "MappedFile.hh"
#include "StringOp.hh"
#include "ZlibInflate.hh"

namespace openmsx {

ZipFileAdapter::ZipFileAdapter(std::unique_ptr<FileBase> file_, zstring_view filename_, zstring_view extension_)
	: CompressedFileAdapter(std::move(file_), filename_)
	, extension(extension_)
{
}

void ZipFileAdapter::decompress(FileBase& f, Decompressed& d)
{
	auto mmap = MappedFile<const uint8_t>(f.mmap(0, true));
	ZlibInflate zlib(mmap);

	while (true) {
		if (zlib.get32LE() != 0x04034B50) {
			throw FileException("Invalid ZIP file");
		}

		// skip "version needed to extract" and "general purpose bit flag"
		zlib.skip(2 + 2);

		// compression method
		if (auto x = zlib.get16LE(); x != 0 && x != 0x0008) {
			throw FileException("Unsupported zip compression method");
		}

		// skip "last mod file time", "last mod file data", "crc32"
		zlib.skip(2 + 2 + 4);

		unsigned compSize = zlib.get32LE(); // compressed size
		unsigned origSize = zlib.get32LE(); // uncompressed size
		unsigned filenameLen = zlib.get16LE(); // filename length
		unsigned extraFieldLen = zlib.get16LE(); // extra field length
		d.originalName = zlib.getString(filenameLen); // original filename
		zlib.skip(extraFieldLen); // skip "extra field"

		if (extension.empty() || StringOp::toLower(d.originalName).ends_with(extension)) {
			d.buf = zlib.inflate(origSize);
			return;
		}

		zlib.skip(compSize);
	}
}

} // namespace openmsx
