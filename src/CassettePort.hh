// $Id$

#ifndef __MSXCASSETTEPORT_HH__
#define __MSXCASSETTEPORT_HH__

#include "MSXDevice.hh"
#include "CassetteDevice.hh"
#include "EmuTime.hh"


class CassettePortInterface
{
	public:
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
		 * Plugs a given CassetteDevice in this CassettePort
		 */
		void plug(CassetteDevice *device);

		/**
		 * Unplugs a CassetteDevice from this CassettePort
		 */
		void unplug();

		/**
		 * Writes all buffered data to CassetteDevice.
		 *  A CassettePort may write data anytime it wants to, but a
		 *  CassetteDevice may ask for an 'early' write because for 
		 *  example it wants to remove the tape.
		 */
		virtual void flushOutput(const EmuTime &time) = 0;

	protected:
		CassetteDevice *device;
};

class CassettePort : public CassettePortInterface
{
	public:
		CassettePort();
		virtual ~CassettePort();
		void setMotor(bool status, const EmuTime &time);
		void cassetteOut(bool output, const EmuTime &time);
		bool cassetteIn(const EmuTime &time);
		void flushOutput(const EmuTime &time);
	private:
		static const int BUFSIZE = 256;
		
		bool lastOutput;
		short nextSample;
		EmuTime prevTime;
		short *buffer;
};

class DummyCassettePort : public CassettePortInterface
{
	public:
		DummyCassettePort();
		void setMotor(bool status, const EmuTime &time);
		void cassetteOut(bool output, const EmuTime &time);
		bool cassetteIn(const EmuTime &time);
		void flushOutput(const EmuTime &time);
};


class CassettePortFactory
{
	public:
		/**
		 * If an Cassette was specified in the conig file this method 
		 * returns an CassettePort object, otherwise an DummyCassette
		 * object
		 */
		static CassettePortInterface *instance();
		
	private:
		static CassettePortInterface *oneInstance;
};


#endif
