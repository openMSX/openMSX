// $Id$

#ifndef CASSETTEPORT_HH
#define CASSETTEPORT_HH

#include "Connector.hh"
#include "CassetteDevice.hh"
#include "EmuTime.hh"
#include "components.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class CassettePlayer;
#ifdef COMPONENT_JACK
class CassetteJack;
#endif
class PluggingController;

class CassettePortInterface : public Connector
{
public:
	CassettePortInterface();

	/**
	* Sets the cassette motor relay
	*  false = off   true = on
	*/
	virtual void setMotor(bool status, const EmuTime& time) = 0;

	/**
	* Writes one bit to the cassette port.
	* From the RedBook:
	*   The CasOut bit is filtered and attenuated before being
	*   taken to the cassette DIN socket as the MIC signal. All
	*   cassette tone generation is performed in software.
	*/
	virtual void cassetteOut(bool output, const EmuTime& time) = 0;

	/**
	 * last bit written to CasOut.
	 * for use in Pluggable::plugHelper()
	 */
	virtual bool lastOut() const = 0;

	/**
	* Reads one bit from the cassette port.
	* From the RedBook:
	*   The cassette input is used to read the signal from the
	*   cassette EAR output. This is passed through a comparator
	*   to clean the edges and to convert to digital levels,
	*   but is otherwise unprocessed.
	*/
	virtual bool cassetteIn(const EmuTime& time) = 0;

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;
	virtual void unplug(const EmuTime& time);
	virtual CassetteDevice& getPlugged() const;
};

class CassettePort : public CassettePortInterface
{
public:
	explicit CassettePort(MSXMotherBoard& motherBoard);
	virtual ~CassettePort();
	virtual void setMotor(bool status, const EmuTime& time);
	virtual void cassetteOut(bool output, const EmuTime& time);
	virtual bool cassetteIn(const EmuTime& time);
	virtual bool lastOut() const;
private:
	PluggingController& pluggingController;
	bool lastOutput;
	short nextSample;
	EmuTime prevTime;

	std::auto_ptr<CassettePlayer> cassettePlayer;
#ifdef COMPONENT_JACK
	std::auto_ptr<CassetteJack> cassetteJack;
#endif
};

class DummyCassettePort : public CassettePortInterface
{
public:
	DummyCassettePort();
	virtual void setMotor(bool status, const EmuTime& time);
	virtual void cassetteOut(bool output, const EmuTime& time);
	virtual bool cassetteIn(const EmuTime& time);
	virtual bool lastOut() const;
};

} // namespace openmsx

#endif
