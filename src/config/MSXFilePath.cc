// $Id$
//

#include "MSXFilePath.hh"

namespace MSXConfig
{

FilePath::FilePath()
:CustomConfig()
{
	tag = "filepath";
	PRT_DEBUG("MSXConfig::FilePath::FilePath()");
}

FilePath::~FilePath()
{
	std::list<std::string*>::iterator i = paths.begin();
	while (i != paths.end())
	{
		delete (*i);
	}
}

}; // end namespace MSXConfig
