// $Id$

#ifndef __ZIPFILEADAPTER_HH__
#define __ZIPFILEADAPTER_HH__

#include "CompressedFileAdapter.hh"

namespace openmsx {

class ZipFileAdapter : public CompressedFileAdapter
{
	public:
		ZipFileAdapter(FileBase* file) throw(FileException);
};

} // namespace openmsx

#endif

