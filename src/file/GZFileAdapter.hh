// $Id$

#ifndef GZFILEADAPTER_HH
#define GZFILEADAPTER_HH

#include "CompressedFileAdapter.hh"

namespace openmsx {

class GZFileAdapter : public CompressedFileAdapter
{
public:
	GZFileAdapter(std::auto_ptr<FileBase> file);

protected:
	virtual void decompress();
};

} // namespace openmsx

#endif
