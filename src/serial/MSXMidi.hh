#ifndef MSXMIDI_HH
#define MSXMIDI_HH

#include "MSXDevice.hh"
#include "IRQHelper.hh"
#include "MidiInConnector.hh"
#include "I8251.hh"

namespace openmsx {

class I8254;
class MidiOutConnector;

class MSXMidi final : public MSXDevice, public MidiInConnector
{
public:
	explicit MSXMidi(const DeviceConfig& config);
	~MSXMidi();

	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	// MidiInConnector
	bool ready() override;
	bool acceptsData() override;
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, ParityBit parity) override;
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
	void registerRange(byte port, unsigned num);
	void unregisterRange(byte port, unsigned num);

	class Counter0 final : public ClockPinListener {
	public:
		explicit Counter0(MSXMidi& midi);
		void signal(ClockPin& pin, EmuTime::param time) override;
		void signalPosEdge(ClockPin& pin, EmuTime::param time) override;
	private:
		MSXMidi& midi;
	} cntr0; // counter 0 clock pin

	class Counter2 final : public ClockPinListener {
	public:
		explicit Counter2(MSXMidi& midi);
		void signal(ClockPin& pin, EmuTime::param time) override;
		void signalPosEdge(ClockPin& pin, EmuTime::param time) override;
	private:
		MSXMidi& midi;
	} cntr2; // counter 2 clock pin

	class I8251Interf final : public I8251Interface {
	public:
		explicit I8251Interf(MSXMidi& midi);
		void setRxRDY(bool status, EmuTime::param time) override;
		void setDTR(bool status, EmuTime::param time) override;
		void setRTS(bool status, EmuTime::param time) override;
		bool getDSR(EmuTime::param time) override;
		bool getCTS(EmuTime::param time) override;
		void setDataBits(DataBits bits) override;
		void setStopBits(StopBits bits) override;
		void setParityBit(bool enable, ParityBit parity) override;
		void recvByte(byte value, EmuTime::param time) override;
		void signal(EmuTime::param time) override;
	private:
		MSXMidi& midi;
	} interf;

	IRQHelper timerIRQ;
	IRQHelper rxrdyIRQ;
	bool timerIRQlatch;
	bool timerIRQenabled;
	bool rxrdyIRQlatch;
	bool rxrdyIRQenabled;

	const bool isExternalMSXMIDI;
	bool isEnabled; /* EN bit */
	bool isLimitedTo8251; /* inverse of E8 bit */

	// must come last
	const std::unique_ptr<MidiOutConnector> outConnector;
	const std::unique_ptr<I8251> i8251;
	const std::unique_ptr<I8254> i8254;
};
SERIALIZE_CLASS_VERSION(MSXMidi, 2);

} // namespace openmsx

#endif
