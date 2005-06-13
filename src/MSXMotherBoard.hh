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
