// $Id$

#include "msxconfig.hh"
#include "xmlhelper.hh"
#include "xmlhelper.ii"

// you need libxml++ 0.13 and libxml2 > 2.0.0
#include <xml++.h>

// for atoi()
#include <cstdlib>

// for tolower
#include <algorithm>

// TODO page,ps,ss value errors
// TODO turn on validation with extern libxml switch var


MSXConfig *volatile MSXConfig::oneInstance;

MSXConfig *MSXConfig::instance()
{
	if (oneInstance == 0)
		oneInstance = new MSXConfig;
	return oneInstance;
}

MSXConfig::MSXConfig():tree(0),loaded_filename("")
{
}

MSXConfig::~MSXConfig()
{
	delete tree; // delete 0 is silently ignored in C++
	// destroy list content:
	for (std::list<Config*>::iterator i(configList.begin()); i != configList.end(); i++)
	{
		delete *i;
	}
}

void MSXConfig::loadFile(const std::string &filename)
{
	tree = new XMLTree();
	if (!tree->read(filename))
		throw MSXConfig::XMLParseException("File I/O Error.");
	loaded_filename = filename;
	XMLNodeList children=tree->root()->children();
	for (XMLNodeList::iterator i = children.begin(); i != children.end(); i++)
	{
		// this list should only contain device and config nodes for now
		if (!(*i)->is_content())
		{
			if ((*i)->name() == "device")
			{
				Device *d = new Device(*i);
				deviceList.push_back(d);
				allList.push_back(d);
			}
			else if ((*i)->name() == "config")
			{
				Config *c = new Config(*i);
				configList.push_back(c);
				allList.push_back(c);
			}
		}
	}
	delete tree;
}

void MSXConfig::saveFile()
{
	saveFile(loaded_filename);
}

void MSXConfig::saveFile(const std::string &filename)
{
	throw std::string("void MSXConfig::saveFile(const string &filename) Unimplemented");
}

MSXConfig::Config::~Config()
{
	for (std::list<Parameter*>::iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		delete *i;
	}
}

MSXConfig::Device::~Device()
{
	for (std::list<Slotted*>::iterator i=slotted.begin(); i != slotted.end(); i++)
	{
		delete *i;
	}
	// TODO: do I need to call Config's destructor explicitly?
}

MSXConfig::Config::Config(XMLNode *configNode):desc(""),rem(""),slotted_pass(0)
{

	std::ostringstream buffer;

// config - <!ELEMENT msxconfig (device|config)+>
	XMLHelper<MSXConfig::XMLParseException> xh_config(configNode);
	// check if it is a configNode
	//xh_config.checkName("config");

// id - ATTLIST config id CDATA #IMPLIED>
	xh_config.checkProperty("id");
	id = xh_config.getProperty("id");

// type - <!ELEMENT config (type,parameter*,desc?,rem?)>

	xh_config.checkChildrenAtLeast(1);
	unsigned int config_children_count = xh_config.childrenSize();
	XMLNodeList::const_iterator config_children_i = configNode->children().begin();
	XMLHelper<MSXConfig::XMLParseException> xh_config_child(*config_children_i);
	xh_config_child.checkName("type");
	xh_config_child.checkContentNode();
	configType = xh_config_child.getContent();
	if (config_children_count==1)
		return; // it was only one child node, soo we can return
	// else: either a slotted structure, or the first parameter

// parameter* - <!ELEMENT config (type,parameter*,desc?,rem?)>

	++config_children_i;
	xh_config_child.setNode(*config_children_i);
	std::list<std::string> string_list;
	string_list.push_back("slotted");
	string_list.push_back("parameter");
	string_list.push_back("rem");
	string_list.push_back("desc");
	xh_config_child.checkName(string_list);

// parameter* - <!ELEMENT config (type,parameter*)>
	while (config_children_i!=configNode->children().end())
	{
		xh_config_child.setNode(*config_children_i);
		if (xh_config_child.justCheckName("slotted"))
		{
			if (slotted_pass == 0)
			{
				slotted_pass = new XMLNodeList;
				std::cerr << "allocated slotted xmlnodelist" << std::endl;
			}
			std::cerr << "added slotted" << std::endl;
			slotted_pass->push_back(*config_children_i);
			config_children_i++;
		}
		else if (xh_config_child.justCheckName("parameter"))
		{
			xh_config_child.checkProperty("name");
			std::string name(xh_config_child.getProperty("name"));
			// class is optional
			std::string clasz(xh_config_child.getProperty("class"));
			xh_config_child.checkContentNode();
			std::string value(xh_config_child.getContent());
			//
			parameters.push_back(new Parameter(name,value,clasz));
			config_children_i++;
		}
		else if (xh_config_child.justCheckName("desc"))
		{
			xh_config_child.checkContentNode();
			desc = xh_config_child.getContent();
			config_children_i++;
		}
		else if (xh_config_child.justCheckName("rem"))
		{
			xh_config_child.checkContentNode();
			desc = xh_config_child.getContent();
			config_children_i++;
		}
	}
}


MSXConfig::Device::Device(XMLNode *deviceNode):Config(deviceNode)
{
	if (slotted_pass != 0)
	{
		XMLNodeList::const_iterator slotted_list_i = slotted_pass->begin();
		for ( ; slotted_list_i != slotted_pass->end(); slotted_list_i++)
		{
	
	// slotted* - <!ELEMENT device (type,slotted*,parameter*)>
			XMLHelper<MSXConfig::XMLParseException> xh_device_child(*slotted_list_i);
			xh_device_child.checkChildrenAtLeast(1);
			XMLNodeList::const_iterator slotted_children_i = (*slotted_list_i)->children().begin();
			int ps = -1;
			int ss = -1;
			int page = -1;
			while (slotted_children_i!=(*slotted_list_i)->children().end())
			{
				XMLHelper<MSXConfig::XMLParseException> xh_slotted_child(*slotted_children_i);
				xh_slotted_child.checkContentNode();
				if (xh_slotted_child.justCheckName("ps")) // ps - <!ELEMENT ps (#PCDATA)>
					ps = atoi(xh_slotted_child.getContent().c_str());
				else if (xh_slotted_child.justCheckName("ss")) // ss - <!ELEMENT ss (#PCDATA)>
					ss = atoi(xh_slotted_child.getContent().c_str());
				else if (xh_slotted_child.justCheckName("page")) // page - <!ELEMENT page (#PCDATA)>
					page =  atoi(xh_slotted_child.getContent().c_str());
				slotted_children_i++;
			}
			slotted.push_back(new Slotted(ps,ss,page));
			// increment on the <slotted> and <parameter> level
		}
	delete slotted_pass;
	}
}

const std::string &MSXConfig::Config::getType()
{
	return configType;
}

const std::string &MSXConfig::Config::getDesc()
{
	return desc;
}

const std::string &MSXConfig::Config::getRem()
{
	return rem;
}

const std::string &MSXConfig::Config::getId()
{
	return id;
}

bool MSXConfig::Config::isDevice()
{
	return false;
}

bool MSXConfig::Device::isDevice()
{
	return true;
}

bool MSXConfig::Device::isSlotted()
{
	return (!slotted.empty());
}

int MSXConfig::Device::getPage()
{
	if (slotted.empty())
	{
		throw MSXConfig::Exception("No slotted defined.");
	}
	return (*slotted.begin())->getPage();
}

int MSXConfig::Device::getPS()
{
	if (slotted.empty())
	{
		throw MSXConfig::Exception("No slotted defined.");
	}
	return (*slotted.begin())->getPS();
}

int MSXConfig::Device::getSS()
{
	if (slotted.empty())
	{
		throw MSXConfig::Exception("No slotted defined.");
	}
	return (*slotted.begin())->getSS();
}

bool MSXConfig::Config::hasParameter(const std::string &name)
{
	for (std::list<Parameter*>::const_iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		if ((*i)->name==name)
			return true;
	}
	return false;
}

const MSXConfig::Config::Parameter &MSXConfig::Config::getParameterByName(const std::string &name)
{
	std::ostringstream buffer;
	for (std::list<Parameter*>::const_iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		if ((*i)->name==name)
			return *(*i);
	}
	buffer << "Trying to get non-existing parameter " << name << " for device i" << id;
	throw Exception(buffer);
}

const std::string &MSXConfig::Config::getParameter(const std::string &name)
{
	return getParameterByName(name).value;
}

const bool MSXConfig::Config::getParameterAsBool(const std::string &name)
{
	return getParameterByName(name).getAsBool();
}

const int MSXConfig::Config::getParameterAsInt(const std::string &name)
{
	return getParameterByName(name).getAsInt();
}

const uint64 MSXConfig::Config::getParameterAsUint64(const std::string &name)
{
	return getParameterByName(name).getAsUint64();
}

void MSXConfig::Config::dump()
{
	std::cout << "Device id='" << getId() << "', type='" << getType() << "'" << std::endl;
	std::cout << "<desc>";
	std::cout << getDesc();
	std::cout << "</desc>" << std::endl;
	std::cout << "<rem>";
	std::cout << getRem();
	std::cout << "</rem>" << std::endl;
	for (std::list<Parameter*>::const_iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		std::cout << "       parameter name='" << (*i)->name << "' value='" << (*i)->value << "' class='" << (*i)->clasz << "'" << std::endl;
	}
}

void MSXConfig::Device::dump()
{
	Config::dump();
if (!slotted.empty())
	{
		std::cout << "slotted:";
		for (std::list<Slotted*>::const_iterator i=slotted.begin(); i != slotted.end(); i++)
		{
            std::cout << " ps=" << (*i)->getPS();
            if ((*i)->hasSS())
            {
                std::cout << " ss=" << (*i)->getSS();
            }
			if ((*i)->hasPage())
            {
               std::cout << " page=" << (*i)->getPage();
			}
            std::cout << std::endl;
		}
	}
	else
	{
		std::cout << "       not slotted." << std::endl;
	}
}

MSXConfig::Config::Parameter::Parameter(const std::string &nam, const std::string &valu, const std::string &clas=""):name(nam),value(valu),clasz(clas)
{
}

std::string MSXConfig::Config::Parameter::lowerValue() const
{
	std::string s(value);
	std::transform (s.begin(), s.end(), s.begin(), tolower);
	return s;
}

const bool MSXConfig::Config::Parameter::getAsBool() const
{
	std::string low = lowerValue();
	if (low == "true" || low == "yes") return true;
	return false;
}

const int MSXConfig::Config::Parameter::getAsInt() const
{
	return atoi(value.c_str());
}

const uint64 MSXConfig::Config::Parameter::getAsUint64() const
{
	return atoll(value.c_str());
}

std::list<const MSXConfig::Config::Parameter*> MSXConfig::Config::getParametersWithClass(const std::string &clasz)
{
	std::list<const Parameter*> a;
	for (std::list <Parameter*>::const_iterator i=parameters.begin();i!=parameters.end();i++)
	{
		if ((*i)->clasz==clasz)
			a.push_back(*i);
	}
	return a;
}

MSXConfig::Device::Slotted::Slotted(int PS, int SS=-1, int Page=-1):ps(PS),ss(SS),page(Page)
{
}

bool MSXConfig::Device::Slotted::hasSS()
{
	return (ps!=-1);
}

bool MSXConfig::Device::Slotted::hasPage()
{
	return (page!=-1);
}

int MSXConfig::Device::Slotted::getPS()
{
	return ps;
}

int MSXConfig::Device::Slotted::getSS()
{
	if (ss==-1)
	{
		throw MSXConfig::Exception("Request for SS on a Slotted without SS");
	}
	return ss;
}

int MSXConfig::Device::Slotted::getPage()
{
	if (page==-1)
	{
		throw MSXConfig::Exception("Request for Page on a Slotted without Page");
	}
	return page;
}

MSXConfig::Config* MSXConfig::getConfigById(const std::string &id)
{
    for (std::list <MSXConfig::Config*>::const_iterator i=configList.begin();i!=configList.end();i++)
	{
		if ((*i)->getId()==id) return *i;
	}
	throw MSXConfig::Exception("Request for non-existing config for Id");

	// not reached:
	return 0;
}
