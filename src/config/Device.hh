// $Id$

#ifndef __DEVICE_HH__
#define __DEVICE_HH__

#include "Config.hh"

namespace openmsx {

class Device : public Config
{
public:
	// a slotted is the same for all backends:
	class Slotted {
	public:
		Slotted(int PS, int SS, int Page);
		~Slotted();
		
		int getPS() const;
		int getSS() const;
		int getPage() const;

	private:
		int ps;
		int ss;
		int page;
	};

	Device(XML::Element* element, FileContext& context);
	~Device();

	list<Slotted*> slotted;
};

} // namespace openmsx

#endif
