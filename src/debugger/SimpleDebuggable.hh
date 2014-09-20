#ifndef SIMPLEDEBUGGABLE_HH
#define SIMPLEDEBUGGABLE_HH

#include "Debuggable.hh"
#include "EmuTime.hh"

namespace openmsx {

class MSXMotherBoard;

class SimpleDebuggable : public Debuggable
{
public:
	unsigned getSize() const final override;
	const std::string& getDescription() const final override;

	byte read(unsigned address) override;
	virtual byte read(unsigned address, EmuTime::param time);
	void write(unsigned address, byte value) override;
	virtual void write(unsigned address, byte value, EmuTime::param time);

	const std::string& getName() const { return name; }
	MSXMotherBoard& getMotherBoard() const { return motherBoard; }

protected:
	SimpleDebuggable(MSXMotherBoard& motherBoard, const std::string& name,
	                 const std::string& description, unsigned size);
	~SimpleDebuggable();

private:
	MSXMotherBoard& motherBoard;
	const std::string name;
	const std::string description;
	const unsigned size;
};

} // namespace openmsx

#endif
