// $Id$
//

#include "MSXFilePath.hh"

namespace MSXConfig
{

FilePath::FilePath()
:CustomConfig(std::string("filepath"))
{
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
