// $Id$

#include "Device.hh"

namespace openmsx {

// class Device

Device::Device(XML::Element* element_, FileContext& context_)
	: Config(element_, context_)
{
	// TODO: create slotted-eds ???
	for (list<XML::Element*>::iterator i = element->children.begin();
	     i != element->children.end(); ++i) {
		if ((*i)->name == "slotted") {
			int PS = -2;
			int SS = -1;
			int Page = -1;
			for (list<XML::Element*>::iterator j = (*i)->children.begin();
			     j != (*i)->children.end(); ++j) {
				if ((*j)->name == "ps") {
					PS = Config::Parameter::stringToInt((*j)->pcdata);
				} else if ((*j)->name == "ss") {
					SS = Config::Parameter::stringToInt((*j)->pcdata);
				} else if ((*j)->name == "page") {
					Page = Config::Parameter::stringToInt((*j)->pcdata);
				}
			}
			if (PS != -2) {
				slotted.push_back(new Device::Slotted(PS,SS,Page));
			}
		}
	}
}

Device::Device(const string& type, const string& id)
	: Config(type, id)
{
}

Device::~Device()
{
}

// class Slotted

Device::Slotted::Slotted(int PS, int SS, int Page)
	: ps(PS), ss(SS), page(Page)
{
}

Device::Slotted::~Slotted()
{
}

int Device::Slotted::getPS() const
{
	return ps;
}

int Device::Slotted::getSS() const
{
	return ss;
}

int Device::Slotted::getPage() const
{
	return page;
}

} // namespace openmsx
