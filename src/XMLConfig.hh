// $Id$

#ifndef __XMLCONFIG_HH__
#define __XMLCONFIG_HH__

#include "MSXConfig.hh"

namespace XMLConfig
{

class Config: public MSXConfig::Config
{
public:
	Config();
	virtual ~Config();

	virtual const std::string &getType();
	virtual const std::string &getId();
}

};

#endif // defined __XMLCONFIG_HH__
