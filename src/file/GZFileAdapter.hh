// $Id$

#ifndef __GZFILEADAPTER_HH__
#define __GZFILEADAPTER_HH__

#include <zlib.h>
#include "CompressedFileAdapter.hh"

namespace openmsx {

class GZFileAdapter : public CompressedFileAdapter
{
public:
	GZFileAdapter(auto_ptr<FileBase> file);

protected:
	virtual void decompress();

private:
	bool skipHeader(z_stream& s);
	byte getByte(z_stream &s);
};

} // namespace openmsx

#endif

