// $Id$

#ifndef __GZFILEADAPTER_HH__
#define __GZFILEADAPTER_HH__

#include <zlib.h>
#include <string>
#include "FileBase.hh"
#include "File.hh"

using std::string;

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
		virtual const string getLocalName();
		virtual bool isReadOnly() const;

	private:
		bool skipHeader(z_stream& s);
		byte getByte(z_stream &s);

		FileBase* file;
		byte* buf;
		int size;
		int pos;

		static int tmpCount;	// nb of files in tmp dir
		static string tmpDir;	// name of tmp dir (when tmpCount > 0)
		char* localName;	// name of tmp file (when != 0)
};

} // namespace openmsx

#endif

