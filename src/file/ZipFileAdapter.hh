// $Id$

#ifndef ZIPFILEADAPTER_HH
#define ZIPFILEADAPTER_HH

#include "CompressedFileAdapter.hh"

namespace openmsx {

class ZipFileAdapter : public CompressedFileAdapter
{
public:
	ZipFileAdapter(std::auto_ptr<FileBase> file);

protected:
	virtual void decompress();
};

} // namespace openmsx

#endif
