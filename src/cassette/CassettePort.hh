// $Id$

#ifndef __MSXCASSETTEPORT_HH__
#define __MSXCASSETTEPORT_HH__

#include "EmuTime.hh"
#include "Connector.hh"

// forward declaration
class CassetteDevice;
class DummyCassetteDevice;
class CassettePlayer;


class CassettePortInterface : public Connector
{
	public:
		CassettePortInterface(const EmuTime &time);
		virtual ~CassettePortInterface();
	
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
		virtual const std::string &getName();
		virtual const std::string &getClass();
		virtual void plug(Pluggable *dev, const EmuTime &time);
		virtual void unplug(const EmuTime &time);

	private:
		DummyCassetteDevice* dummy;
};

class CassettePort : public CassettePortInterface
{
	public:
		CassettePort(const EmuTime &time);
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

		CassettePlayer* player;
};

class DummyCassettePort : public CassettePortInterface
{
	public:
		DummyCassettePort(const EmuTime &time);
		virtual void setMotor(bool status, const EmuTime &time);
		virtual void cassetteOut(bool output, const EmuTime &time);
		virtual bool cassetteIn(const EmuTime &time);
		virtual void flushOutput(const EmuTime &time);
};


class CassettePortFactory
{
	public:
		/**
		 * If an Cassette was specified in the config file this method 
		 * returns n CassettePort object, otherwise a DummyCassette
		 * object
		 */
		static CassettePortInterface *instance(const EmuTime &time);
};


#endif
