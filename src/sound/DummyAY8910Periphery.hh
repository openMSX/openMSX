#ifndef DUMMYAY8910PERIPHERY_HH
#define DUMMYAY8910PERIPHERY_HH

#include "AY8910Periphery.hh"

namespace openmsx {

class DummyAY8910Periphery final : public AY8910Periphery
{
public:
	static DummyAY8910Periphery& instance()
	{
		static DummyAY8910Periphery oneInstance;
		return oneInstance;
	}

	byte readA(EmuTime::param /*time*/) override { return 255; }
	byte readB(EmuTime::param /*time*/) override { return 255; }
	void writeA(byte /*value*/, EmuTime::param /*time*/) override {}
	void writeB(byte /*value*/, EmuTime::param /*time*/) override {}

private:
	DummyAY8910Periphery() {}
	~DummyAY8910Periphery() {}
};

}; // namespace openmsx

#endif
