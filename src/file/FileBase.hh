// $Id$

#ifndef __FILEBASE_HH__
#define __FILEBASE_HH__

#include <string>
#include "openmsx.hh"

using std::string;

namespace openmsx {

class FileBase
{
	public:
		FileBase();
		virtual ~FileBase();
		virtual void read (byte* buffer, int num) = 0;
		virtual void write(const byte* buffer, int num) = 0;
		virtual byte* mmap(bool writeBack = false);
		virtual void munmap();
		virtual int getSize() = 0;
		virtual void seek(int pos) = 0;
		virtual int getPos() = 0;
		virtual const string getURL() const = 0;
		virtual const string getLocalName() const = 0;
		virtual bool isReadOnly() const = 0;

	protected:
		byte* mmem;

	private:
		bool mmapWrite;
		int mmapSize;
};

} // namespace openmsx

#endif
