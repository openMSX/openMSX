// $Id$

#include "msxconfig.hh"
#include "xmlhelper.hh"
#include "xmlhelper.ii"

// you need libxml++ 0.13 and libxml2 > 2.0.0
#include <xml++.h>

// for atoi()
#include <stdlib.h>

// TODO reorder methods
// TODO page,ps,ss value errors


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
	for (std::list<Device*>::iterator i(deviceList.begin()); i != deviceList.end(); i++)
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
		// this list should only contain device nodes for now
		if (!(*i)->is_content())
		{
			Device *d = new Device(*i);
			deviceList.push_back(d);
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

MSXConfig::Device::~Device()
{
	for (std::list<Parameter*>::iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		delete *i;
	}
}

MSXConfig::Device::Device(XMLNode *deviceNode):desc(""),rem("")
{
// TODO: rework this

	std::ostringstream buffer;

// device - <!ELEMENT msxconfig (device)+>
	XMLHelper<MSXConfig::XMLParseException> xh_device(deviceNode);
	// check if it is a devicenode
	xh_device.checkName("device");

// id - ATTLIST device id CDATA #IMPLIED>
	xh_device.checkProperty("id");
	id = xh_device.getProperty("id");

// type - <!ELEMENT device (type,slotted*,parameter*,desc?,rem?)>

	xh_device.checkChildrenAtLeast(1);
	unsigned int device_children_count = xh_device.childrenSize();
	XMLNodeList::const_iterator device_children_i = deviceNode->children().begin();
	XMLHelper<MSXConfig::XMLParseException> xh_device_child(*device_children_i);
	xh_device_child.checkName("type");
	xh_device_child.checkContentNode();
	deviceType = xh_device_child.getContent();
	if (device_children_count==1)
		return; // it was only one child node, soo we can return
	// else: either a slotted structure, or the first parameter

// slotted*,parameter* - <!ELEMENT device (type,slotted*,parameter*,desc?,rem?)>

	++device_children_i;
	xh_device_child.setNode(*device_children_i);
	std::list<string> string_list;
	string_list.push_back("slotted");
	string_list.push_back("parameter");
	string_list.push_back("rem");
	string_list.push_back("desc");
	xh_device_child.checkName(string_list);

// slotted*,parameter* - <!ELEMENT device (type,slotted*,parameter*)>

	while (device_children_i!=deviceNode->children().end())
	{
		xh_device_child.setNode(*device_children_i);
		if (xh_device_child.justCheckName("slotted"))
		{

// slotted* - <!ELEMENT device (type,slotted*,parameter*)>

			xh_device_child.checkChildrenAtLeast(1);
			XMLNodeList::const_iterator slotted_children_i = (*device_children_i)->children().begin();
			int ps = -1;
			int ss = -1;
			int page = -1;
			while (slotted_children_i!=(*device_children_i)->children().end())
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
			device_children_i++;
		}
		else if (xh_device_child.justCheckName("parameter"))

// parameter* - <!ELEMENT device (type,slotted*,parameter*)>

		{
			xh_device_child.checkProperty("name");
			string name(xh_device_child.getProperty("name"));
			// class is optional
			string clasz(xh_device_child.getProperty("class"));
			xh_device_child.checkContentNode();
			string value(xh_device_child.getContent());
			//
			parameters.push_back(new Parameter(name,value,clasz));
			device_children_i++;
		}
		else if (xh_device_child.justCheckName("desc"))
		{
			xh_device_child.checkContentNode();
			desc = xh_device_child.getContent();
			device_children_i++;
		}
		else if (xh_device_child.justCheckName("rem"))
		{
			xh_device_child.checkContentNode();
			desc = xh_device_child.getContent();
			device_children_i++;
		}
	}
}

const std::string &MSXConfig::Device::getType()
{
	return deviceType;
}

const std::string &MSXConfig::Device::getDesc()
{
	return desc;
}

const std::string &MSXConfig::Device::getRem()
{
	return rem;
}

const std::string &MSXConfig::Device::getId()
{
	return id;
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

bool MSXConfig::Device::hasParameter(const std::string &name)
{
	for (list<Parameter*>::const_iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		if ((*i)->name==name)
			return true;
	}
	return false;
}

const std::string &MSXConfig::Device::getParameter(const std::string &name)
{
	std::ostringstream buffer;
	for (list<Parameter*>::const_iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		if ((*i)->name==name)
			return ((*i)->value);
	}
	buffer << "Trying to get non-existing parameter " << name << " for device i" << id;
	throw Exception(buffer);
}

void MSXConfig::Device::dump()
{
	std::cout << "Device id='" << getId() << "', type='" << getType() << "'" << std::endl;
	std::cout << "<desc>";
	std::cout << getDesc();
	std::cout << "</desc>" << std::endl;
	std::cout << "<rem>";
	std::cout << getRem();
	std::cout << "</rem>" << std::endl;
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
	for (std::list<Parameter*>::const_iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		std::cout << "       parameter name='" << (*i)->name << "' value='" << (*i)->value << "' class='" << (*i)->clasz << "'" << std::endl;
	}
}

MSXConfig::Device::Parameter::Parameter(const std::string &nam, const std::string &valu, const std::string &clas=""):name(nam),value(valu),clasz(clas)
{
}

std::list<const MSXConfig::Device::Parameter*> MSXConfig::Device::getParametersWithClass(const std::string &clasz)
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
