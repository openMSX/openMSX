#ifndef RS232CONNECTOR_HH
#define RS232CONNECTOR_HH

#include "Connector.hh"
#include "SerialDataInterface.hh"
#include "serialize_meta.hh"

namespace openmsx {

class RS232Device;

class RS232Connector : public Connector, public SerialDataInterface
{
public:
	RS232Device& getPluggedRS232Dev() const;

	// Connector
	std::string_view getDescription() const final override;
	std::string_view getClass() const final override;

	// input (SerialDataInterface)
	void setDataBits(DataBits bits) override = 0;
	void setStopBits(StopBits bits) override = 0;
	void setParityBit(bool enable, ParityBit parity) override = 0;
	void recvByte(byte value, EmuTime::param time) override = 0;
	virtual bool ready() = 0;
	virtual bool acceptsData() = 0;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	RS232Connector(PluggingController& pluggingController,
	               std::string name);
	~RS232Connector() = default;
};

REGISTER_BASE_CLASS(RS232Connector, "rs232connector");

} // namespace openmsx

#endif
