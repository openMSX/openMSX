// $Id$

#ifndef __COMPRESSEDFILEADAPTER_HH__
#define __COMPRESSEDFILEADAPTER_HH__

#include "FileBase.hh"

namespace openmsx {

class CompressedFileAdapter : public FileBase
{
	public:
		virtual ~CompressedFileAdapter();
		virtual void read(byte* buffer, int num);
		virtual void write(const byte* buffer, int num);
		virtual int getSize();
		virtual void seek(int pos);
		virtual int getPos();
		virtual const string getURL() const;
		virtual const string getLocalName();
		virtual bool isReadOnly() const;

	protected:
		CompressedFileAdapter(FileBase* file);

		FileBase* file;
		byte* buf;
		int size;

	private:
		int pos;

		static int tmpCount;	// nb of files in tmp dir
		static string tmpDir;	// name of tmp dir (when tmpCount > 0)
		char* localName;	// name of tmp file (when != 0)
};

} // namespace openmsx

#endif

