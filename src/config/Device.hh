// $Id$

#ifndef __DEVICE_HH__
#define __DEVICE_HH__

#include <vector>
#include "Config.hh"

using std::vector;

namespace openmsx {

class Device : public Config
{
public:
	class Slot {
	public:
		Slot(int ps, int ss, int page);
		
		int getPS() const { return ps; }
		int getSS() const { return ss; }
		int getPage() const { return page; }

	private:
		int ps;
		int ss;
		int page;
	};

	Device(const XMLElement& element, const FileContext& context);
	~Device();

	typedef vector<Slot> Slots;
	const Slots& getSlots() const { return slots; }
	
private:
	vector<Slot> slots;
};

} // namespace openmsx

#endif
