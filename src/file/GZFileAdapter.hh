// $Id$

#ifndef __GZFILEADAPTER_HH__
#define __GZFILEADAPTER_HH__

#include <zlib.h>
#include "CompressedFileAdapter.hh"

namespace openmsx {

class GZFileAdapter : public CompressedFileAdapter
{
	public:
		GZFileAdapter(FileBase* file);

	private:
		bool skipHeader(z_stream& s);
		byte getByte(z_stream &s);
};

} // namespace openmsx

#endif

