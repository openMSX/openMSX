// $Id$

#ifndef __CASSETTEPORT_HH__
#define __CASSETTEPORT_HH__

#include "EmuTime.hh"
#include "Connector.hh"

namespace openmsx {

class CassetteDevice;


class CassettePortInterface : public Connector {
public:
	CassettePortInterface();

	/**
	* Sets the casette motor relay
	*  false = off   true = on
	*/
	virtual void setMotor(bool status, const EmuTime &time) = 0;

	/**
	* Writes one bit to the cassette port.
	* From the RedBook:
	*   The CasOut bit is filtered and attenuated before being
	*   taken to the cassette DIN socket as the MIC signal. All
	*   cassette tone generation is performed in software.
	*/
	virtual void cassetteOut(bool output, const EmuTime &time) = 0;

	/**
	* Reads one bit from the cassette port.
	* From the RedBook:
	*   The cassette input is used to read the signal from the
	*   cassette EAR output. This is passed through a comparator
	*   to clean the edges and to convert to digital levels,
	*   but is otherwise unprocessed.
	*/
	virtual bool cassetteIn(const EmuTime &time) = 0;

	/**
	* Writes all buffered data to CassetteDevice.
	*  A CassettePort may write data anytime it wants to, but a
	*  CassetteDevice may ask for an 'early' write because for
	*  example it wants to remove the tape.
	*/
	virtual void flushOutput(const EmuTime &time) = 0;

	// Connector
	virtual const string& getDescription() const; 
	virtual const string& getClass() const;
	virtual void unplug(const EmuTime &time);
};

class CassettePort : public CassettePortInterface {
public:
	CassettePort();
	virtual ~CassettePort();
	virtual void setMotor(bool status, const EmuTime &time);
	virtual void cassetteOut(bool output, const EmuTime &time);
	virtual bool cassetteIn(const EmuTime &time);
	virtual void flushOutput(const EmuTime &time);
private:
	static const int BUFSIZE = 256;

	bool lastOutput;
	short nextSample;
	EmuTime prevTime;
	short *buffer;
};

class DummyCassettePort : public CassettePortInterface {
public:
	DummyCassettePort();
	virtual void setMotor(bool status, const EmuTime &time);
	virtual void cassetteOut(bool output, const EmuTime &time);
	virtual bool cassetteIn(const EmuTime &time);
	virtual void flushOutput(const EmuTime &time);
};


class CassettePortFactory {
public:
	/**
	* If an Cassette was specified in the config file this method
	* returns a CassettePort object, otherwise a DummyCassettePort
	* object
	*/
	static CassettePortInterface *instance();
};

} // namespace openmsx

#endif // __CASSETTEPORT_HH__
