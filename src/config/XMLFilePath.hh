// $Id$

#ifndef __XMLFILEPATH_HH__
#define __XMLFILEPATH_HH__

#include "XMLConfig.hh"
#include "MSXFilePath.hh"

namespace XMLConfig
{

class FilePath: public virtual MSXConfig::FilePath, public virtual CustomConfig
{
public:
	FilePath();
	virtual ~FilePath();

	virtual void backendInit(XML::Element *e);
private:
	FilePath(const FilePath &foo); // block usage
	FilePath &operator=(const FilePath &foo); // block usage
};

}; // end namespace XMLConfig

#endif // defined __XMLFILEPATH_HH__
