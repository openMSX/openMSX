// $Id$

#include "msxconfig.hh"

// you need libxml++ 0.13 and libxml2 > 2.0.0
#include <xml++.h>

// for atoi()
#include <stdlib.h>

// TODO reorder methods
// TODO page,ps,ss value errors


int getSlottedSub(const string &subname, XMLNodeList::const_iterator &slotted_children_i, const string &id)
{
	ostringstream buffer;
	if ((*slotted_children_i)->children().size()!=1)
	{
		buffer << "Missing content node for <" << subname << "> for <device id='" << id << "'>.";
		throw MSXConfig::XMLParseException(buffer);
	}
	XMLNodeList::const_iterator content_node = (*slotted_children_i)->children().begin();
	if (!((*content_node)->is_content()))
	{
		buffer << "Child node of <" << subname << "> for <device id='" << id << "'> is not a content node.";
	throw MSXConfig::XMLParseException(buffer);
	}
	return atoi((*content_node)->content().c_str());
}

MSXConfig *volatile MSXConfig::oneInstance;

MSXConfig *MSXConfig::instance()
{
	if (oneInstance == 0)
		oneInstance = new MSXConfig;
	return oneInstance;
}

MSXConfig::MSXConfig():tree(0)
{
}

MSXConfig::~MSXConfig()
{
	delete tree; // delete 0 is silently ignored in C++
	// destroy list content:
	for (list<Device*>::iterator i(deviceList.begin()); i != deviceList.end(); i++)
	{
		delete *i;
	}
}

void MSXConfig::loadFile(const string &filename)
{
	tree = new XMLTree();
	if (!tree->read(filename))
		throw MSXConfig::XMLParseException("File I/O Error.");

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
}

MSXConfig::Device::~Device()
{
	for (list<Parameter*>::iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		delete *i;
	}
}

//MSXConfig::Device::Device(XMLNode *deviceNodeP):deviceNode(deviceNodeP),slotted(false)
MSXConfig::Device::Device(XMLNode *deviceNodeP):deviceNode(deviceNodeP)
{
	ostringstream buffer;

// device - <!ELEMENT msxconfig (device)+>

	// check if it is a devicenode
	if (deviceNode->name()!="device")
		throw MSXConfig::XMLParseException("Expected <device> node.");

// id - ATTLIST device id CDATA #IMPLIED>

	if (deviceNode->property("id")==0)
		throw MSXConfig::XMLParseException("<device> node is missing mandatory 'id' property.");
	id = deviceNode->property("id")->value();

// type - <!ELEMENT device (type,slotted*,parameter+)>

	XMLNodeList::size_type device_children_count;
	if ((device_children_count=deviceNode->children().size())<1)
	{
		buffer << "Expecting at least one child node for <device id='" << id << "'>.";
		throw MSXConfig::XMLParseException(buffer);
	}
	XMLNodeList::const_iterator device_children_i = deviceNode->children().begin();
	if ((*device_children_i)->name()!="type")
	{
		buffer << "Missing mandatory first child node <type> for <device id='" << id << "'>.";
		throw MSXConfig::XMLParseException(buffer);
	}
	if ((*device_children_i)->children().size()!=1)
	{
		buffer << "Missing content node for <type> for <device id='" << id << "'>.";
		throw MSXConfig::XMLParseException(buffer);
	}
	XMLNodeList::const_iterator content_node = (*device_children_i)->children().begin();
	if (!((*content_node)->is_content()))
	{
		buffer << "Child node of <type> for <device id='" << id << "'> is not a content node.";
		throw MSXConfig::XMLParseException(buffer);
	}
	deviceType = ((*content_node)->content());
	if (device_children_count==1)
		return; // it was only one child node, soo we can return
	// else: either a slotted structure, or the first parameter

// slotted*,parameter* - <!ELEMENT device (type,slotted*,parameter*)>

	device_children_i++;
	if ((*device_children_i)->name()!="slotted" 
		&& (*device_children_i)->name()!="parameter")
	{
		buffer << "Either <slotted> or <parameter> expected as second child node for <device id='" << id << "'>.";
		throw MSXConfig::XMLParseException(buffer);
	}

// slotted*,parameter* - <!ELEMENT device (type,slotted*,parameter*)>

	while (device_children_i!=deviceNode->children().end())
	{
		if ((*device_children_i)->name()=="slotted")
		{

// slotted* - <!ELEMENT device (type,slotted*,parameter*)>

			if ((*device_children_i)->children().size()<1)
			{
				buffer << "Expected at least 1 child node for <slotted> node in <device id='" << id << "'>.";
				throw MSXConfig::XMLParseException(buffer);
			} // end ((*device_children_i)->children().size()<1)
			XMLNodeList::const_iterator slotted_children_i = (*device_children_i)->children().begin();
			int ps = -1;
			int ss = -1;
			int page = -1;
			while (slotted_children_i!=(*device_children_i)->children().end())
			{
				if ((*slotted_children_i)->name()=="ps") // ps - <!ELEMENT ps (#PCDATA)>
					ps = getSlottedSub(string("ps"), slotted_children_i, id);
				else if ((*slotted_children_i)->name()=="ss") // ss - <!ELEMENT ss (#PCDATA)>
					ss = getSlottedSub(string("ss"), slotted_children_i, id);
				else if ((*slotted_children_i)->name()=="page") // page - <!ELEMENT page (#PCDATA)>
					page = getSlottedSub(string("page"), slotted_children_i, id);
				slotted_children_i++;
			}
			slotted.push_back(new Slotted(ps,ss,page));
			// increment on the <slotted> and <parameter> level
			device_children_i++;
		}
		else if ((*device_children_i)->name()=="parameter")

// parameter* - <!ELEMENT device (type,slotted*,parameter*)>

		{
			if ((*device_children_i)->property("name")==0)
			{
				buffer << "Expected mandatory 'name' property in <parameter> child node in <device id='" << id << "'>.";
				throw MSXConfig::XMLParseException(buffer);
			}
			string name((*device_children_i)->property("name")->value());
			// class is optional
			string clasz("");
			if ((*device_children_i)->property("class")!=0)
			{
				clasz=(*device_children_i)->property("class")->value();
			}
			if ((*device_children_i)->children().size()!=1)
			{
				buffer << "Missing content node for <parameter name='" << name << "'> for <device id='" << id << "'>.";
				throw MSXConfig::XMLParseException(buffer);
			}
			content_node = (*device_children_i)->children().begin();
			if (!((*content_node)->is_content()))
			{
				buffer << "Child node of <parameter name='" << name << "'> for <device id='" << id << "'> is not a content node.";
				throw MSXConfig::XMLParseException(buffer);
			}
			string value((*content_node)->content());
			parameters.push_back(new Parameter(name,value,clasz));
			device_children_i++;
		}
	}
}

const string &MSXConfig::Device::getType()
{
	return deviceType;
}

const string &MSXConfig::Device::getId()
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

bool MSXConfig::Device::hasParameter(const string &name)
{
	for (list<Parameter*>::const_iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		if ((*i)->name==name)
			return true;
	}
	return false;
}

const string &MSXConfig::Device::getParameter(const string &name)
{
	char buffer[200];
	for (list<Parameter*>::const_iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		if ((*i)->name==name)
			return ((*i)->value);
	}
	snprintf(buffer,200,"Trying to get non-existing parameter %s for device %s.", name.c_str(), id.c_str());
	throw Exception(buffer);
}

void MSXConfig::Device::dump()
{
	cout << "Device id='" << getId() << "', type='" << getType() << "'" << endl;
	if (!slotted.empty())
	{
		for (list<Slotted*>::const_iterator i=slotted.begin(); i != slotted.end(); i++)
		{
			cout << "       slotted: page=" << (*i)->getPage() << " ps=" << (*i)->getPS() << " ss=" << (*i)->getSS() << endl;
		}
	}
	else
	{
		cout << "       not slotted." << endl;
	}
	for (list<Parameter*>::const_iterator i=parameters.begin(); i != parameters.end(); i++)
	{
		cout << "       parameter name='" << (*i)->name << "' value='" << (*i)->value << "' class='" << (*i)->clasz << "'" << endl;
	}
}

MSXConfig::Device::Parameter::Parameter(const string &nam, const string &valu, const string &clas=""):name(nam),value(valu),clasz(clas)
{
}

list<const MSXConfig::Device::Parameter*> MSXConfig::Device::getParametersWithClass(const string &clasz)
{
	list<const Parameter*> a;
	for (list <Parameter*>::const_iterator i=parameters.begin();i!=parameters.end();i++)
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
