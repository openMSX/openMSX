#ifndef ZIPFILEADAPTER_HH
#define ZIPFILEADAPTER_HH

#include "CompressedFileAdapter.hh"

namespace openmsx {

class ZipFileAdapter : public CompressedFileAdapter
{
public:
	explicit ZipFileAdapter(std::unique_ptr<FileBase> file);

private:
	virtual void decompress(FileBase& file, Decompressed& decompressed);
};

} // namespace openmsx

#endif
