// $Id$

#ifndef __MSXDEVICESWITCH_HH__
#define __MSXDEVICESWITCH_HH__

#include "MSXIODevice.hh"


class MSXSwitchedDevice
{
	public:
		MSXSwitchedDevice(byte id);
		virtual ~MSXSwitchedDevice();
		
		virtual void reset(const EmuTime &time) = 0;
		virtual byte readIO(byte port, const EmuTime &time) = 0;
		virtual void writeIO(byte port, byte value, const EmuTime &time) = 0;

	private:
		byte id;
};


class MSXDeviceSwitch : public MSXIODevice
{
	public:
		virtual ~MSXDeviceSwitch();

		static MSXDeviceSwitch* instance();

		// (un)register methods for devices
		void registerDevice(byte id, MSXSwitchedDevice* device); 
		void unregisterDevice(byte id); 

		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
	
	private:
		MSXDeviceSwitch(MSXConfig::Device *config, const EmuTime &time);
		
		static MSXDeviceSwitch* oneInstance;
		byte selected;
		MSXSwitchedDevice* devices[256];
};

#endif
