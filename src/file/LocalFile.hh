// $Id$

#ifndef __LOCALFILE_HH__
#define __LOCALFILE_HH__

#include "FileBase.hh"
#include <fstream>


class LocalFile : public FileBase
{
	public:
		LocalFile(const std::string &filename, int options);
		virtual ~LocalFile();
		virtual void read (byte* buffer, int num);
		virtual void write(const byte* buffer, int num);
		virtual int size();
		virtual void seek(int pos);
		virtual int pos();

	private:
		bool readOnly;
		std::fstream *file;
};

#endif

