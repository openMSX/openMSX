// $Id$
//
// Base class for MSX ROM Devices

#ifndef __MSXROM_HH__
#define __MSXROM_HH__

#include "msxconfig.hh"
#include "MSXDevice.hh"
#include "LoadFile.hh"

class MSXRom: virtual public MSXDevice, public LoadFile
{
    public:

        /**
         * delete memory bank
         */
        virtual ~MSXRom();

        /**
         * Return the MSXConfig object
         */
        MSXConfig::Device* getDeviceConfig();

        byte* memoryBank;
};

#endif // __MSXROM_HH__
