// $Id$
//
// Base class for MSX ROM Devices

#ifndef __MSXROM_HH__
#define __MSXROM_HH__

#include "msxconfig.hh"
#include "MSXDevice.hh"
#include "LoadFile.hh"

class MSXRom: public LoadFile
{
    public:

        /**
         * delete memory bank
         */
        virtual ~MSXRom();

    protected:
        /**
         * Delegate down more
         */
        virtual MSXConfig::Device* GetDeviceConfig()=0;

        byte* memoryBank;
};

#endif // __MSXROM_HH__
