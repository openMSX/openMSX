// $Id$

#include "msxconfig.hh"

// you need libxml++ 0.13 and libxml2 > 2.0.0
#include <xml++.h>

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

bool MSXConfig::loadFile(const string &filename)
{
	tree = new XMLTree();
	if (!tree->read(filename))
		return false;
	//cout << tree->write_buffer() << endl;

	XMLNodeList children=tree->root()->children();
	for (XMLNodeList::iterator i(children.begin()); i != children.end(); i++)
	{
		// this list should only contain device nodes for now
		if (!(*i)->is_content())
		{
			deviceList.push_back(new Device(*i));
		}
	}
	return true;
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
}

const string &MSXConfig::Device::getId()
{
	return deviceNode->property("id")->value();
}

bool MSXConfig::Device::isSlotted()
{
}

int MSXConfig::Device::getPage()
{
}

int MSXConfig::Device::getPS()
{
}

int MSXConfig::Device::getSS()
{
}

bool MSXConfig::Device::hasParameter(const string &name)
{
}

const string &MSXConfig::Device::getParameter(const string &name)
{
}
