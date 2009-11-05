// $Id$

#include "Keyboard.hh"
#include "KeyboardSettings.hh"
#include "Keys.hh"
#include "Clock.hh"
#include "EventListener.hh"
#include "EventDistributor.hh"
#include "InputEventFactory.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "MSXException.hh"
#include "RecordedCommand.hh"
#include "CommandException.hh"
#include "SimpleDebuggable.hh"
#include "InputEvents.hh"
#include "StateChange.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "UnicodeKeymap.hh"
#include "utf8_checked.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cstdarg>
#include <deque>

using std::string;
using std::vector;

namespace openmsx {

static const byte SHIFT_MASK = 0x01;
static const byte CTRL_MASK  = 0x02;
static const byte GRAPH_MASK = 0x04;
static const byte CAPS_MASK  = 0x08;
static const byte CODE_MASK  = 0x10;

class KeyMatrixUpCmd : public RecordedCommand
{
public:
	KeyMatrixUpCmd(CommandController& commandController,
	               StateChangeDistributor& stateChangeDistributor,
	               Scheduler& scheduler, Keyboard& keyboard);
	virtual string execute(const vector<string>& tokens, EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;
private:
	Keyboard& keyboard;
};

class KeyMatrixDownCmd : public RecordedCommand
{
public:
	KeyMatrixDownCmd(CommandController& commandController,
	                 StateChangeDistributor& stateChangeDistributor,
	                 Scheduler& scheduler, Keyboard& keyboard);
	virtual string execute(const vector<string>& tokens, EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;
private:
	Keyboard& keyboard;
};

class MsxKeyEventQueue : public Schedulable
{
public:
	MsxKeyEventQueue(Scheduler& scheduler, Keyboard& keyboard);
	void process_asap(EmuTime::param time, shared_ptr<const Event> event);
	void clear();
	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual const string& schedName() const;
	std::deque<shared_ptr<const Event> > eventQueue;
	Keyboard& keyboard;
};

class KeyInserter : public RecordedCommand, public Schedulable
{
public:
	KeyInserter(CommandController& commandController,
	            StateChangeDistributor& stateChangeDistributor,
	            Scheduler& scheduler, Keyboard& keyboard);
	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void type(const string& str);
	void reschedule(EmuTime::param time);

	// Command
	virtual string execute(const vector<string>& tokens, EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;

	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual const string& schedName() const;

	Keyboard& keyboard;
	string text_utf8;
	bool releaseLast;
	unsigned last;
	int lockKeysMask;
	bool oldCodeKanaLockOn;
	bool oldGraphLockOn;
	bool oldCapsLockOn;
};

class CapsLockAligner : private EventListener, private Schedulable
{
public:
	CapsLockAligner(EventDistributor& eventDistributor,
	                MSXEventDistributor& msxEventDistributor,
	                Scheduler& scheduler, Keyboard& keyboard);
	virtual ~CapsLockAligner();

private:
	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual const string& schedName() const;

	void alignCapsLock(EmuTime::param time);

	Keyboard& keyboard;
	EventDistributor& eventDistributor;
	MSXEventDistributor& msxEventDistributor;
};

class KeybDebuggable : public SimpleDebuggable
{
public:
	KeybDebuggable(MSXMotherBoard& motherBoard, Keyboard& keyboard);
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);
private:
	Keyboard& keyboard;
};


class KeyMatrixState : public StateChange
{
public:
	KeyMatrixState(EmuTime::param time, byte row_, byte press_, byte release_)
		: StateChange(time)
		, row(row_), press(press_), release(release_)
	{
		// disallow useless events
		assert((press != 0) || (release != 0));
		// avoid confusion about what happens when some bits are both
		// set and reset (in other words: don't rely on order of and-
		// and or-operations)
		assert((press & release) == 0);
	}
	byte getRow()     const { return row; }
	byte getPress()   const { return press; }
	byte getRelease() const { return release; }
private:
	const byte row, press, release;
};


Keyboard::Keyboard(MSXMotherBoard& motherBoard,
                   Scheduler& scheduler,
                   CommandController& commandController_,
                   EventDistributor& eventDistributor,
                   MSXEventDistributor& msxEventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   string& keyboardType, bool hasKP, bool keyGhosting_,
                   bool keyGhostSGCprotected, bool codeKanaLocks_,
                   bool graphLocks_)
	: Schedulable(scheduler)
	, commandController(commandController_)
	, msxEventDistributor(msxEventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, keyMatrixUpCmd  (new KeyMatrixUpCmd  (
		commandController, stateChangeDistributor, scheduler, *this))
	, keyMatrixDownCmd(new KeyMatrixDownCmd(
		commandController, stateChangeDistributor, scheduler, *this))
	, keyTypeCmd(new KeyInserter(
		commandController, stateChangeDistributor, scheduler, *this))
	, capsLockAligner(new CapsLockAligner(
		eventDistributor, msxEventDistributor, scheduler, *this))
	, keyboardSettings(new KeyboardSettings(commandController))
	, msxKeyEventQueue(new MsxKeyEventQueue(scheduler, *this))
	, keybDebuggable(new KeybDebuggable(motherBoard, *this))
	, unicodeKeymap(new UnicodeKeymap(keyboardType))
	, hasKeypad(hasKP)
	, keyGhosting(keyGhosting_)
	, keyGhostingSGCprotected(keyGhostSGCprotected)
	, codeKanaLocks(codeKanaLocks_)
	, graphLocks(graphLocks_)
{
	keysChanged = false;
	msxCapsLockOn = false;
	msxCodeKanaLockOn = false;
	msxGraphLockOn = false;
	msxmodifiers = 0xff;
	memset(keyMatrix,     255, sizeof(keyMatrix));
	memset(cmdKeyMatrix,  255, sizeof(cmdKeyMatrix));
	memset(userKeyMatrix, 255, sizeof(userKeyMatrix));
	memset(dynKeymap, 0, sizeof(dynKeymap));

	msxEventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
	// We do not listen for CONSOLE_OFF_EVENTS because rescanning the
	// keyboard can have unwanted side effects
}

Keyboard::~Keyboard()
{
	stateChangeDistributor.unregisterListener(*this);
	msxEventDistributor.unregisterEventListener(*this);
}


const byte* Keyboard::getKeys()
{
	if (keysChanged) {
		keysChanged = false;
		for (unsigned i = 0; i < NR_KEYROWS; ++i) {
			keyMatrix[i] = cmdKeyMatrix[i] & userKeyMatrix[i];
		}
		if (keyGhosting) {
			doKeyGhosting();
		}
	}
	return keyMatrix;
}

/* Received an MSX event (through EventTranslator class)
 * Following events get processed:
 *  OPENMSX_KEY_DOWN_EVENT
 *  OPENMSX_KEY_UP_EVENT
 */
void Keyboard::signalEvent(shared_ptr<const Event> event,
                           EmuTime::param time)
{
	EventType type = event->getType();
	if ((type == OPENMSX_KEY_DOWN_EVENT) ||
	    (type == OPENMSX_KEY_UP_EVENT)) {
		// Ignore possible console on/off events:
		// we do not rescan the keyboard since this may lead to
		// an unwanted pressing of <return> in MSX after typing
		// "set console off" in the console.
		msxKeyEventQueue->process_asap(time, event);
	}
}

void Keyboard::signalStateChange(shared_ptr<const StateChange> event)
{
	const KeyMatrixState* kms = dynamic_cast<const KeyMatrixState*>(event.get());
	if (!kms) return;

	userKeyMatrix[kms->getRow()] &= ~kms->getPress();
	userKeyMatrix[kms->getRow()] |=  kms->getRelease();
	keysChanged = true; // do ghosting at next getKeys()
}

void Keyboard::stopReplay(EmuTime::param time)
{
	// TODO Read actual state from keyboard, currently we just clear all
	//      pressed keys. Might be hard to implement correctly, is it worth
	//      the effort?
	for (unsigned row = 0; row < NR_KEYROWS; ++row) {
		// TODO for the moment it's required that we actually set
		//      userKeyMatrix to the new value before actually sending
		//      the event. Otherwise we trigger a new stopReplay() that
		//      would again send the event and so on
		//      This is a very fragile solution. We should refactor
		//      this soon.
		changeKeyMatrixEvent(time, row, 0xff);
	}
	msxmodifiers = 0xff;
	msxKeyEventQueue->clear();
	memset(dynKeymap, 0, sizeof(dynKeymap));
}

void Keyboard::pressKeyMatrixEvent(EmuTime::param time, byte row, byte press)
{
	changeKeyMatrixEvent(time, row, userKeyMatrix[row] & ~press);
}
void Keyboard::releaseKeyMatrixEvent(EmuTime::param time, byte row, byte release)
{
	changeKeyMatrixEvent(time, row, userKeyMatrix[row] | release);
}
void Keyboard::changeKeyMatrixEvent(EmuTime::param time, byte row, byte newValue)
{
	byte diff = userKeyMatrix[row] ^ newValue;
	if (diff == 0) {
		// event won't actually change the keymatrix, so ignore it
		return;
	}
	byte press   = userKeyMatrix[row] & diff;
	byte release = newValue           & diff;
	userKeyMatrix[row] = newValue; // to break infinite loop, see stopReplay()
	stateChangeDistributor.distributeNew(shared_ptr<const StateChange>(
		new KeyMatrixState(time, row, press, release)));
}

bool Keyboard::processQueuedEvent(shared_ptr<const Event> event, EmuTime::param time)
{
	bool insertCodeKanaRelease = false;
	const KeyEvent& keyEvent = checked_cast<const KeyEvent&>(*event);
	bool down = event->getType() == OPENMSX_KEY_DOWN_EVENT;
	Keys::KeyCode key = static_cast<Keys::KeyCode>
		(int(keyEvent.getKeyCode()) & int(Keys::K_MASK));
	if (down) {
		debug("Key pressed, unicode: 0x%04x, keyCode: 0x%05x, keyName: %s\n",
		      keyEvent.getUnicode(),
		      keyEvent.getKeyCode(),
		      Keys::getName(keyEvent.getKeyCode()).c_str());
	} else {
		debug("Key released, unicode: 0x%04x, keyCode: 0x%05x, keyName: %s\n",
		      keyEvent.getUnicode(),
		      keyEvent.getKeyCode(),
		      Keys::getName(keyEvent.getKeyCode()).c_str());
	}
	if (key == Keys::K_RCTRL &&
	    keyboardSettings->getMappingMode().getValue() ==
	            KeyboardSettings::CHARACTER_MAPPING) {
		processRightControlEvent(time, down);
	} else if (key == Keys::K_CAPSLOCK) {
		processCapslockEvent(time);
	} else if (key == keyboardSettings->getCodeKanaHostKey().getValue()) {
		processCodeKanaChange(time, down);
	} else if (key == Keys::K_LALT) {
		processGraphChange(time, down);
	} else if (key == Keys::K_KP_ENTER) {
		processKeypadEnterKey(time, down);
	} else {
		insertCodeKanaRelease = processKeyEvent(time, down, keyEvent);
	}
	return insertCodeKanaRelease;
}

/*
 * Process a change (up or down event) of the CODE/KANA key
 * It presses or releases the key in the MSX keyboard matrix
 * and changes the kanalock state in case of a press
 */
void Keyboard::processCodeKanaChange(EmuTime::param time, bool down)
{
	if (down) {
		msxCodeKanaLockOn = !msxCodeKanaLockOn;
	}
	updateKeyMatrix(time, down, 6, CODE_MASK);
}

/*
 * Process a change (up or down event) of the GRAPH key
 * It presses or releases the key in the MSX keyboard matrix
 * and changes the graphlock state in case of a press
 */
void Keyboard::processGraphChange(EmuTime::param time, bool down)
{
	if (down) {
		msxGraphLockOn = !msxGraphLockOn;
	}
	updateKeyMatrix(time, down, 6, GRAPH_MASK);
}

/*
 * Process a change (up or down event) of the RIGHT CTRL key
 * It presses or releases the DEADKEY at the correct location
 * in the keyboard matrix
 */
void Keyboard::processRightControlEvent(EmuTime::param time, bool down)
{
	UnicodeKeymap::KeyInfo deadkey = unicodeKeymap->getDeadkey();
	updateKeyMatrix(time, down, deadkey.row, deadkey.keymask);
}

/*
 * Process a change event of the CAPSLOCK *STATUS*;
 *  SDL sends a CAPSLOCK press event at the moment that the host CAPSLOCK
 *  status goes 'on' and it sends the release event only when the host
 *  CAPSLOCK status goes 'off'. However, the emulated MSX must see a press
 *  and release event when CAPSLOCK status goes on and another press and
 *  release event when it goes off again. This is achieved by pressing
 *  CAPSLOCK key at the moment that the host CAPSLOCK status changes and
 *  releasing the CAPSLOCK key shortly after (via a timed event)
 *
 * Please be aware that there is a 'bug-fix' for SDL that changes the
 * behaviour of SDL with respect to the handling of the CAPSLOCK status.
 * When this bug-fix is effective, SDL sends a press event at the moment that
 * the user presses the CAPSLOCK key and a release event at the moment that the
 * user releases the CAPSLOCK key. Once this bug-fix becomes the standard
 * behaviour, this routine should be adapted accordingly.
 */
void Keyboard::processCapslockEvent(EmuTime::param time)
{
	msxCapsLockOn = !msxCapsLockOn;
	updateKeyMatrix(time, true, 6, CAPS_MASK);
	Clock<1000> now(time);
	setSyncPoint(now + 100);
}

void Keyboard::executeUntil(EmuTime::param time, int /*userData*/)
{
	updateKeyMatrix(time, false, 6, CAPS_MASK);
}

const string& Keyboard::schedName() const
{
	static const string schedName = "Keyboard";
	return schedName;
}

void Keyboard::processKeypadEnterKey(EmuTime::param time, bool down)
{
	if (!hasKeypad && !keyboardSettings->getAlwaysEnableKeypad().getValue()) {
		// User entered on host keypad but this MSX model does not have one
		// Ignore the keypress/release
		return;
	}
	int row;
	byte mask;
	if (keyboardSettings->getKpEnterMode().getValue() ==
	    KeyboardSettings::MSX_KP_COMMA) {
		row = 10;
		mask = 0x40;
	} else {
		row = 7;
		mask = 0x80;
	}
	updateKeyMatrix(time, down, row, mask);
}

/*
 * Process an SDL key press/release event. It concerns a
 * special key (e.g. SHIFT, UP, DOWN, F1, F2, ...) that can not
 * be unambiguously derived from a unicode character;
 *  Map the SDL key to an equivalent MSX key press/release event
 */
void Keyboard::processSdlKey(EmuTime::param time, bool down, int key)
{
	if (key < MAX_KEYSYM) {
		int row   = keyTab[key][0];
		byte mask = keyTab[key][1];
		updateKeyMatrix(time, down, row, mask);
	}
}

/*
 * Update the MSX keyboard matrix
 */
void Keyboard::updateKeyMatrix(EmuTime::param time, bool down, int row, byte mask)
{
	if (down) {
		pressKeyMatrixEvent(time, row, mask);
		if (row == 6) {
			// Keep track of the MSX modifiers (CTRL, GRAPH, CODE, SHIFT)
			// The MSX modifiers in row 6 of the matrix sometimes get
			// overruled by the unicode character processing, in
			// which case the unicode processing must be able to restore
			// them to the real key-combinations pressed by the user
			msxmodifiers &= ~(mask & 0x17);
		}
	} else {
		releaseKeyMatrixEvent(time, row, mask);
		if (row == 6) {
			msxmodifiers |= (mask & 0x17);
		}
	}
}

/*
 * Process an SDL key event;
 *  Check if it is a special key, in which case it can be directly
 *  mapped to the MSX matrix.
 *  Otherwise, retrieve the unicode character value for the event
 *  and map the unicode character to the key-combination that must
 *  be pressed to generate the equivalent character on the MSX
 */
bool Keyboard::processKeyEvent(EmuTime::param time, bool down, const KeyEvent& keyEvent)
{
	bool insertCodeKanaRelease = false;
	Keys::KeyCode keyCode = keyEvent.getKeyCode();
	Keys::KeyCode key = static_cast<Keys::KeyCode>
		(int(keyCode) & int(Keys::K_MASK));
	unsigned unicode;

	bool isOnKeypad = (
		(key >= Keys::K_KP0 && key <= Keys::K_KP9) ||
		(key == Keys::K_KP_PERIOD) ||
		(key == Keys::K_KP_DIVIDE) ||
		(key == Keys::K_KP_MULTIPLY) ||
		(key == Keys::K_KP_MINUS) ||
		(key == Keys::K_KP_PLUS));

	if (isOnKeypad && !hasKeypad &&
	    !keyboardSettings->getAlwaysEnableKeypad().getValue()) {
		// User entered on host keypad but this MSX model does not have one
		// Ignore the keypress/release
		return false;
	}

	if (down) {
		if ((userKeyMatrix[6] & 2) == 0 ||
		    isOnKeypad ||
		    keyboardSettings->getMappingMode().getValue() == KeyboardSettings::KEY_MAPPING) {
			// CTRL-key is active, user entered a key on numeric
			// keypad or the driver is in KEY mapping mode.
			// First two options (CTRL key active, keypad keypress) maps
			// to same unicode as some other key combinations (e.g. digit
			// on main keyboard or TAB/DEL)
			// Use unicode to handle the more common combination
			// and use direct matrix to matrix mapping for the exceptional
			// cases (CTRL+character or numeric keypad usage)
			unicode = 0;
		} else {
			unicode = keyEvent.getUnicode();
			if ((unicode < 0x20) || ((0x7F <= unicode) && (unicode < 0xA0))) {
				// Control character in C0 or C1 range.
				// Use SDL's interpretation instead.
				unicode = 0;
			} else if ((0xE000 <= unicode) && (unicode < 0xF900)) {
				// Code point in Private Use Area: undefined by Unicode,
				// so we rely on SDL's interpretation instead.
				// For example the Mac's cursor keys are in this range.
				unicode = 0;
			}
		}
		if (key < MAX_KEYSYM) {
			// Remember which unicode character is currently derived
			// from this SDL key. It must be stored here (during key-press)
			// because during key-release SDL never returns the unicode
			// value (it always returns the value 0). But we must know
			// the unicode value in order to be able to perform the correct
			// key-combination-release in the MSX
			dynKeymap[key] = unicode;
		} else {
			// Unexpectedly high key-code. Can't store the unicode
			// character for this key. Instead directly treat the key
			// via matrix to matrix mapping
			unicode = 0;
		}
		if (unicode == 0) {
			// It was an ambiguous key (numeric key-pad, CTRL+character)
			// or a special key according to SDL (like HOME, INSERT, etc)
			// or a first keystroke of a composed key
			// (e.g. altr-gr + = on azerty keyboard) or driver is in
			// direct SDL mapping mode:
			// Perform direct SDL matrix to MSX matrix mapping
			// But only when it is not a first keystroke of a
			// composed key
			if ((keyCode & Keys::KM_MODE) == 0) {
				processSdlKey(time, down, key);
			}
		} else {
			// It is a unicode character; map it to the right key-combination
			insertCodeKanaRelease = pressUnicodeByUser(time, unicode, key, true);
		}
	} else {
		// key was released
		if (key < MAX_KEYSYM) {
			unicode = dynKeymap[key]; // Get the unicode that was derived from this key
		} else {
			unicode = 0;
		}
		if (unicode == 0) {
			// It was a special key, perform matrix to matrix mapping
			// But only when it is not a first keystroke of a
			// composed key
			if ((keyCode & Keys::KM_MODE) == 0) {
				processSdlKey(time, down, key);
			}
		} else {
			// It was a unicode character; map it to the right key-combination
			pressUnicodeByUser(time, unicode, key, false);
		}
	}
	return insertCodeKanaRelease;
}

void Keyboard::doKeyGhosting()
{
	// This routine enables keyghosting as seen on a real MSX
	//
	// If on a real MSX in the keyboardmatrix the
	// real buttons are pressed as in the left matrix
	//           then the matrix to the
	// 10111111  right will be read by   10110101
	// 11110101  because of the simple   10110101
	// 10111101  electrical connections  10110101
	//           that are established  by
	// the closed switches
	// However, some MSX models have protection against
	// key-ghosting for SHIFT, GRAPH and CODE keys
	// On those models, SHIFT, GRAPH and CODE are
	// connected to row 6 via a diode. It prevents that
	// SHIFT, GRAPH and CODE get ghosted to another
	// row.
	bool changedSomething;
	do {
		changedSomething = false;
		for (unsigned i = 0; i < NR_KEYROWS - 1; i++) {
			byte row1 = keyMatrix[i];
			for (unsigned j = i + 1; j < NR_KEYROWS; j++) {
				byte row2 = keyMatrix[j];
				if ((row1 != row2) && ((row1 | row2) != 0xff)) {
					byte rowIold = keyMatrix[i];
					byte rowJold = keyMatrix[j];
					if (keyGhostingSGCprotected && i == 6) {
						keyMatrix[i] = row1 & row2;
						keyMatrix[j] = (row1 | 0x15) & row2;
						row1 &= row2;
					}
					else if (keyGhostingSGCprotected && j == 6) {
						keyMatrix[i] = row1 & (row2 | 0x15);
						keyMatrix[j] = row1 & row2;
						row1 &= (row2 | 0x15);
					}
					else {
						// not same and some common zero's
						//  --> inherit other zero's
						byte newRow = row1 & row2;
						keyMatrix[i] = newRow;
						keyMatrix[j] = newRow;
						row1 = newRow;
					}
					if (rowIold != keyMatrix[i] ||
					    rowJold != keyMatrix[j]) {
						changedSomething = true;
					}
				}
			}
		}
	} while (changedSomething);
}

string Keyboard::processCmd(const vector<string>& tokens, bool up)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	char* endPtr;
	unsigned long row = strtoul(tokens[1].c_str(), &endPtr, 0);
	if ((*endPtr != '\0') || (row >= NR_KEYROWS)) {
		throw CommandException("Invalid row");
	}
	unsigned long mask = strtoul(tokens[2].c_str(), &endPtr, 0);
	if ((*endPtr != '\0') || (mask >= 256)) {
		throw CommandException("Invalid mask");
	}
	if (up) {
		cmdKeyMatrix[row] |= mask;
	} else {
		cmdKeyMatrix[row] &= ~mask;
	}
	keysChanged = true;
	return "";
}

/*
 * This routine processes unicode characters. It maps a unicode character
 * to the correct key-combination on the MSX.
 *
 * There are a few caveats with respect to the MSX and Host modifier keys
 * that you must be aware about if you want to understand why the routine
 * works as it works.
 *
 * Row 6 of the MSX keyboard matrix contains the MSX modifier keys:
 *   CTRL, CODE, GRAPH and SHIFT
 *
 * The SHIFT key is also a modifier key on the host machine. However, the
 * SHIFT key behaviour can differ between HOST and MSX for all 'special'
 * characters (anything but A-Z).
 * For example, on AZERTY host keyboard, user presses SHIFT+& to make the '1'
 * On MSX QWERTY keyboard, the same key-combination leads to '!'.
 * So this routine must not only PRESS the SHIFT key when required according
 * to the unicode mapping table but it must also RELEASE the SHIFT key for all
 * these special keys when the user PRESSES the key/character.
 *
 * On the other hand, for A-Z, this routine must not touch the SHIFT key at all.
 * Otherwise it might give strange behaviour when CAPS lock is on (which also
 * acts as a key-modifier for A-Z). The routine can rely on the fact that
 * SHIFT+A-Z behaviour is the same on all host and MSX keyboards. It is
 * approximately the only part of keyboards that is de-facto standardized :-)
 *
 * For the other modifiers (CTRL, CODE and GRAPH), the routine must be able to
 * PRESS them when required but there is no need to RELEASE them during
 * character press. On the contrary; the host keys that map to CODE and GRAPH
 * do not work as modifiers on the host itself, so if the routine would release
 * them, it would give wrong result.
 * For example, 'ALT-A' on Host will lead to unicode character 'a', just like
 * only pressing the 'A' key. The MSX however must know about the difference.
 *
 * As a reminder: here is the build-up of row 6 of the MSX key matrix
 *              7    6     5     4    3      2     1    0
 * row  6   |  F3 |  F2 |  F1 | code| caps|graph| ctrl|shift|
 */
bool Keyboard::pressUnicodeByUser(EmuTime::param time, unsigned unicode, int key, bool down)
{
	bool insertCodeKanaRelease = false;
	UnicodeKeymap::KeyInfo keyInfo = unicodeKeymap->get(unicode);
	if (down) {
		if (codeKanaLocks &&
		    keyboardSettings->getAutoToggleCodeKanaLock().getValue() &&
		    msxCodeKanaLockOn != ((keyInfo.modmask & CODE_MASK) == CODE_MASK) &&
		    keyInfo.row < 6) { // only toggle CODE lock for 'normal' characters
			// Code Kana locks, is in wrong state and must be auto-toggled:
			// Toggle it by pressing the lock key and scheduling a
			// release event
			msxCodeKanaLockOn = !msxCodeKanaLockOn;
			pressKeyMatrixEvent(time, 6, CODE_MASK);
			insertCodeKanaRelease = true;
		} else {
			// Press the character key and related modifiers
			// Ignore the CODE key in case that Code Kana locks
			// (e.g. do not press it).
			// Ignore the GRAPH key in case that Graph locks
			// Always ignore CAPSLOCK mask (assume that user will
			// use real CAPS lock to switch/ between hiragana and
			// katanana on japanese model)
			pressKeyMatrixEvent(time, keyInfo.row, keyInfo.keymask);

			byte modmask = keyInfo.modmask & ~CAPS_MASK;
			if (codeKanaLocks) modmask &= ~CODE_MASK;
			if (graphLocks)    modmask &= ~GRAPH_MASK;
			if ((Keys::K_A <= key) && (key <= Keys::K_Z)) {
				// for A-Z, leave shift unchanged, (other
				// modifiers are only pressed, never released)
				pressKeyMatrixEvent(time, 6, modmask & ~SHIFT_MASK);
			} else {
				// for other keys, set shift according to modmask
				// so also release shift when required (other
				// modifiers are only pressed, never released)
				byte newRow = (userKeyMatrix[6] | SHIFT_MASK) & ~modmask;
				changeKeyMatrixEvent(time, 6, newRow);
			}
		}
	} else {
		releaseKeyMatrixEvent(time, keyInfo.row, keyInfo.keymask);

		// Do not simply unpress graph, ctrl, code and shift but
		// restore them to the values currently pressed by the user
		byte mask = SHIFT_MASK | CTRL_MASK;
		if (!codeKanaLocks) mask |= CODE_MASK;
		if (!graphLocks)    mask |= GRAPH_MASK;
		byte newRow = userKeyMatrix[6];
		newRow &= msxmodifiers | ~mask;
		newRow |= msxmodifiers &  mask;
		changeKeyMatrixEvent(time, 6, newRow);
	}
	keysChanged = true;
	return insertCodeKanaRelease;
}

/*
 * Press an ASCII character. It is used by the 'Insert characters'
 * function that is exposed to the console.
 * The characters are inserted in a separate keyboard matrix, to prevent
 * interference with the keypresses of the user on the MSX itself
 */
int Keyboard::pressAscii(unsigned unicode, bool down)
{
	int releaseMask = 0;
	UnicodeKeymap::KeyInfo keyInfo = unicodeKeymap->get(unicode);
	byte modmask = keyInfo.modmask & (~CAPS_MASK); // ignore CAPSLOCK mask;
	if (codeKanaLocks) {
		modmask &= (~CODE_MASK); // ignore CODE mask if CODE locks
	}
	if (graphLocks) {
		modmask &= (~GRAPH_MASK); // ignore GRAPH mask if GRAPH locks
	}
	if (down) {
		if (codeKanaLocks &&
		    msxCodeKanaLockOn != ((keyInfo.modmask & CODE_MASK) == CODE_MASK) &&
		    keyInfo.row < 6) { // only toggle CODE lock for 'normal' characters
			debug("Toggling CODE/KANA lock\n");
			msxCodeKanaLockOn = !msxCodeKanaLockOn;
			cmdKeyMatrix[6] &= (~CODE_MASK);
			releaseMask = CODE_MASK;
		}
		if (graphLocks &&
		    msxGraphLockOn != ((keyInfo.modmask & GRAPH_MASK) == GRAPH_MASK) &&
		    keyInfo.row < 6) { // only toggle GRAPH lock for 'normal' characters
			debug("Toggling GRAPH lock\n");
			msxGraphLockOn = !msxGraphLockOn;
			cmdKeyMatrix[6] &= (~GRAPH_MASK);
			releaseMask |= GRAPH_MASK;
		}
		if (msxCapsLockOn != ((keyInfo.modmask & CAPS_MASK) == CAPS_MASK) &&
		    keyInfo.row < 6) { // only toggle CAPS lock for 'normal' characters
			debug("Toggling CAPS lock\n");
			msxCapsLockOn = !msxCapsLockOn;
			cmdKeyMatrix[6] &= (~CAPS_MASK);
			releaseMask |= CAPS_MASK;
		}
		if (releaseMask == 0) {
			debug("Key pasted, unicode: 0x%04x, row: %02d, mask: %02x, modmask: %02x\n",
			      unicode, keyInfo.row, keyInfo.keymask, modmask);
			cmdKeyMatrix[keyInfo.row] &= ~keyInfo.keymask;
			cmdKeyMatrix[6] &= ~modmask;
		}
	} else {
		cmdKeyMatrix[keyInfo.row] |= keyInfo.keymask;
		cmdKeyMatrix[6] |= modmask;
	}
	keysChanged = true;
	return releaseMask;
}

/*
 * Press a lock key. It is used by the 'Insert characters'
 * function that is exposed to the console.
 * The characters are inserted in a separate keyboard matrix, to prevent
 * interference with the keypresses of the user on the MSX itself
 */
void Keyboard::pressLockKeys(int lockKeysMask, bool down)
{
	if (down) {
		// press CAPS and/or CODE/KANA lock key
		cmdKeyMatrix[6] &= (~lockKeysMask);
	} else {
		// release CAPS and/or CODE/KANA lock key
		cmdKeyMatrix[6] |= lockKeysMask;
	}
	keysChanged = true;
}

/*
 * Check if there are common keys in the MSX matrix for
 * two different unicodes.
 * It is used by the 'insert keys' function to determine if it has to wait for
 * a short while after releasing a key (to enter a certain character) before
 * pressing the next key (to enter the next character)
 */
bool Keyboard::commonKeys(unsigned unicode1, unsigned unicode2)
{
	// get row / mask of key (note: ignore modifier mask)
	UnicodeKeymap::KeyInfo keyInfo1 = unicodeKeymap->get(unicode1);
	UnicodeKeymap::KeyInfo keyInfo2 = unicodeKeymap->get(unicode2);

	return ((keyInfo1.row == keyInfo2.row) &&
	        (keyInfo1.keymask & keyInfo2.keymask));
}

void Keyboard::debug(const char* format, ...)
{
	if (keyboardSettings->getTraceKeyPresses().getValue()) {
		va_list args;
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
	}
}


// class KeyMatrixUpCmd

KeyMatrixUpCmd::KeyMatrixUpCmd(CommandController& commandController,
		StateChangeDistributor& stateChangeDistributor,
		Scheduler& scheduler, Keyboard& keyboard_)
	: RecordedCommand(commandController, stateChangeDistributor,
		scheduler, "keymatrixup")
	, keyboard(keyboard_)
{
}

string KeyMatrixUpCmd::execute(const vector<string>& tokens, EmuTime::param /*time*/)
{
	return keyboard.processCmd(tokens, true);
}

string KeyMatrixUpCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText =
		"keymatrixup <row> <bitmask>  release a key in the keyboardmatrix\n";
	return helpText;
}


// class KeyMatrixDownCmd

KeyMatrixDownCmd::KeyMatrixDownCmd(CommandController& commandController,
		StateChangeDistributor& stateChangeDistributor,
		Scheduler& scheduler, Keyboard& keyboard_)
	: RecordedCommand(commandController, stateChangeDistributor,
		scheduler, "keymatrixdown")
	, keyboard(keyboard_)
{
}

string KeyMatrixDownCmd::execute(const vector<string>& tokens, EmuTime::param /*time*/)
{
	return keyboard.processCmd(tokens, false);
}

string KeyMatrixDownCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText=
		"keymatrixdown <row> <bitmask>  press a key in the keyboardmatrix\n";
	return helpText;
}


// class MsxKeyEventQueue

MsxKeyEventQueue::MsxKeyEventQueue(Scheduler& scheduler, Keyboard& keyboard_)
	: Schedulable(scheduler)
	, keyboard(keyboard_)
{
}

void MsxKeyEventQueue::process_asap(EmuTime::param time, shared_ptr<const Event> event)
{
	bool processImmediately = eventQueue.empty();
	eventQueue.push_back(event);
	if (processImmediately) {
		executeUntil(time, 0);
	}
}

void MsxKeyEventQueue::clear()
{
	eventQueue.clear();
	removeSyncPoint();
}

void MsxKeyEventQueue::executeUntil(EmuTime::param time, int /*userData*/)
{
	// Get oldest event from the queue and process it
	shared_ptr<const Event> event = eventQueue.front();
	bool insertCodeKanaRelease = keyboard.processQueuedEvent(event, time);

	if (insertCodeKanaRelease) {
		// The processor pressed the CODE/KANA key
		// Schedule a CODE/KANA release event, to be processed
		// before any of the other events in the queue
		eventQueue.push_front(shared_ptr<const Event>(new KeyUpEvent(
			keyboard.keyboardSettings->getCodeKanaHostKey().getValue())));
	} else {
		// The event has been completely processed. Delete it from the queue
		if (!eventQueue.empty()) {
			eventQueue.pop_front();
		} else {
			// it's possible clear() has been called
			// (indirectly from keyboard.processQueuedEvent())
		}
	}

	if (!eventQueue.empty()) {
		// There are still events. Process them in 1/15s from now
		Clock<15> nextTime(time);
		setSyncPoint(nextTime + 1);
	}
}

const string& MsxKeyEventQueue::schedName() const
{
	static const string schedName = "MsxKeyEventQueue";
	return schedName;
}


// class KeyInserter

KeyInserter::KeyInserter(CommandController& commandController,
		StateChangeDistributor& stateChangeDistributor,
		Scheduler& scheduler, Keyboard& keyboard_)
	: RecordedCommand(commandController, stateChangeDistributor,
		scheduler, "type")
	, Schedulable(scheduler)
	, keyboard(keyboard_)
	, releaseLast(false)
	, lockKeysMask(0)
{
	// avoid UMR
	last = 0;
	oldCodeKanaLockOn = false;
	oldGraphLockOn = false;
	oldCapsLockOn = false;
}

string KeyInserter::execute(const vector<string>& tokens, EmuTime::param /*time*/)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	type(tokens[1]);
	return "";
}

string KeyInserter::help(const vector<string>& /*tokens*/) const
{
	static const string helpText = "Type a string in the emulated MSX.";
	return helpText;
}

void KeyInserter::type(const string& str)
{
	if (str.empty()) {
		return;
	}
	oldCodeKanaLockOn = keyboard.msxCodeKanaLockOn;
	oldGraphLockOn = keyboard.msxGraphLockOn;
	oldCapsLockOn = keyboard.msxCapsLockOn;
	if (text_utf8.empty()) {
		reschedule(getCurrentTime());
	}
	text_utf8 += str;
}

void KeyInserter::executeUntil(EmuTime::param time, int /*userData*/)
{
	if (lockKeysMask != 0) {
		// release CAPS and/or Code/Kana Lock keys
		keyboard.pressLockKeys(lockKeysMask, false);
	}
	if (releaseLast) {
		keyboard.pressAscii(last, false); // release previous character
	}
	if (text_utf8.empty()) {
		releaseLast = false;
		lockKeysMask = 0;
		if (oldCodeKanaLockOn != keyboard.msxCodeKanaLockOn) {
			keyboard.debug("Restoring CODE/KANA lock\n");
			lockKeysMask = CODE_MASK;
			keyboard.msxCodeKanaLockOn = !keyboard.msxCodeKanaLockOn;
		}
		if (oldGraphLockOn != keyboard.msxGraphLockOn) {
			keyboard.debug("Restoring GRAPH lock\n");
			lockKeysMask |= GRAPH_MASK;
			keyboard.msxGraphLockOn = !keyboard.msxGraphLockOn;
		}
		if (oldCapsLockOn != keyboard.msxCapsLockOn) {
			keyboard.debug("Restoring CAPS lock\n");
			lockKeysMask |= CAPS_MASK;
			keyboard.msxCapsLockOn = !keyboard.msxCapsLockOn;
		}
		if (lockKeysMask != 0) {
			// press CAPS, GRAPH and/or Code/Kana Lock keys
			keyboard.pressLockKeys(lockKeysMask, true);
			reschedule(time);
		}
		return;
	}

	try {
		string::iterator it = text_utf8.begin();
		unsigned current = utf8::next(it, text_utf8.end());
		if (releaseLast == true && keyboard.commonKeys(last, current)) {
			// There are common keys between previous and current character
			// Do not immediately press again but give MSX the time to notice
			// that the keys have been released
			releaseLast = false;
		} else {
			// All keys in current char differ from previous char. The new keys
			// can immediately be pressed
			lockKeysMask = keyboard.pressAscii(current, true);
			if (lockKeysMask == 0) {
				last = current;
				releaseLast = true;
				text_utf8.erase(text_utf8.begin(), it);
			}
		}
		reschedule(time);
	} catch (std::exception&) {
		// utf8 encoding error
		text_utf8.clear();
	}
}

void KeyInserter::reschedule(EmuTime::param time)
{
	Clock<15> nextTime(time);
	setSyncPoint(nextTime + 1);
}

const string& KeyInserter::schedName() const
{
	static const string schedName = "KeyInserter";
	return schedName;
}

/*
 * class CapsLockAligner
 *
 * It is used to align MSX CAPS lock status with the host CAPS lock status
 * during the reset of the MSX or after the openMSX window regains focus.
 *
 * It listens to the 'BOOT' event and schedules the real alignment
 * 2 seconds later. Reason is that it takes a while before the MSX
 * reset routine starts monitoring the MSX keyboard.
 *
 * For focus regain, the alignment is done immediately.
 */
CapsLockAligner::CapsLockAligner(EventDistributor& eventDistributor_,
                                 MSXEventDistributor& msxEventDistributor_,
                                 Scheduler& scheduler, Keyboard& keyboard_)
	: Schedulable(scheduler)
	, keyboard(keyboard_)
	, eventDistributor(eventDistributor_)
	, msxEventDistributor(msxEventDistributor_)
{
	eventDistributor.registerEventListener(OPENMSX_BOOT_EVENT,  *this);
	eventDistributor.registerEventListener(OPENMSX_FOCUS_EVENT, *this);
}

CapsLockAligner::~CapsLockAligner()
{
	eventDistributor.unregisterEventListener(OPENMSX_FOCUS_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_BOOT_EVENT,  *this);
}

bool CapsLockAligner::signalEvent(shared_ptr<const Event> event)
{
	EmuTime::param time = getCurrentTime();
	EventType type = event->getType();
	if (type == OPENMSX_FOCUS_EVENT) {
		const FocusEvent& focusEvent = checked_cast<const FocusEvent&>(*event);
		if (focusEvent.getGain() == true) {
			alignCapsLock(time);
		}
	} else if (type == OPENMSX_BOOT_EVENT) {
		Clock<100> now(time);
		setSyncPoint(now + 200);
	}
	return true;
}

void CapsLockAligner::executeUntil(EmuTime::param time, int /*userData*/)
{
	alignCapsLock(time);
}

/*
 * Align MSX caps lock state with host caps lock state
 * WARNING: This function assumes that the MSX will see and
 *          process the caps lock key press.
 *          If MSX misses the key press for whatever reason (e.g.
 *          interrupts are disabled), the caps lock state in this
 *          module will mismatch with the real MSX caps lock state
 * TODO: Find a solution for the above problem. For example by monitoring
 *       the MSX caps-lock LED state.
 */
void CapsLockAligner::alignCapsLock(EmuTime::param time)
{
	bool hostCapsLockOn = ((SDL_GetModState() & KMOD_CAPS) != 0);
	if (keyboard.msxCapsLockOn != hostCapsLockOn) {
		// note: send out another event iso directly calling
		// processCapslockEvent() because we want this to be recorded
		shared_ptr<const Event> event(new KeyDownEvent(Keys::K_CAPSLOCK));
		msxEventDistributor.distributeEvent(event, time);
	}
}

const string& CapsLockAligner::schedName() const
{
	static const string schedName = "CapsLockAligner";
	return schedName;
}


// class KeybDebuggable

KeybDebuggable::KeybDebuggable(MSXMotherBoard& motherBoard, Keyboard& keyboard_)
	: SimpleDebuggable(motherBoard, "keymatrix", "MSX Keyboard Matrix",
	                   Keyboard::NR_KEYROWS)
	, keyboard(keyboard_)
{
}

byte KeybDebuggable::read(unsigned address)
{
	return keyboard.getKeys()[address];
}

void KeybDebuggable::write(unsigned /*address*/, byte /*value*/)
{
	// ignore
}


template<typename Archive>
void KeyInserter::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);
	ar.serialize("text", text_utf8);
	ar.serialize("last", last);
	ar.serialize("lockKeysMask", lockKeysMask);
	ar.serialize("releaseLast", releaseLast);
	ar.serialize("oldCodeKanaLockOn", oldCodeKanaLockOn);
	ar.serialize("oldGraphLockOn", oldGraphLockOn);
	ar.serialize("oldCapsLockOn", oldCapsLockOn);
}

// version 1: Initial version: {userKeyMatrix, dynKeymap, msxmodifiers,
//            msxKeyEventQueue} was intentionally not serialized. The reason
//            was that after a loadstate, you want the MSX keyboard to reflect
//            the state of the host keyboard. So any pressed MSX keys from the
//            time the savestate was created are cleared.
// version 2: For reverse-replay it is important that snapshots contain the
//            full state of the MSX keyboard, so now we do serialize it.
// TODO Is the assumption in version 1 correct (clear keyb state on load)?
//      If it is still useful for 'regular' loadstate, then we could implement
//      it by explicitly clearing the keyb state from the actual loadstate
//      command. (But let's only do this when experience shows it's really
//      better).
template<typename Archive>
void Keyboard::serialize(Archive& ar, unsigned version)
{
	ar.serialize("keyTypeCmd", *keyTypeCmd);
	ar.serialize("cmdKeyMatrix", cmdKeyMatrix);
	ar.serialize("msxCapsLockOn", msxCapsLockOn);
	ar.serialize("msxCodeKanaLockOn", msxCodeKanaLockOn);
	ar.serialize("msxGraphLockOn", msxGraphLockOn);

	if (version >= 2) {
		ar.serialize("userKeyMatrix", userKeyMatrix);
		ar.serialize("dynKeymap", dynKeymap);
		ar.serialize("msxmodifiers", msxmodifiers);
		ar.serialize("msxKeyEventQueue", *msxKeyEventQueue);
	}

	if (ar.isLoader()) {
		// force recalculation of keyMatrix
		// (from cmdKeyMatrix and userKeyMatrix)
		keysChanged = true;
	}
}
INSTANTIATE_SERIALIZE_METHODS(Keyboard);

template<typename Archive>
void MsxKeyEventQueue::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);

	// serialization of deque<shared_ptr<const Event> > is not directly
	// supported by the serialization framework (main problem is the
	// constness, collections of shared_ptr to polymorhpic objects are
	// not a problem). Worked around this by serializing the events in
	// ascii format. (In all practical cases this queue will anyway be
	// empty or contain very few elements).
	//ar.serialize("eventQueue", eventQueue);
	vector<string> eventStrs;
	if (!ar.isLoader()) {
		for (std::deque<shared_ptr<const Event> >::const_iterator it =
		       eventQueue.begin(); it != eventQueue.end(); ++it) {
			eventStrs.push_back((*it)->toString());
		}
	}
	ar.serialize("eventQueue", eventStrs);
	if (ar.isLoader()) {
		assert(eventQueue.empty());
		for (vector<string>::const_iterator it = eventStrs.begin();
		     it != eventStrs.end(); ++it) {
			eventQueue.push_back(InputEventFactory::createInputEvent(*it));
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(MsxKeyEventQueue);


/** Keyboard bindings ****************************************/

// Key-Matrix table
//
// row/bit  7     6     5     4     3     2     1     0
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   0   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   1   |  ;  |  ]  |  [  |  \  |  =  |  -  |  9  |  8  |
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   2   |  B  |  A  | Acc |  /  |  .  |  ,  |  `  |  '  |
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   3   |  J  |  I  |  H  |  G  |  F  |  E  |  D  |  C  |
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   4   |  R  |  Q  |  P  |  O  |  N  |  M  |  L  |  K  |
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   5   |  Z  |  Y  |  X  |  W  |  V  |  U  |  T  |  S  |
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   6   |  F3 |  F2 |  F1 | code| caps|graph| ctrl|shift|
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   7   | ret |selec|  bs | stop| tab | esc |  F5 |  F4 |
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   8   |right| down|  up | left| del | ins | hom |space|
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   9   |  4  |  3  |  2  |  1  |  0  |  /  |  +  |  *  |
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//  10   |  .  |  ,  |  -  |  9  |  8  |  7  |  6  |  5  |
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//  11   |     |     |     |     | 'NO'|     |'YES'|     |
//       +-----+-----+-----+-----+-----+-----+-----+-----+

// Mapping from SDL keys to MSX keys
const byte Keyboard::keyTab[MAX_KEYSYM][2] = {
/* 0000 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {7,0x20},{7,0x08},{0,0x00},{0,0x00},{0,0x00},{7,0x80},{0,0x00},{0,0x00},
/* 0010 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{7,0x04},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0020 */
  {8,0x01},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{2,0x01},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{2,0x04},{1,0x04},{2,0x08},{2,0x10},
/* 0030 */
  {0,0x01},{0,0x02},{0,0x04},{0,0x08},{0,0x10},{0,0x20},{0,0x40},{0,0x80},
  {1,0x01},{1,0x02},{0,0x00},{1,0x80},{0,0x00},{1,0x08},{0,0x00},{0,0x00},
/* 0040 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0050 */
  {0,0x00},{8,0x10},{8,0x20},{8,0x80},{8,0x40},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{1,0x20},{1,0x10},{1,0x40},{0,0x00},{0,0x00},
/* 0060 */
  {2,0x02},{2,0x40},{2,0x80},{3,0x01},{3,0x02},{3,0x04},{3,0x08},{3,0x10},
  {3,0x20},{3,0x40},{3,0x80},{4,0x01},{4,0x02},{4,0x04},{4,0x08},{4,0x10},
/* 0070 */
  {4,0x20},{4,0x40},{4,0x80},{5,0x01},{5,0x02},{5,0x04},{5,0x08},{5,0x10},
  {5,0x20},{5,0x40},{5,0x80},{0,0x00},{0,0x00},{0,0x00},{6,0x04},{8,0x08},
/* 0080 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0090 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00A0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00B0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00C0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00D0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {8,0x02},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00E0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00F0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0100 */
  {9,0x08},{9,0x10},{9,0x20},{9,0x40},{9,0x80},{10,0x01},{10,0x02},{10,0x04},
  {10,0x08},{10,0x10},{10,0x80},{9,0x04},{9,0x01},{10,0x20},{9,0x02},{10,0x40},
/* 0110 */
  {0,0x00},{8,0x20},{8,0x40},{8,0x80},{8,0x10},{8,0x04},{8,0x02},{0,0x00},
  {0,0x00},{0,0x00},{6,0x20},{6,0x40},{6,0x80},{7,0x01},{7,0x02},{0,0x00},
/* 0120 */
  {7,0x40},{7,0x10},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{6,0x08},{0,0x00},{6,0x01},
/* 0130 */
  {6,0x01},{2,0x20},{6,0x02},{6,0x10},{6,0x04},{11,0x08},{11,0x02},{11,0x08},
  {11,0x02},{11,0x02},{11,0x08},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0140 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00}
};

} // namespace openmsx

