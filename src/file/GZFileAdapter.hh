// $Id$

#ifndef __GZFILEADAPTER_HH__
#define __GZFILEADAPTER_HH__

#include <zlib.h>
#include "FileBase.hh"
#include "File.hh"

namespace openmsx {

class GZFileAdapter : public FileBase
{
	public:
		GZFileAdapter(FileBase* file);
		virtual ~GZFileAdapter();
		virtual void read (byte* buffer, int num);
		virtual void write(const byte* buffer, int num);
		virtual int getSize();
		virtual void seek(int pos);
		virtual int getPos();
		virtual const string getURL() const;
		virtual const string getLocalName() const;
		virtual bool isReadOnly() const;

	private:
		bool skipHeader(z_stream& s);
		byte getByte(z_stream &s);

		FileBase* file;
		byte* buf;
		int size;
		int pos;
};

} // namespace openmsx

#endif

