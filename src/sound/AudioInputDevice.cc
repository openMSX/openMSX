// $Id$

#include "AudioInputDevice.hh"


const string &AudioInputDevice::getClass() const
{
	static const string className("Audio Input Port");
	return className;
}
