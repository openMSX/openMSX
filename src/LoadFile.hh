// $Id$
//
// LoadFile mixin

#ifndef __LOADFILE__HH__
#define __LOADFILE__HH__

#include "config.h"
#include "openmsx.hh"
#include "msxconfig.hh"

class LoadFile
{
    public:
    /**
	 * Load a file of given size and allocates memory for it, 
	 * the pointer memorybank will point to this memory block.
	 * The filename is the "filename" parameter in config file.
	 * The first "skip_headerbytes" bytes of the file are ignored.
	 */
	void loadFile(byte** memoryBank, int fileSize);

    protected:
    /**
     * Pure virtual function that needs to be supplied
     * by class that is using this mixin.
     */
    virtual MSXConfig::Device* GetDeviceConfig()=0;
};


#endif // __LOADFILE__HH__
