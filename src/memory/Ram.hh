// $Id$

#ifndef RAM_HH
#define RAM_HH

#include "Debuggable.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

class MSXMotherBoard;
class Debugger;

class Ram
{
public:
	Ram(MSXMotherBoard& motherBoard, const std::string& name,
	    unsigned size);
	Ram(MSXMotherBoard& motherBoard, const std::string& name,
	    const std::string& description, unsigned size);
	virtual ~Ram();

	const byte& operator[](unsigned addr) const {
		assert(addr < size);
		return ram[addr];
	}
	byte& operator[](unsigned addr) {
		assert(addr < size);
		return ram[addr];
	}
	unsigned getSize() const;
	void clear();

private:
	void init();
	
	class Debug : public Debuggable {
	public:
		Debug(Ram& parent);
		virtual unsigned getSize() const;
		virtual const std::string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		Ram& parent;
	} debug;

	Debugger& debugger;
	const std::string name;
	const std::string description;
	unsigned size;
	byte* ram;
};

} // namespace openmsx

#endif
