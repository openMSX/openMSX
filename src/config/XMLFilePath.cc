// $Id$
//

#include "XMLFilePath.hh"

namespace XMLConfig
{

FilePath::FilePath()
:MSXConfig::FilePath(),XMLConfig::CustomConfig()
{
}

FilePath::~FilePath()
{
}

void FilePath::backendInit(XML::Element *e)
{
	// TODO
}

}; // end namespace XMLConfig
