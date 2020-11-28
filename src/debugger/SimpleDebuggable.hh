#ifndef SIMPLEDEBUGGABLE_HH
#define SIMPLEDEBUGGABLE_HH

#include "Debuggable.hh"
#include "EmuTime.hh"

namespace openmsx {

class MSXMotherBoard;

class SimpleDebuggable : public Debuggable
{
public:
	[[nodiscard]] unsigned getSize() const final;
	[[nodiscard]] const std::string& getDescription() const final;

	[[nodiscard]] byte read(unsigned address) override;
	[[nodiscard]] virtual byte read(unsigned address, EmuTime::param time);
	void write(unsigned address, byte value) override;
	virtual void write(unsigned address, byte value, EmuTime::param time);

	[[nodiscard]] const std::string& getName() const { return name; }
	[[nodiscard]] MSXMotherBoard& getMotherBoard() const { return motherBoard; }

protected:
	SimpleDebuggable(MSXMotherBoard& motherBoard, std::string name,
	                 std::string description, unsigned size);
	~SimpleDebuggable();

private:
	MSXMotherBoard& motherBoard;
	const std::string name;
	const std::string description;
	const unsigned size;
};

} // namespace openmsx

#endif
