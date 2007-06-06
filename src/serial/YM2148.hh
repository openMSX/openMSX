// $Id$
/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/Memory/romMapperSfg05.c,v 
** Revision: 1.12
** Date: 2007/04/28 05:06:29
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2006 Daniel Vik
*/

// SFG05 Midi: The MIDI Out is probably buffered. If UART is unbuffered, all
//             data will not be transmitted correctly. Question is how big
//             the buffer is.
//             The command bits are not clear at all. Only known bit is the
//             reset bit.

// NOTES: Cmd bit 3: seems to be enable/disable something (checked before RX
//        Cmd bit 4: is set when IM2 is used, cleared when IM1 is used


#ifndef YM2148_HH
#define YM2148_HH

#include "openmsx.hh"

namespace openmsx {

#define RX_QUEUE_SIZE 256

class YM2148
{

public:
	YM2148();
	~YM2148();
	void reset();

	byte readStatus();
	byte readData();
	void setVector(byte value);
	void writeCommand(byte value);
	void writeData(byte value);

private:
	void midiInCallback(byte* buffer, unsigned length);
	void onRecv(unsigned time);
	void onTrans(unsigned time);

//TODO:	MidiIO*     midiIo;
	byte       command;
	byte       rxData;
	byte       status;
	byte       txBuffer;
	int        txPending;
	byte       rxQueue[RX_QUEUE_SIZE];
	int        rxPending;
	int        rxHead;
	void*      semaphore;
	unsigned   charTime;
	byte       vector;
//TODO:	BoardTimer* timerRecv;
	unsigned   timeRecv;
//TODO:	BoardTimer* timerTrans;
	unsigned   timeTrans;
};

} // namespace openmsx

#endif
