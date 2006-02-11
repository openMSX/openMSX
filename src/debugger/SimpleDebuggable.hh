// $Id$

#ifndef SIMPLEDEBUGGABLE_HH
#define SIMPLEDEBUGGABLE_HH

#include "Debuggable.hh"

namespace openmsx {

class MSXMotherBoard;
class EmuTime;

class SimpleDebuggable : public Debuggable
{
public:
	SimpleDebuggable(MSXMotherBoard& motherBoard, const std::string& name,
	                 const std::string& description, unsigned size);
	virtual ~SimpleDebuggable();

	virtual unsigned getSize() const;
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;

	virtual byte read(unsigned address);
	virtual byte read(unsigned address, const EmuTime& time);
	virtual void write(unsigned address, byte value);
	virtual void write(unsigned address, byte value, const EmuTime& time);

private:
	MSXMotherBoard& motherBoard;
	const std::string name;
	const std::string description;
	unsigned size;
};

} // namespace openmsx

#endif
