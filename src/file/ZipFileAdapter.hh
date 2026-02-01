#ifndef ZIPFILEADAPTER_HH
#define ZIPFILEADAPTER_HH

#include "CompressedFileAdapter.hh"

namespace openmsx {

class ZipFileAdapter final : public CompressedFileAdapter
{
public:
	explicit ZipFileAdapter(std::unique_ptr<FileBase> file, zstring_view filename);

private:
	void decompress(FileBase& file, Decompressed& decompressed) override;
};

} // namespace openmsx

#endif
