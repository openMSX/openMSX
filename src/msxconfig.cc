// $Id$

#include "msxconfig.hh"

// you need libxml++ 0.13 and libxml2 > 2.0.0
#include <xml++.h>

// for atoi()
#include <stdlib.h>

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
	for (XMLNodeList::iterator i(children.begin()); i != children.end(); i++)
	{
		// this list should only contain device nodes for now
		if (!(*i)->is_content())
		{
			deviceList.push_back(new Device(*i));
		}
	}
}

MSXConfig::Device::~Device()
{
}

MSXConfig::Device::Device(XMLNode *deviceNodeP):deviceNode(deviceNodeP)
{
	cerr << "Creating " << deviceNode->name() << " : ";
	cerr << "	id=" << deviceNode->property("id")->value() << endl;

	// TODO!! do basic validity checking of XMLTree
}

const string &MSXConfig::Device::getType()
{
	// mandatory first child
	// TODO add checks ... also for next functions!!
	return (*(deviceNode->children().begin()))->content();
}

const string &MSXConfig::Device::getId()
{
	return deviceNode->property("id")->value();
}

bool MSXConfig::Device::isSlotted()
{
	// if there, it is the second child
	return (*(deviceNode->children().begin()++))->name()=="slotted";
}

int MSXConfig::Device::getPage()
{
	// TODO: clean + exceptions
	if (isSlotted())
		return atoi((*(*(deviceNode->children().begin()++))->children().begin())->content().c_str());
	return -1;
}

int MSXConfig::Device::getPS()
{
	// TODO: clean + exceptions
	if (isSlotted())
		return atoi((*(*((deviceNode->children().begin()++)++))->children().begin())->content().c_str());
	return -1;
}

int MSXConfig::Device::getSS()
{
	// TODO: clean + exceptions
	if (isSlotted())
		return atoi((*(*(((deviceNode->children().begin()++)++)++))->children().begin())->content().c_str());
	return -1;
}

bool MSXConfig::Device::hasParameter(const string &name)
{
}

const string &MSXConfig::Device::getParameter(const string &name)
{
}
