#ifndef CASSETTEPORT_HH
#define CASSETTEPORT_HH

#include "Connector.hh"
#include "CassettePlayerCommand.hh"
#include "serialize_meta.hh"
#include "components.hh"

namespace openmsx {

class HardwareConfig;
class MSXMotherBoard;
class CassetteDevice;
class CassettePlayer;
#if COMPONENT_LASERDISC
class LaserdiscPlayer;
#endif

class CassettePortInterface
{
public:
	virtual ~CassettePortInterface() = default;

	/**
	* Sets the cassette motor relay
	*  false = off   true = on
	*/
	virtual void setMotor(bool status, EmuTime::param time) = 0;

	/**
	* Writes one bit to the cassette port.
	* From the RedBook:
	*   The CasOut bit is filtered and attenuated before being
	*   taken to the cassette DIN socket as the MIC signal. All
	*   cassette tone generation is performed in software.
	*/
	virtual void cassetteOut(bool output, EmuTime::param time) = 0;

	/**
	 * last bit written to CasOut.
	 * for use in Pluggable::plugHelper()
	 */
	[[nodiscard]] virtual bool lastOut() const = 0;

	/**
	* Reads one bit from the cassette port.
	* From the RedBook:
	*   The cassette input is used to read the signal from the
	*   cassette EAR output. This is passed through a comparator
	*   to clean the edges and to convert to digital levels,
	*   but is otherwise unprocessed.
	*/
	virtual bool cassetteIn(EmuTime::param time) = 0;

#if COMPONENT_LASERDISC
	/**
	* Set the Laserdisc Player; when the motor control is off, sound
	* is read from the laserdisc.
	*/
	virtual void setLaserdiscPlayer(LaserdiscPlayer *laserdisc) = 0;
#endif

	/**
	* Get the cassette player (if available)
	*/
	virtual CassettePlayer* getCassettePlayer() = 0;
};

class CassettePort final : public CassettePortInterface, public Connector
{
public:
	explicit CassettePort(const HardwareConfig& hwConf);
	~CassettePort() override;
	void setMotor(bool status, EmuTime::param time) override;
	void cassetteOut(bool output, EmuTime::param time) override;
	bool cassetteIn(EmuTime::param time) override;
#if COMPONENT_LASERDISC
	void setLaserdiscPlayer(LaserdiscPlayer* laserdisc) override;
#endif
	[[nodiscard]] bool lastOut() const override;
	CassettePlayer* getCassettePlayer() override { return cassettePlayer; }

	// Connector
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] std::string_view getClass() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] CassetteDevice& getPluggedCasDev() const;

	MSXMotherBoard& motherBoard;

#if COMPONENT_LASERDISC
	LaserdiscPlayer* laserdiscPlayer = nullptr;
#endif
	CassettePlayer* cassettePlayer;

	bool lastOutput = false;
	bool motorControl = false;
};
SERIALIZE_CLASS_VERSION(CassettePort, 2);

class DummyCassettePort final : public CassettePortInterface
{
public:
	explicit DummyCassettePort(MSXMotherBoard& motherBoard);
	void setMotor(bool status, EmuTime::param time) override;
	void cassetteOut(bool output, EmuTime::param time) override;
	bool cassetteIn(EmuTime::param time) override;
#if COMPONENT_LASERDISC
	void setLaserdiscPlayer(LaserdiscPlayer *laserdisc) override;
#endif
	[[nodiscard]] bool lastOut() const override;
	CassettePlayer* getCassettePlayer() override { return nullptr; }
private:
	CassettePlayerCommand cassettePlayerCommand;
};

} // namespace openmsx

#endif
