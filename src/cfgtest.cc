// $Id$

// stub code for testing new config code

#include "MSXConfig.hh"

int main(int argc, char** argv)
{
	MSXConfig::Backend* config = MSXConfig::Backend::createBackend("xml");
	config->loadFile("msxconfig.xml");
	delete config;
}
