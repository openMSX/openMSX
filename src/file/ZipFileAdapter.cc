// $Id$

#include <zlib.h>
#include "File.hh"
#include "ZipFileAdapter.hh"

namespace openmsx {

ZipFileAdapter::ZipFileAdapter(auto_ptr<FileBase> file_)
	: CompressedFileAdapter(file_)
{
}

void ZipFileAdapter::decompress()
{
	byte* inputBuf = file->mmap();

	byte* ptr = inputBuf;

	// file header id
	if (((*ptr++) != 0x50) || ((*ptr++) != 0x4B) ||
	    ((*ptr++) != 0x03) || ((*ptr++) != 0x04)) {
		throw FileException("Invalid ZIP file");
	}
	
	// skip "version needed to extract" and "general purpose bit flag"
	ptr += 2 + 2;
	
	// compression method
	word method = *ptr++;
	method += (*ptr++) << 8;
	if (method != 0x0008) {
		throw FileException("Unsupported zip compression method");
	}

	// skip "last mod file time" and "last mod file data" and "crc32"
	ptr += 2 + 2 + 4;

	// compressed size
	unsigned comp_size = *ptr++;
	comp_size += (*ptr++) << 8;
	comp_size += (*ptr++) << 16;
	comp_size += (*ptr++) << 24;

	// uncompressed size
	unsigned orig_size = *ptr++;
	orig_size += (*ptr++) << 8;
	orig_size += (*ptr++) << 16;
	orig_size += (*ptr++) << 24;

	// filename length
	word filename_len = *ptr++;
	filename_len += (*ptr++) << 8;
	
	// extra field length
	word extra_field_len = *ptr++;
	extra_field_len += (*ptr++) << 8;

	// skip "filename" and "extra field"
	ptr += filename_len + extra_field_len;

	z_stream s;
	s.zalloc = 0;
	s.zfree = 0;
	s.opaque = 0;
	s.next_in  = ptr;
	s.avail_in = comp_size;
	inflateInit2(&s, -MAX_WBITS);

	buf = (byte*)malloc(orig_size);
	s.next_out = buf;
	s.avail_out = orig_size;

	if (inflate(&s, Z_FINISH) != Z_STREAM_END) {
		free(buf);
		buf = 0;
		throw FileException("Error decompressing zip");
	}

	size = s.total_out;

	inflateEnd(&s);

	file->munmap();
}

} // namespace openmsx
