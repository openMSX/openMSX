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

	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	// MidiInConnector
	[[nodiscard]] bool ready() override;
	[[nodiscard]] bool acceptsData() override;
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, Parity parity) override;
	void recvByte(uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setTimerIRQ(bool status, EmuTime time);
	void enableTimerIRQ(bool enabled, EmuTime time);
	void updateEdgeEvents(EmuTime time);
	void setRxRDYIRQ(bool status);
	void enableRxRDYIRQ(bool enabled);

	void registerIOports(uint8_t value);

private:
	struct Counter0 final : ClockPinListener {
		void signal(ClockPin& pin, EmuTime time) override;
		void signalPosEdge(ClockPin& pin, EmuTime time) override;
	} cntr0; // counter 0 clock pin

	struct Counter2 final : ClockPinListener {
		void signal(ClockPin& pin, EmuTime time) override;
		void signalPosEdge(ClockPin& pin, EmuTime time) override;
	} cntr2; // counter 2 clock pin

	struct Interface final : I8251Interface {
		void setRxRDY(bool status, EmuTime time) override;
		void setDTR(bool status, EmuTime time) override;
		void setRTS(bool status, EmuTime time) override;
		[[nodiscard]] bool getDSR(EmuTime time) override;
		[[nodiscard]] bool getCTS(EmuTime time) override;
		void setDataBits(DataBits bits) override;
		void setStopBits(StopBits bits) override;
		void setParityBit(bool enable, Parity parity) override;
		void recvByte(uint8_t value, EmuTime time) override;
		void signal(EmuTime time) override;
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
