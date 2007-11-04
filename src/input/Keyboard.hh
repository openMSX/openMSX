// $Id$

#ifndef KEYBOARD_HH
#define KEYBOARD_HH

#include "MSXEventListener.hh"
#include "Schedulable.hh"
#include "openmsx.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class Scheduler;
class MSXCommandController;
class MSXEventDistributor;
class EmuTime;
class KeyMatrixUpCmd;
class KeyMatrixDownCmd;
class KeyInserter;
class KeyEvent;
class BootCapsLockAligner;

class Keyboard : private MSXEventListener, private Schedulable
{
public:
	/**
	 * Constructs a new Keyboard object.
	 * @param scheduler ref to the scheduler
	 * @param msxCommandController ref to the command controller
	 * @param eventDistributor ref to the user input event distributor
	 * @param keyboardType contains type of keyboard (0: international, 1: japanese, etc)
	 * @param hasKeypad turn MSX keypad on/off
	 * @param keyGhosting turn keyGhosting on/off
	 */
	Keyboard(Scheduler& scheduler, MSXCommandController& msxCommandController,
	         MSXEventDistributor& eventDistributor, 
		 int keyboardType, bool hasKeypad, bool keyGhosting);

	virtual ~Keyboard();

	/**
	 * Returns a pointer to the current KeyBoard matrix
	 */
	const byte* getKeys();

	static const unsigned NR_KEYROWS = 16;

private:
	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         const EmuTime& time);

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	void alignCapsLock();
	void processKey(bool down, int key);
        void processKeyEvent(bool down, const KeyEvent& keyEvent);
	void normalKey(bool down, word chr, int);
	void doKeyGhosting();
	void parseKeymapfile(const byte* buf, unsigned size);
	void loadKeymapfile(const std::string& filename);
	void parseUnicodeKeymapfile(const byte* buf, unsigned size);
	void loadUnicodeKeymapfile(const std::string& filename);
	std::string processCmd(const std::vector<std::string>& tokens, bool up);
	void pressUnicodeByUser(word unicode, int key, bool down);
	void pressAscii(word unicode, bool down);
	bool commonKeys(word unicode1, word unicode2);

	MSXEventDistributor& eventDistributor;

	friend class KeyMatrixUpCmd;
	friend class KeyMatrixDownCmd;
	friend class KeyInserter;
	friend class BootCapsLockAligner;
	const std::auto_ptr<KeyMatrixUpCmd>   keyMatrixUpCmd;
	const std::auto_ptr<KeyMatrixDownCmd> keyMatrixDownCmd;
	const std::auto_ptr<KeyInserter>      keyTypeCmd;
	const std::auto_ptr<BootCapsLockAligner> bootCapsLockAligner;

	byte cmdKeyMatrix[NR_KEYROWS];
	byte userKeyMatrix[NR_KEYROWS];
	byte keyMatrix[NR_KEYROWS];
	byte msxmodifiers;
	bool keyGhosting;
	bool keysChanged;
	bool msxCapsLockOn;
	bool hasKeypad;
	static const int MAX_KEYSYM = 0x150;
	static byte keyTab[MAX_KEYSYM][2];
//	static short asciiTab[256][2];
	word dynKeymap[MAX_KEYSYM];
	byte unicodeTab[65536][3];
};

} // namespace openmsx

#endif
