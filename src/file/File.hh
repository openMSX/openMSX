// $Id$

#ifndef __FILEOPENER_HH__
#define __FILEOPENER_HH__

#include <string>
#include "openmsx.hh"
#include "MSXException.hh"


class FileBase;


enum FileType {
	OTHER,
	DISK,
	ROM,
	TAPE,
	STATE,
	CONFIG,
};

enum FileOption {
	TRUNCATE = 1,
};

class FileException : public MSXException {
	public:
		FileException(const std::string &desc) : MSXException(desc) {}
};

class File
{
	public:
		File(const std::string &url, FileType fileType = OTHER,
		     int options = 0);
		~File();
		
		void read(byte* buffer, int num);
		void write(const byte* buffer, int num);
		int size();
		void seek(int pos);
		int pos();
		
		static const std::string findName(const std::string &url,
			FileType fileType = OTHER, int options = 0);
	
	private:
		static FileBase* searchFile(const std::string &origName,
			std::string &newName, FileType filetype,
			int options);
		static FileBase* openFile(const std::string &url, int options);
		
		FileBase *file;
};

#endif

