# $Id$

SRC_HDR:= \
	I8251 I8254 \
	ClockPin \
	MSXMidi \
	MidiInDevice DummyMidiInDevice MidiInConnector MidiInReader \
	MidiOutDevice DummyMidiOutDevice MidiOutConnector MidiOutLogger \
	RS232Connector RS232Device MSXRS232 DummyRS232Device RS232Tester

HDR_ONLY:= \
	SerialDataInterface.hh

$(eval $(PROCESS_NODE))

