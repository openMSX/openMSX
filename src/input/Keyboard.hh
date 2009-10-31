// $Id$

#ifndef KEYBOARD_HH
#define KEYBOARD_HH

#include "MSXEventListener.hh"
#include "Schedulable.hh"
#include "serialize_meta.hh"
#include "openmsx.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class Scheduler;
class CommandController;
class EventDistributor;
class MSXEventDistributor;
class KeyMatrixUpCmd;
class KeyMatrixDownCmd;
class KeyInserter;
class KeyEvent;
class CapsLockAligner;
class KeyboardSettings;
class MsxKeyEventQueue;
class UnicodeKeymap;
class KeybDebuggable;

class Keyboard : private MSXEventListener, private Schedulable
{
public:
	static const unsigned NR_KEYROWS = 16;

	/** Constructs a new Keyboard object.
	 * @param motherBoard ref to the motherBoard
	 * @param scheduler ref to the scheduler
	 * @param commandController ref to the command controller
	 * @param eventDistributor ref to the emu event distributor
	 * @param msxEventDistributor ref to the user input event distributor
	 * @param keyboardType contains filename extension of unicode keymap file
	 * @param hasKeypad turn MSX keypad on/off
	 * @param keyGhosting turn keyGhosting on/off
	 * @param keyGhostingSGCprotected Shift, Graph and Code are keyGhosting protected
	 * @param codeKanaLocks CodeKana key behave as a lock key on this machine
	 * @param graphLocks Graph key behave as a lock key on this machine
	 */
	Keyboard(MSXMotherBoard& motherBoard, Scheduler& scheduler,
	         CommandController& commandController,
	         EventDistributor& eventDistributor,
	         MSXEventDistributor& msxEventDistributor,
	         std::string& keyboardType, bool hasKeypad,
	         bool keyGhosting, bool keyGhostingSGCprotected,
	         bool codeKanaLocks, bool graphLocks);

	virtual ~Keyboard();

	/** Returns a pointer to the current KeyBoard matrix
	 */
	const byte* getKeys();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         EmuTime::param time);

	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual const std::string& schedName() const;

	void processRightControlEvent(bool down);
	void processCapslockEvent(EmuTime::param time);
	void processCodeKanaChange(bool down);
	void processGraphChange(bool down);
	void processKeypadEnterKey(bool down);
	void processSdlKey(bool down, int key);
	bool processQueuedEvent(shared_ptr<const Event> event, EmuTime::param time);
	bool processKeyEvent(bool down, const KeyEvent& keyEvent);
	void updateKeyMatrix(bool down, int row, byte mask);
	void doKeyGhosting();
	std::string processCmd(const std::vector<std::string>& tokens, bool up);
	bool pressUnicodeByUser(unsigned unicode, int key, bool down);
	int pressAscii(unsigned unicode, bool down);
	void pressLockKeys(int lockKeysMask, bool down);
	bool commonKeys(unsigned unicode1, unsigned unicode2);
	void debug(const char* format, ...);

	CommandController& commandController;
	MSXEventDistributor& msxEventDistributor;

	friend class KeyMatrixUpCmd;
	friend class KeyMatrixDownCmd;
	friend class KeyInserter;
	friend class CapsLockAligner;
	friend class MsxKeyEventQueue;

	const std::auto_ptr<KeyMatrixUpCmd>   keyMatrixUpCmd;
	const std::auto_ptr<KeyMatrixDownCmd> keyMatrixDownCmd;
	const std::auto_ptr<KeyInserter>      keyTypeCmd;
	const std::auto_ptr<CapsLockAligner>  capsLockAligner;
	const std::auto_ptr<KeyboardSettings> keyboardSettings;
	const std::auto_ptr<MsxKeyEventQueue> msxKeyEventQueue;
	const std::auto_ptr<KeybDebuggable>   keybDebuggable;

	const std::auto_ptr<UnicodeKeymap> unicodeKeymap;
	byte cmdKeyMatrix[NR_KEYROWS];
	byte userKeyMatrix[NR_KEYROWS];
	byte keyMatrix[NR_KEYROWS];
	byte msxmodifiers;
	const bool hasKeypad;
	const bool keyGhosting;
	const bool keyGhostingSGCprotected;
	const bool codeKanaLocks;
	const bool graphLocks;
	bool keysChanged;
	bool msxCapsLockOn;
	bool msxCodeKanaLockOn;
	bool msxGraphLockOn;

	static const int MAX_KEYSYM = 0x150;
	static const byte keyTab[MAX_KEYSYM][2];
	unsigned dynKeymap[MAX_KEYSYM];
};
SERIALIZE_CLASS_VERSION(Keyboard, 2);

} // namespace openmsx

#endif
