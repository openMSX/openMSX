// $Id$

#ifndef __ZIPFILEADAPTER_HH__
#define __ZIPFILEADAPTER_HH__

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

