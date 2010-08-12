// $Id$

#ifndef RAM_HH
#define RAM_HH

#include "MemBuffer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class RamDebuggable;

class Ram : private noncopyable
{
public:
	/** Create Ram object with an associated debuggable. */
	Ram(MSXMotherBoard& motherBoard, const std::string& name,
	    const std::string& description, unsigned size);

	/** Create Ram object without debuggable. */
	explicit Ram(unsigned size);

	~Ram();

	const byte& operator[](unsigned addr) const {
		return ram[addr];
	}
	byte& operator[](unsigned addr) {
		return ram[addr];
	}
	unsigned getSize() const {
		return ram.size();
	}

	const std::string& getName() const;
	void clear();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MemBuffer<byte> ram;
	const std::auto_ptr<RamDebuggable> debuggable;
};

} // namespace openmsx

#endif
