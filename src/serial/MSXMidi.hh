#ifndef MSXMIDI_HH
#define MSXMIDI_HH

#include "MSXDevice.hh"
#include "IRQHelper.hh"
#include "MidiInConnector.hh"
#include "MidiOutConnector.hh"
#include "I8251.hh"
#include "I8254.hh"

namespace openmsx {

class MSXMidi final : public MSXDevice, public MidiInConnector
{
public:
	explicit MSXMidi(const DeviceConfig& config);
	~MSXMidi() override;

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
	void setTimerIRQ(bool status, EmuTime::param time);
	void enableTimerIRQ(bool enabled, EmuTime::param time);
	void updateEdgeEvents(EmuTime::param time);
	void setRxRDYIRQ(bool status);
	void enableRxRDYIRQ(bool enabled);

	void registerIOports(byte value);

private:
	struct Counter0 final : ClockPinListener {
		void signal(ClockPin& pin, EmuTime::param time) override;
		void signalPosEdge(ClockPin& pin, EmuTime::param time) override;
	} cntr0; // counter 0 clock pin

	struct Counter2 final : ClockPinListener {
		void signal(ClockPin& pin, EmuTime::param time) override;
		void signalPosEdge(ClockPin& pin, EmuTime::param time) override;
	} cntr2; // counter 2 clock pin

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

	IRQHelper timerIRQ;
	IRQHelper rxrdyIRQ;
	bool timerIRQlatch = false;
	bool timerIRQenabled = false;
	bool rxrdyIRQlatch = false;
	bool rxrdyIRQenabled = false;

	const bool isExternalMSXMIDI;
	bool isEnabled; /* EN bit */
	bool isLimitedTo8251 = true; /* inverse of E8 bit */

	// must come last
	MidiOutConnector outConnector;
	I8251 i8251;
	I8254 i8254;
};
SERIALIZE_CLASS_VERSION(MSXMidi, 2);

} // namespace openmsx

#endif
