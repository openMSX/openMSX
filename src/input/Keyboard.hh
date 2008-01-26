// $Id$

#ifndef KEYBOARD_HH
#define KEYBOARD_HH

#include "MSXEventListener.hh"
#include "Unicode.hh"
#include "Observer.hh"
#include "Schedulable.hh"
#include "openmsx.hh"
#include "Keys.hh"
#include "KeyboardSettings.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class Scheduler;
class MSXCommandController;
class EventDistributor;
class MSXEventDistributor;
class EmuTime;
class KeyMatrixUpCmd;
class KeyMatrixDownCmd;
class KeyInserter;
class KeyEvent;
class CapsLockAligner;
class Setting;
template <typename T> class EnumSetting;

class Keyboard : private MSXEventListener, private Schedulable, private Observer<Setting>
{
public:
	/**
	 * Constructs a new Keyboard object.
	 * @param scheduler ref to the scheduler
	 * @param msxCommandController ref to the command controller
	 * @param eventDistributor ref to the emu event distributor
	 * @param msxEventDistributor ref to the user input event distributor
	 * @param keyboardType contains filename extension of unicode keymap file
	 * @param hasKeypad turn MSX keypad on/off
	 * @param keyGhosting turn keyGhosting on/off
	 * @param keyGhostingSGCprotected Shift, Graph and Code are keyGhosting protected
	 */
	Keyboard(Scheduler& scheduler, MSXCommandController& msxCommandController,
	         EventDistributor& eventDistributor,
	         MSXEventDistributor& msxEventDistributor,
	         std::string& keyboardType, bool hasKeypad,
		 bool keyGhosting, bool keyGhostingSGCprotected);

	virtual ~Keyboard();

	/**
	 * Returns a pointer to the current KeyBoard matrix
	 */
	const byte* getKeys();

	EnumSetting<Keys::KeyCode>& getCodeKanaHostKey();

	static const unsigned NR_KEYROWS = 16;

private:
	// Observer<Setting>
	virtual void update(const Setting& setting);

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         const EmuTime& time);

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	void processCodeKanaChange(bool down);
	void processCapslockEvent(const EmuTime& time);
	void processKeypadEnterKey(bool down);
	void processSdlKey(bool down, int key);
	void processKeyEvent(bool down, const KeyEvent& keyEvent);
	void updateKeyMatrix(bool down, int row, byte mask);
	void doKeyGhosting();
	void parseKeymapfile(const byte* buf, unsigned size);
	void loadKeymapfile(const std::string& filename);
	void parseUnicodeKeymapfile(const byte* buf, unsigned size);
	void loadUnicodeKeymapfile(const std::string& filename);
	std::string processCmd(const std::vector<std::string>& tokens, bool up);
	void pressUnicodeByUser(Unicode::unicode1_char unicode, int key, bool down);
	void pressAscii(Unicode::unicode1_char unicode, bool down);
	bool commonKeys(Unicode::unicode1_char unicode1, Unicode::unicode1_char unicode2);

	MSXEventDistributor& msxEventDistributor;

	friend class KeyMatrixUpCmd;
	friend class KeyMatrixDownCmd;
	friend class KeyInserter;
	friend class CapsLockAligner;

	const std::auto_ptr<KeyMatrixUpCmd>   keyMatrixUpCmd;
	const std::auto_ptr<KeyMatrixDownCmd> keyMatrixDownCmd;
	const std::auto_ptr<KeyInserter>      keyTypeCmd;
	const std::auto_ptr<CapsLockAligner>  capsLockAligner;
	const std::auto_ptr<KeyboardSettings> keyboardSettings;

	byte cmdKeyMatrix[NR_KEYROWS];
	byte userKeyMatrix[NR_KEYROWS];
	byte keyMatrix[NR_KEYROWS];
	byte msxmodifiers;
	bool keyGhosting;
	bool keyGhostingSGCprotected;
	bool keysChanged;
	bool msxCapsLockOn;
	bool hasKeypad;
	static const int MAX_KEYSYM = 0x150;
	static byte keyTab[MAX_KEYSYM][2];
//	static short asciiTab[256][2];
	Unicode::unicode1_char dynKeymap[MAX_KEYSYM];
	byte unicodeTab[65536][3];
};

} // namespace openmsx

#endif
