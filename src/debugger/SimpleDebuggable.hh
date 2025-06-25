#ifndef SIMPLEDEBUGGABLE_HH
#define SIMPLEDEBUGGABLE_HH

#include "Debuggable.hh"
#include "EmuTime.hh"
#include "static_string_view.hh"

namespace openmsx {

class MSXMotherBoard;

class SimpleDebuggable : public Debuggable
{
public:
	[[nodiscard]] unsigned getSize() const final;
	[[nodiscard]] std::string_view getDescription() const final;

	[[nodiscard]] uint8_t read(unsigned address) override;
	[[nodiscard]] virtual uint8_t read(unsigned address, EmuTime time);
	void write(unsigned address, uint8_t value) override;
	virtual void write(unsigned address, uint8_t value, EmuTime time);

	[[nodiscard]] const std::string& getName() const { return name; }
	[[nodiscard]] MSXMotherBoard& getMotherBoard() const { return motherBoard; }

protected:
	SimpleDebuggable(MSXMotherBoard& motherBoard, std::string name,
	                 static_string_view description, unsigned size);
	~SimpleDebuggable();

private:
	MSXMotherBoard& motherBoard;
	const std::string name;
	const static_string_view description;
	const unsigned size;
};

} // namespace openmsx

#endif
