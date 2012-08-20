// $Id$

#ifndef SIMPLEDEBUGGABLE_HH
#define SIMPLEDEBUGGABLE_HH

#include "Debuggable.hh"
#include "EmuTime.hh"

namespace openmsx {

class MSXMotherBoard;

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
	virtual byte read(unsigned address, EmuTime::param time);
	virtual void write(unsigned address, byte value);
	virtual void write(unsigned address, byte value, EmuTime::param time);

	MSXMotherBoard& getMotherBoard() const { return motherBoard; }

private:
	MSXMotherBoard& motherBoard;
	const std::string name;
	const std::string description;
	const unsigned size;
};

} // namespace openmsx

#endif
