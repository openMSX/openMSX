// $Id$

// stub code for testing new config code

#include "MSXConfig.hh"

int main(int argc, char** argv)
{
	MSXConfig::Backend* config = MSXConfig::Backend::createBackend("xml");
	try
	{
		config->loadFile("msxconfig.xml");
	}
	catch (MSXConfig::Exception &e)
	{
		std::cout << e.desc << std::endl;
	}
	delete config;
}
