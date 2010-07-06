// $Id$

#ifndef GZFILEADAPTER_HH
#define GZFILEADAPTER_HH

#include "CompressedFileAdapter.hh"

namespace openmsx {

class GZFileAdapter : public CompressedFileAdapter
{
public:
	explicit GZFileAdapter(std::auto_ptr<FileBase> file);

private:
	virtual void decompress(FileBase& file, Decompressed& decompressed);
};

} // namespace openmsx

#endif
