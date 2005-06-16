// $Id$

#ifndef MSXMOTHERBOARD_HH
#define MSXMOTHERBOARD_HH

#include "SettingListener.hh"
#include "Command.hh"
#include <memory>
#include <vector>

namespace openmsx {

class CliComm;
class MSXDevice;
class XMLElement;
class BooleanSetting;
class CartridgeSlotManager;
class DummyDevice;
class MSXCPU;
class MSXCPUInterface;
class PanasonicMemory;
class MSXDeviceSwitch;

class MSXMotherBoard : private SettingListener
{
public:
	MSXMotherBoard();
	virtual ~MSXMotherBoard();

	/**
	 * Run emulation.
	 * @return True if emulation steps were done,
	 *   false if emulation is suspended.
	 */
	bool execute();

	/**
	 * Block the complete MSX (CPU and devices), used by breakpoints
	 */
	void block();
	void unblock();

	/**
	 * Pause MSX machine. Only CPU is paused, other devices continue
	 * running. Used by turbor hardware pause.
	 */
	void pause();
	void unpause();
	
	void powerUpMSX();
	void powerDownMSX();

	/**
	 * This will reset all MSXDevices (the reset() method of
	 * all registered MSXDevices is called)
	 */
	void resetMSX();

	/** Parse machine config file and instantiate MSX machine
	  */
	void readConfig();

	// The following classes are unique per MSX machine
	CartridgeSlotManager& getSlotManager() const { return *slotManager; }
	DummyDevice& getDummyDevice();
	MSXCPU& getCPU() const { return *msxCpu; }
	MSXCPUInterface& getCPUInterface();
	PanasonicMemory& getPanasonicMemory();
	MSXDeviceSwitch& getDeviceSwitch();
	
private:
	// SettingListener
	virtual void update(const Setting* setting);

	/**
	 * All MSXDevices should be registered by the MotherBoard.
	 * This method should only be called at start-up
	 */
	void addDevice(std::auto_ptr<MSXDevice> device);

	void createDevices(const XMLElement& elem);

	typedef std::vector<MSXDevice*> Devices;
	Devices availableDevices;

	bool powered;

	int blockedCounter;

	BooleanSetting& powerSetting;
	CliComm& output;
	std::auto_ptr<CartridgeSlotManager> slotManager;
	std::auto_ptr<XMLElement> dummyDeviceConfig;
	std::auto_ptr<DummyDevice> dummyDevice;
	std::auto_ptr<MSXCPU> msxCpu;
	std::auto_ptr<MSXCPUInterface> msxCpuInterface;
	std::auto_ptr<PanasonicMemory> panasonicMemory;
	std::auto_ptr<XMLElement> devSwitchConfig;
	std::auto_ptr<MSXDeviceSwitch> deviceSwitch;

	class ResetCmd : public SimpleCommand {
	public:
		ResetCmd(MSXMotherBoard& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		MSXMotherBoard& parent;
	} resetCommand;
};

} // namespace openmsx

#endif
