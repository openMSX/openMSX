// $Id$

#ifndef __XMLCONFIG_HH__
#define __XMLCONFIG_HH__

#include "MSXConfig.hh"
#include "libxmlx/xmlx.hh"

namespace XMLConfig
{

class Config: virtual public MSXConfig::Config
{
public:
	Config(XML::Element *element);
	virtual ~Config();

	virtual const std::string &getType();
	virtual const std::string &getId();
	virtual const std::string &getDesc();
	virtual const std::string &getRem();

	virtual bool hasParameter(const std::string &name);
	virtual const std::string &getParameter(const std::string &name);
	virtual const bool getParameterAsBool(const std::string &name);
	virtual const int getParameterAsInt(const std::string &name);
	virtual const uint64 getParameterAsUint64(const std::string &name);
	virtual std::list<Parameter*>* getParametersWithClass(const std::string &clasz);

private:
	Config(); // block usage
	Config(const Config &foo);            // block usage
	Config &operator=(const Config &foo); // block usage

	XML::Element* element;

	XML::Element* getParameterElement(const std::string &name);
};

class Device: public XMLConfig::Config, public MSXConfig::Device
{
public:
	Device(XML::Element *element);
	virtual ~Device();

private:
	Device(); // block usage
	Device(const Device &foo);            // block usage
	Device &operator=(const Device &foo); // block usage
};

class Backend: public MSXConfig::Backend
{

// my factory is my friend:
friend MSXConfig::Backend* MSXConfig::Backend::createBackend(const std::string &name);

public:
	virtual void loadFile(const std::string &filename);
	virtual void saveFile();
	virtual void saveFile(const std::string &filename);

	virtual MSXConfig::Config* getConfigById(const std::string &id);

protected:
	virtual ~Backend();
	Backend();

private:
	Backend(const Backend &foo);            // block usage
	Backend &operator=(const Backend &foo); // block usage

	bool hasConfigWithId(const std::string &id);

	std::list<XML::Document*> docs;
	std::list<Config*> configs;
	std::list<Device*> devices;
};

}; // end namespace XMLConfig

#endif // defined __XMLCONFIG_HH__
