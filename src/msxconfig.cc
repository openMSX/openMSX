// $Id$

#include "msxconfig.hh"

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
	list<string*>::iterator i=parameter_names.begin();
	list<string*>::iterator j=parameter_values.begin();
	for (; i != parameter_names.end(); i++,j++)
	{
		delete *i;
		delete *j;
	}
}

// TODO !!! content nodes work slightly different then I thought
MSXConfig::Device::Device(XMLNode *deviceNodeP):deviceNode(deviceNodeP),page(0),ps(0),ss(0),slotted(false)
{
	char buffer[200];

//  <device id="mydummy1">
	// check if it is a devicenode
	if (deviceNode->name()!="device")
		throw MSXConfig::XMLParseException("Expected <device> node.");
	if (deviceNode->property("id")==0)
		throw MSXConfig::XMLParseException("<device> node is missing mandatory 'id' property.");
	id = deviceNode->property("id")->value();
//    <type>DummyDevice</type>
	// check if first child is mandatory type node
	XMLNodeList::size_type csize;
	if ((csize=deviceNode->children().size())<1)
	{
		snprintf(buffer,200,"Expecting at least one child node for <device id='%s'>.", id.c_str());
		throw MSXConfig::XMLParseException(buffer);
	}
	XMLNodeList::const_iterator dci = deviceNode->children().begin();
	if ((*dci)->name()!="type")
	{
		snprintf(buffer,200,"Missing mandatory first child node <type> for <device id='%s'>.", id.c_str());
		throw MSXConfig::XMLParseException(buffer);
	}
	if ((*dci)->children().size()!=1)
	{
		snprintf(buffer,200,"Missing content node for <type> for <device id='%s'>.", id.c_str());
		throw MSXConfig::XMLParseException(buffer);
	}
	XMLNodeList::const_iterator cn = (*dci)->children().begin();
	if (!((*cn)->is_content()))
	{
		snprintf(buffer,200,"Child node of <type> for <device id='%s'> is not a content node.", id.c_str());
		throw MSXConfig::XMLParseException(buffer);
	}
	deviceType = ((*cn)->content());
	if (csize==1)
		return;
	// else: either a slotted structure, or the first parameter
	dci++;
	if ((*dci)->name()!="slotted" && (*dci)->name()!="parameter")
	{
		snprintf(buffer,200,"Either <slotted> or <parameter> expected as second child node for <device id='%s'>.", id.c_str());
		throw MSXConfig::XMLParseException(buffer);
	}
	if ((*dci)->name()=="slotted")
	{
		slotted = true;
		if ((*dci)->children().size()!=3)
		{
			snprintf(buffer,200,"Expected 3 child nodes for <slotted> node in <device id='%s'>.", id.c_str());
			throw MSXConfig::XMLParseException(buffer);
		}
		XMLNodeList::const_iterator sli = (*dci)->children().begin();
		if ((*sli)->name()!="page")
		{
			snprintf(buffer,200,"Expected <page> as first child node for <slotted> node in <device id='%s'>.", id.c_str());
			throw MSXConfig::XMLParseException(buffer);
		}
		page=atoi((*sli)->content().c_str());
		sli++;
		if ((*sli)->name()!="ps")
		{
			snprintf(buffer,200,"Expected <ps> as second child node for <slotted> node in <device id='%s'>.", id.c_str());
			throw MSXConfig::XMLParseException(buffer);
		}
		ps=atoi((*sli)->content().c_str());
		sli++;
		if ((*sli)->name()!="ss")
		{
			snprintf(buffer,200,"Expected <ss> as third child node for <slotted> node in <device id='%s'>.", id.c_str());
			throw MSXConfig::XMLParseException(buffer);
		}
		ss=atoi((*sli)->content().c_str());
		dci++;
	}
//        <slotted>
//          <page>0</page>
//          <ps>0</ps>
//          <ss>0</ss>
//        </slotted>
	// the rest are parameters
	while (dci!=deviceNode->children().end())
	{
		if ((*dci)->name()!="parameter")
		{
			snprintf(buffer,200,"Expected <parameter> as child node in <device id='%s'>.", id.c_str());
			throw MSXConfig::XMLParseException(buffer);
		}
		if ((*dci)->property("name")==0)
		{
			snprintf(buffer,200,"Expected mandatory 'name' property in <parameter> child node in <device id='%s'>.", id.c_str());
			throw MSXConfig::XMLParseException(buffer);
		}
		string *name=new string((*dci)->property("name")->value());
		//if (!(*dci)->is_content())
		//{
		//	snprintf(buffer,200,"Expected content in <parameter name='%s'> as child node in <device id='%s'>.", name->c_str(), id.c_str());
		//	throw MSXConfig::XMLParseException(buffer);
		//}
		string *value=new string((*dci)->content());
		parameter_names.push_back(name);
		parameter_values.push_back(value);
		dci++;
	}
//        <parameter name="hello">world</parameter>
//        <parameter name="hi">alpha centauri</parameter>
//  </device>

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
	return slotted;
}

int MSXConfig::Device::getPage()
{
	return page;
}

int MSXConfig::Device::getPS()
{
	return ps;
}

int MSXConfig::Device::getSS()
{
	return ss;
}

bool MSXConfig::Device::hasParameter(const string &name)
{
	for (list<string*>::iterator i=parameter_names.begin(); i != parameter_names.end(); i++)
	{
		if ((*(*i))==name)
			return true;
	}
	return false;
}

const string &MSXConfig::Device::getParameter(const string &name)
{
	char buffer[200];
	list<string*>::iterator i=parameter_names.begin();
	list<string*>::iterator j=parameter_values.begin();
	for (; i != parameter_names.end(); i++,j++)
	{
		if ((*(*i))==name)
			return (*(*j));
	}
	snprintf(buffer,200,"Trying to get non-existing parameter %s for device %s.", name.c_str(), id.c_str());
	throw Exception(buffer);
}

void MSXConfig::Device::dump()
{
	cout << "Device id='" << getId() << "', type='" << getType() << "'" << endl;
	if (isSlotted())
	{
		cout << "       slotted: page=" << getPage() << " ps=" << getPS() << " ss=" << getSS() << endl;
	}
	else
	{
		cout << "       not slotted." << endl;
	}
	list<string*>::iterator i=parameter_names.begin();
	list<string*>::iterator j=parameter_values.begin();
	for (; i != parameter_names.end(); i++,j++)
	{
		cout << "       parameter name='" << **i << "' value='" << **j << "'" << endl;
	}
}
