// $Id$

#include "Device.hh"
#include "xmlx.hh"

namespace openmsx {

// class Device

Device::Device(const XMLElement& element, const FileContext& context)
	: Config(element, context)
{
	const XMLElement::Children& children = element.getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == "slotted") {
			int ps = -2;
			int ss = -1;
			int page = -1;
			const XMLElement::Children& slot_children = (*it)->getChildren();
			for (XMLElement::Children::const_iterator it2 = slot_children.begin();
			     it2 != slot_children.end(); ++it2) {
				if ((*it2)->getName() == "ps") {
					ps = stringToInt((*it2)->getPcData());
				} else if ((*it2)->getName() == "ss") {
					ss = stringToInt((*it2)->getPcData());
				} else if ((*it2)->getName() == "page") {
					page = stringToInt((*it2)->getPcData());
				}
			}
			if (ps != -2) {
				slots.push_back(Slot(ps, ss, page));
			}
		}
	}
}

Device::~Device()
{
}

// class Slot

Device::Slot::Slot(int PS, int SS, int Page)
	: ps(PS), ss(SS), page(Page)
{
}

} // namespace openmsx
