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
};

class Device: public Config, public MSXConfig::Device
{
public:
	Device();
	virtual ~Device();
};

class Backend: public MSXConfig::Backend
{
public:
	virtual void loadFile(const std::string &filename);
	virtual void saveFile();
	virtual void saveFile(const std::string &filename);

	virtual MSXConfig::Config* getConfigById(const std::string &type);

protected:
	virtual ~Backend();

private:
	Backend();
};

}; // end namespace XMLConfig

#endif // defined __XMLCONFIG_HH__
