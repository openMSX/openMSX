// $Id$

#ifndef __CASSETTEDEVICE_HH__
#define __CASSETTEDEVICE_HH__

#include "Pluggable.hh"


class CassetteDevice : public Pluggable
{
	public:
		/**
		 * Sets the cassette motor relay
		 *  false = off   true = on
		 */
		virtual void setMotor(bool status, const EmuTime &time) = 0;
		
		/**
		 * Read wave data from cassette device
		 */
		virtual short readSample(const EmuTime &time) = 0;
		
		/**
		 * Write wave data to cassette device
		 */
		virtual void writeWave(short *buf, int length) = 0;
		
		/**
		 * Returns the sample rate of the writeWave() wave form.
		 * A sample rate of 0 means this CassetteDevice is not interested
		 * in writeWave() data (iow is not recording).
		 */
		virtual int getWriteSampleRate() = 0;

		// Pluggable
		virtual const std::string &getClass() const;
};

#endif
