#ifndef GZFILEADAPTER_HH
#define GZFILEADAPTER_HH

#include "CompressedFileAdapter.hh"

namespace openmsx {

class GZFileAdapter final : public CompressedFileAdapter
{
public:
	explicit GZFileAdapter(std::unique_ptr<FileBase> file, zstring_view filename);

private:
	void decompress(FileBase& file, Decompressed& decompressed) override;
};

} // namespace openmsx

#endif
