// $Id: MSXCASSETTE.hh,v

#ifndef __MSXCASSETTEPORT_HH__
#define __MSXCASSETTEPORT_HH__

#include "MSXDevice.hh"
#include "CassetteDevice.hh"
#include "emutime.hh"


class CassettePortInterface
{
	public:
		/**
		 * Constructor
		 */
		CassettePortInterface();

		/**
		 * Sets the casette motor relay
		 *  false = off   true = on
		 */
		virtual void setMotor(bool status, const Emutime &time) = 0;

		/**
		 * Writes one bit to the cassette port.
		 * From the RedBook:
		 *   The CasOut bit is filtered and attenuated before being
		 *   taken to the cassette DIN socket as the MIC signal. All
		 *   cassette tone generation is performed in software.
		 */
		virtual void cassetteOut(bool output, const Emutime &time) = 0;

		/**
		 * Reads one bit from the cassette port.
		 * From the RedBook:
		 *   The cassette input is used to read the signal from the 
		 *   cassette EAR output. This is passed through a comparator
		 *   to clean the edges and to convert to digital levels,
		 *   but is otherwise unprocessed.
		 */
		virtual bool cassetteIn(const Emutime &time) = 0;


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
		virtual void flushOutput(const Emutime &time) = 0;

	protected:
		CassetteDevice *device;
};

class CassettePort : public CassettePortInterface
{
	public:
		CassettePort();
		virtual ~CassettePort();
		void setMotor(bool status, const Emutime &time);
		void cassetteOut(bool output, const Emutime &time);
		bool cassetteIn(const Emutime &time);
		void flushOutput(const Emutime &time);
	private:
		static const int BUFSIZE = 256;
		
		bool lastOutput;
		short nextSample;
		Emutime prevTime;
		short *buffer;
};

class DummyCassettePort : public CassettePortInterface
{
	public:
		void setMotor(bool status, const Emutime &time);
		void cassetteOut(bool output, const Emutime &time);
		bool cassetteIn(const Emutime &time);
		void flushOutput(const Emutime &time);
};


class MSXCassettePort : public MSXDevice
{
	public:
		/**
		 * Constructor.
		 * This is a singleton class. Constructor can only be used once
		 * by the class devicefactory.
		 */
		MSXCassettePort(MSXConfig::Device *config); 

		/**
		 * Destructor
		 */
		~MSXCassettePort(); 

		/**
		 * If an MSXCassette was specified in the conig file (constructor
		 * was called) this method returns an CassettePort object, 
		 * otherwise an DummyCassette object
		 */
		static CassettePortInterface *instance();
		
	private:
		static bool constructed;
		static CassettePortInterface *oneInstance;
};


#endif
