#ifndef __FILE_HH__
#define __FILE_HH__

#include "openmsx.hh"

#include <string>

class File
{
public:
	typedef enum
	{
		read   = 1,
		write  = 2,
		prefer = 4
	} mode;

	static File* createFile(const std::string& uri, byte mode=read|write|prefer);
	virtual void fetch()=0;
	virtual byte* getData(size_t& len)=0;

protected:
	File();
	virtual ~File();
private:
	File(const File& foo);
	File& operator=(const File& foo);
};

#endif
