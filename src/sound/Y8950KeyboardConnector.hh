// $Id$

#ifndef __Y8950KEYBOARDCONNECTOR_HH__
#define __Y8950KEYBOARDCONNECTOR_HH__

#include "Y8950KeyboardDevice.hh"
#include "Connector.hh"


namespace openmsx {

class DummyY8950KeyboardDevice : public Y8950KeyboardDevice {
public:
	virtual void write(byte data, const EmuTime& time);
	virtual byte read(const EmuTime& time);

	virtual const string& getDescription() const;
	virtual void plug(Connector* connector, const EmuTime& time) throw();
	virtual void unplug(const EmuTime& time);
};

class Y8950KeyboardConnector : public Connector {
public:
	Y8950KeyboardConnector();
	virtual ~Y8950KeyboardConnector();

	void write(byte data, const EmuTime& time);
	byte read(const EmuTime& time);

	// Connector
	virtual const string& getDescription() const;
	virtual const string& getClass() const;
	virtual void plug(Pluggable* dev, const EmuTime& time)
		throw(PlugException);

private:
	byte data;
};

} // namespace openmsx

#endif
