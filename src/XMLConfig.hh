// $Id$

#ifndef __XMLCONFIG_HH__
#define __XMLCONFIG_HH__

#include "MSXConfig.hh"
#include "libxmlx/xmlx.hh"

namespace XMLConfig
{

class Config: public MSXConfig::Config
{
public:
	Config(XML::Element *element);
	virtual ~Config();

	virtual const std::string &getType();
	virtual const std::string &getId();

private:
	Config(); // block usage

	XML::Element* element;
};

class Device: public Config, public MSXConfig::Device
{
public:
	Device(XML::Element *element);
	virtual ~Device();

private:
	Device(); // block usage
};

class Backend: public MSXConfig::Backend
{

// my factory is my friend:
friend MSXConfig::Backend* MSXConfig::Backend::createBackend(const std::string &name);

public:
	virtual void loadFile(const std::string &filename);
	virtual void saveFile();
	virtual void saveFile(const std::string &filename);

	virtual MSXConfig::Config* getConfigById(const std::string &type);

protected:
	virtual ~Backend();
	Backend();

private:
	std::list<XML::Document*> docs;
	std::list<Config*> configs;
	std::list<Device*> devices;
};

}; // end namespace XMLConfig

#endif // defined __XMLCONFIG_HH__
