// $Id$

#ifndef GZFILEADAPTER_HH
#define GZFILEADAPTER_HH

#include <zlib.h>
#include "CompressedFileAdapter.hh"

namespace openmsx {

class GZFileAdapter : public CompressedFileAdapter
{
public:
	GZFileAdapter(std::auto_ptr<FileBase> file);

protected:
	virtual void decompress();

private:
	bool skipHeader(z_stream& s);
	byte getByte(z_stream &s);
};

} // namespace openmsx

#endif
