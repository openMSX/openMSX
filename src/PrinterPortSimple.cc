// $Id$

#include <cassert>
#include "PrinterPortSimple.hh"
#include "DACSound.hh"
#include "PluggingController.hh"
#include "sound/DACSound.hh"


PrinterPortSimple::PrinterPortSimple()
{
	PRT_DEBUG("Creating an PrinterPortSimple");
	dac = NULL;
	PluggingController::instance()->registerPluggable(this);
}

PrinterPortSimple::~PrinterPortSimple()
{
	PluggingController::instance()->unregisterPluggable(this);
	delete dac;	// unplug();
}

bool PrinterPortSimple::getStatus(const EmuTime &time)
{
	return true;	// TODO check
}

void PrinterPortSimple::setStrobe(bool strobe, const EmuTime &time)
{
	// ignore strobe	TODO check
}

void PrinterPortSimple::writeData(byte data, const EmuTime &time)
{
	dac->writeDAC(data,time);
}

void PrinterPortSimple::plug(const EmuTime &time)
{
	short volume = 12000;	// TODO read from config 
	dac = new DACSound(volume, 16000, time);
}

void PrinterPortSimple::unplug(const EmuTime &time)
{
	delete dac;
	dac = NULL;
}

const std::string &PrinterPortSimple::getName()
{
	return name;
}
const std::string PrinterPortSimple::name("simple");
