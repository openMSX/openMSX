// $Id$
//
// Base class for MSX ROM Devices

#ifndef __MSXROM_HH__
#define __MSXROM_HH__

#include "msxconfig.hh"
#include "MSXDevice.hh"
#include "LoadFile.hh"

class MSXRom: public MSXDevice, public LoadFile
{
    public:
        MSXRom(MSXConfig::Device *config);

    protected:
        /**
         * Trivially needed for LoadFile mixin
         */
        virtual MSXConfig::Device* LoadFileGetConfigDevice() {
            return deviceConfig; }

    private:
        byte* memoryBank;
};

#endif // __MSXROM_HH__
