// $Id$

#ifndef __MSXFILEPATH_HH__
#define __MSXFILEPATH_HH__

#include "MSXConfig.hh"

namespace MSXConfig
{

class FilePath: public CustomConfig
{
public:
	FilePath();
	virtual ~FilePath();
private:
	FilePath(const FilePath &foo); // block usage
	FilePath &operator=(const FilePath &foo); // block usage
};

}; // end namespace MSXConfig

#endif // defined __MSXFILEPATH_HH__
