// $Id$

#ifndef __Y8950KEYBOARDCONNECTOR_HH__
#define __Y8950KEYBOARDCONNECTOR_HH__

#include "Y8950KeyboardDevice.hh"
#include "Connector.hh"


class DummyY8950KeyboardDevice : public Y8950KeyboardDevice
{
	virtual void write(byte data, const EmuTime &time);
	virtual byte read(const EmuTime &time);
	virtual const std::string &getName();
	static const std::string name;
};


class Y8950KeyboardConnector : public Connector
{
	public:
		Y8950KeyboardConnector(const EmuTime &time);
		virtual ~Y8950KeyboardConnector();

		void write(byte data, const EmuTime &time);
		byte read(const EmuTime &time);

		// Connector
		virtual const std::string &getName();
		virtual const std::string &getClass();
		virtual void plug(Pluggable *dev, const EmuTime &time);
		virtual void unplug(const EmuTime &time);

	private:
		DummyY8950KeyboardDevice *dummy;
		
		byte data;

		static const std::string name;
		static const std::string className;
};

#endif
