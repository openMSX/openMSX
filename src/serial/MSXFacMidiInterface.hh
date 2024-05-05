#ifndef MSXFACMIDIINTERFACE_HH
#define MSXFACMIDIINTERFACE_HH

#include "MSXDevice.hh"
#include "MidiInConnector.hh"
#include "MidiOutConnector.hh"
#include "I8251.hh"

namespace openmsx {

class MSXFacMidiInterface final : public MSXDevice, public MidiInConnector
{
public:
	explicit MSXFacMidiInterface(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	// MidiInConnector
	[[nodiscard]] bool ready() override;
	[[nodiscard]] bool acceptsData() override;
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, Parity parity) override;
	void recvByte(byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct Interface final : I8251Interface {
		void setRxRDY(bool status, EmuTime::param time) override;
		void setDTR(bool status, EmuTime::param time) override;
		void setRTS(bool status, EmuTime::param time) override;
		[[nodiscard]] bool getDSR(EmuTime::param time) override;
		[[nodiscard]] bool getCTS(EmuTime::param time) override;
		void setDataBits(DataBits bits) override;
		void setStopBits(StopBits bits) override;
		void setParityBit(bool enable, Parity parity) override;
		void recvByte(byte value, EmuTime::param time) override;
		void signal(EmuTime::param time) override;
	} interface;

	// must come last
	MidiOutConnector outConnector;
	I8251 i8251;
};

} // namespace openmsx

#endif
