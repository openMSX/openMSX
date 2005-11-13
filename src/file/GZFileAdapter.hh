// $Id$

#ifndef GZFILEADAPTER_HH
#define GZFILEADAPTER_HH

#include "CompressedFileAdapter.hh"

namespace openmsx {

class GZFileAdapter : public CompressedFileAdapter
{
public:
	explicit GZFileAdapter(std::auto_ptr<FileBase> file);

protected:
	virtual void decompress();
};

} // namespace openmsx

#endif
