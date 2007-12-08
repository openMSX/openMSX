// $Id$

#include "Keyboard.hh"
#include "Observer.hh"
#include "Clock.hh"
#include "EventListener.hh"
#include "EventDistributor.hh"
#include "MSXEventDistributor.hh"
#include "SettingsConfig.hh"
#include "MSXException.hh"
#include "XMLElement.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "MSXCommandController.hh"
#include "RecordedCommand.hh"
#include "CommandException.hh"
#include "InputEvents.hh"
#include "Scheduler.hh"
#include "Unicode.hh"
#include "checked_cast.hh"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class KeyMatrixUpCmd : public RecordedCommand
{
public:
	KeyMatrixUpCmd(MSXCommandController& msxCommandController,
	               MSXEventDistributor& msxEventDistributor,
	               Scheduler& scheduler, Keyboard& keyboard);
	virtual string execute(const vector<string>& tokens, const EmuTime& time);
	virtual string help(const vector<string>& tokens) const;
private:
	Keyboard& keyboard;
};

class KeyMatrixDownCmd : public RecordedCommand
{
public:
	KeyMatrixDownCmd(MSXCommandController& msxCommandController,
	                 MSXEventDistributor& msxEventDistributor,
	                 Scheduler& scheduler, Keyboard& keyboard);
	virtual string execute(const vector<string>& tokens, const EmuTime& time);
	virtual string help(const vector<string>& tokens) const;
private:
	Keyboard& keyboard;
};

class KeyInserter : public RecordedCommand, private Schedulable
{
public:
	KeyInserter(MSXCommandController& msxCommandController,
	            MSXEventDistributor& msxEventDistributor,
	            Scheduler& scheduler, Keyboard& keyboard);

private:
	void type(const string& str);
	void reschedule(const EmuTime& time);

	// Command
	virtual string execute(const vector<string>& tokens, const EmuTime& time);
	virtual string help(const vector<string>& tokens) const;

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const string& schedName() const;

	Keyboard& keyboard;
	Unicode::unicode1_string text;
	bool releaseLast;
	Unicode::unicode1_char last;
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
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const string& schedName() const;

	void alignCapsLock(const EmuTime& time);

	Keyboard& keyboard;
	EventDistributor& eventDistributor;
	MSXEventDistributor& msxEventDistributor;
};


void Keyboard::parseKeymapfile(const byte* buf, unsigned size)
{
	unsigned i = 0;
	while  (i < size) {
		string line;
		bool done = false;
		while (!done && (i < size)) {
			char c = buf[i++];
			switch (c) {
				case '#':
					while ((i < size) &&
					       (buf[i++] != '\n')) {
						// skip till end of line
					}
					done = true;
					break;
				case '\r':
				case '\t':
				case ' ':
					// skip whitespace
					break;
				case '\n':
				case '\0':
					// end of line
					done = true;
					break;
				default:
					line += c;
					break;
			}
		}
		int vkey, row, bit;
		int rdnum = sscanf(line.c_str(), "%i=%i,%i", &vkey, &row, &bit);
		if ( rdnum == 3 && 0 <= vkey && vkey < MAX_KEYSYM
		&& 0 <= row && row < 12 && 0 <= bit && bit < 256 ) {
			keyTab[vkey][0] = row;
			keyTab[vkey][1] = bit;
		}
	}
}

void Keyboard::loadKeymapfile(const string& filename)
{
	try {
		File file(filename);
		byte* buf = file.mmap();
		parseKeymapfile(buf, file.getSize());
	} catch (FileException &e) {
		throw MSXException("Couldn't load keymap file: " + filename);
	}
}

void Keyboard::parseUnicodeKeymapfile(const byte* buf, unsigned size)
{
	unsigned i = 0;
	while  (i < size) {
		string line;
		bool done = false;
		// Read one line from buffer
		// Filter out any white-space
		// Only read until # mark
		while (!done && (i < size)) {
			char c = buf[i++];
			switch (c) {
				case '#':
					while ((i < size) &&
					       (buf[i++] != '\n')) {
						// skip till end of line
					}
					done = true;
					break;
				case '\r':
				case '\t':
				case ' ':
					// skip whitespace
					break;
				case '\n':
				case '\0':
					// end of line
					done = true;
					break;
				default:
					line += c;
					break;
			}
		}
		int unicode, rowcol;
		char modifiers[100];
		byte modmask;
		int rdnum = sscanf(line.c_str(), "%x,%x,%99s", &unicode, &rowcol, modifiers);
		if (rdnum == 2) {
			// there are no modifiers, empty the modifiers string
			modifiers[0] = 0;
			rdnum = 3;
		}
		if (rdnum == 3) {
			if (unicode < 0 || unicode > 65535) {
				throw MSXException("Wrong unicode value in keymap file");
			}
			if (rowcol < 0 || rowcol >= 11*16 ) {
				throw MSXException("Wrong rowcol value in keymap file");
			}
			if ((rowcol & 0x0f) > 7) {
				throw MSXException("Too high column value in keymap file");
			}
			modmask = 0;
			if (strstr(modifiers, "SHIFT") != NULL) {
				modmask |= 1;
			}
			if (strstr(modifiers, "CTRL") != NULL) {
				modmask |= 2;
			}
			if (strstr(modifiers, "GRAPH") != NULL) {
				modmask |= 4;
			}
			if (strstr(modifiers, "CODE" ) != NULL) {
				modmask |= 16;
			}
			// TODO: add support for CAPSLOCK and KANALOCK
			// It is mainly needed for the 'InsertKeys' function when it
			// receives kanji-codes
//			if (strstr(modifiers, "CAPSLOCK") != NULL) {
//				modmask != 32;
//			}
//			if (strstr(modifiers, "KANALOCK") != NULL) {
//				modmask != 64;
//			}
// TODO: use constants for unicodeTab addressing or use a small structure
			unicodeTab[unicode][0] = (rowcol >> 4) & 0x0f; // 0 = row
			unicodeTab[unicode][1] = 1 << (rowcol & 7); // 1 = keymask
			unicodeTab[unicode][2] = modmask; // 2 = modmask
		}
	}
}

void Keyboard::loadUnicodeKeymapfile(const string& filename)
{
	try {
		File file(filename);
		byte* buf = file.mmap();
		parseUnicodeKeymapfile(buf, file.getSize());
	} catch (FileException &e) {
		throw MSXException("Couldn't load unicode keymap file: " + filename);
	}
}

Keyboard::Keyboard(Scheduler& scheduler,
                   MSXCommandController& msxCommandController,
                   EventDistributor& eventDistributor,
                   MSXEventDistributor& msxEventDistributor_,
		   string& keyboardType, bool hasKP, bool keyG)
	: Schedulable(scheduler)
	, msxEventDistributor(msxEventDistributor_)
	, keyMatrixUpCmd  (new KeyMatrixUpCmd  (
		msxCommandController, msxEventDistributor, scheduler, *this))
	, keyMatrixDownCmd(new KeyMatrixDownCmd(
		msxCommandController, msxEventDistributor, scheduler, *this))
	, keyTypeCmd(new KeyInserter(
		msxCommandController, msxEventDistributor, scheduler, *this))
	, capsLockAligner(new CapsLockAligner(
		eventDistributor, msxEventDistributor, scheduler, *this))
	, keyboardSettings(new KeyboardSettings(msxCommandController))
{
	keyGhosting = keyG;
	hasKeypad = hasKP;
	keysChanged = false;
	msxCapsLockOn = false;
	msxmodifiers=0xff;
	memset(keyMatrix,     255, sizeof(keyMatrix));
	memset(cmdKeyMatrix,  255, sizeof(cmdKeyMatrix));
	memset(userKeyMatrix, 255, sizeof(userKeyMatrix));
	memset(dynKeymap, 0, sizeof(dynKeymap));
	memset(unicodeTab, 0, sizeof(unicodeTab));


	SystemFileContext context;

	string keymapFile = keyboardSettings->getKeymapFile().getValueString();
	if (!keymapFile.empty()) {
		loadKeymapfile(context.resolve(keymapFile));
	}

	string unicodemapFilename = "unicodemaps/unicodemap." + keyboardType;
	loadUnicodeKeymapfile(context.resolve(unicodemapFilename));

	keyboardSettings->getKeymapFile().attach(*this);
	msxEventDistributor.registerEventListener(*this);
	// We do not listen for CONSOLE_OFF_EVENTS because rescanning the
	// keyboard can have unwanted side effects
}

Keyboard::~Keyboard()
{
	msxEventDistributor.unregisterEventListener(*this);
	keyboardSettings->getKeymapFile().detach(*this);
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
                           const EmuTime& time)
{
	EventType type = event->getType();
	if ((type == OPENMSX_KEY_DOWN_EVENT) ||
	    (type == OPENMSX_KEY_UP_EVENT)) {
		// Ignore possible console on/off events:
		// we do not rescan the keyboard since this may lead to
		// an unwanted pressing of <return> in MSX after typing
		// "set console off" in the console.
		const KeyEvent& keyEvent = checked_cast<const KeyEvent&>(*event);
		Keys::KeyCode key = static_cast<Keys::KeyCode>
			(int(keyEvent.getKeyCode()) & int(Keys::K_MASK));
		if (type == OPENMSX_KEY_DOWN_EVENT &&
		    keyboardSettings->getTraceKeyPresses().getValue()) {
			fprintf(stderr,
				"Key pressed, unicode codepoint: 0x%04x, SDL info: %s\n",
				keyEvent.getUnicode(),
				Keys::getName(keyEvent.getKeyCode()).c_str());
		}
		if (key == Keys::K_CAPSLOCK) {
			processCapslockEvent(time);
		} else if (key == keyboardSettings->getCodeKanaHostKey().getValue()) {
			processCodeKanaChange(type == OPENMSX_KEY_DOWN_EVENT);
		} else if (key == Keys::K_KP_ENTER) {
			processKeypadEnterKey(type == OPENMSX_KEY_DOWN_EVENT);
		} else {
			processKeyEvent(type == OPENMSX_KEY_DOWN_EVENT, keyEvent);
		}
	}
}

/*
 * Process a change (up or down event) of the CODE/KANA key
 * Currently, it simply presses or de-pressed the key
 * in the MSX keyboard matrix
 * TODO:
 * In future, it should keep track of the KANALOCK state
 * so that the keyboard driver can unlock/lock the KANA key if
 * so required to process a certain unicode character.
 * Please be aware that such special lock/unlock logic is also needed for
 * the Philips VG8010, which has a CODELOCK key in stead of a CODE key
 */
void Keyboard::processCodeKanaChange(bool down)
{
	updateKeyMatrix(down, 6, 0x10);
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
void Keyboard::processCapslockEvent(const EmuTime& time)
{
	msxCapsLockOn = !msxCapsLockOn;
	updateKeyMatrix(true, 6, 0x08);
	Clock<1000> now(time);
	setSyncPoint(now + 100);
}

void Keyboard::executeUntil(const EmuTime& /*time*/, int /*userData*/)
{
	updateKeyMatrix(false, 6, 0x08);
}

const string& Keyboard::schedName() const
{
	static const string schedName = "Keyboard";
	return schedName;
}

void Keyboard::processKeypadEnterKey(bool down)
{
	if (!hasKeypad && !keyboardSettings->getAlwaysEnableKeypad().getValue()) {
		// User entered on host keypad but this MSX model does not have one
		// Ignore the keypress/release
		return;
	}
	int row;
	byte mask;
	if (keyboardSettings->getKpEnterMode().getValue() == KeyboardSettings::MSX_KP_COMMA) {
		row = 10;
		mask = 0x40;
	} else {
		row = 7;
		mask = 0x80;
	}
	updateKeyMatrix(down, row, mask);
}

/*
 * Process an SDL key press/release event. It concerns a
 * special key (e.g. SHIFT, UP, DOWN, F1, F2, ...) that can not
 * be unambiguously derived from a unicode character;
 *  Map the SDL key to an equivalent MSX key press/release event
 */
void Keyboard::processSdlKey(bool down, int key)
{
	if (key < MAX_KEYSYM) {
		int row=keyTab[key][0];
		byte mask=keyTab[key][1];
		updateKeyMatrix(down, row, mask);
	}
}

/*
 * Update the MSX keyboard matrix
 */
void Keyboard::updateKeyMatrix(bool down, int row, byte mask)
{
	if (down) {
		// Key pressed: reset bit in user keyMatrix
		userKeyMatrix[row] &= ~mask;
		if (row == 6) {
			// Keep track of the MSX modifiers (CTRL, GRAPH, CODE, SHIFT)
			// The MSX modifiers in row 6 of the matrix sometimes get
			// overruled by the unicode character processing, in
			// which case the unicode processing must be able to restore
			// them to the real key-combinations pressed by the user
			msxmodifiers &= ~(mask & 0x17);
		}
	} else {
		// Key released: set bit in keyMatrix
		userKeyMatrix[row] |= mask;
		if (row == 6) {
			msxmodifiers |= (mask & 0x17);
		}
	}
	keysChanged = true; // do ghosting at next getKeys()
}

/*
 * Process an SDL key event;
 *  Check if it is a special key, in which case it can be directly
 *  mapped to the MSX matrix.
 *  Otherwise, retrieve the unicode character value for the event
 *  and map the unicode character to the key-combination that must
 *  be pressed to generate the equivalent character on the MSX
 */
void Keyboard::processKeyEvent(bool down, const KeyEvent& keyEvent)
{
	Keys::KeyCode keyCode = keyEvent.getKeyCode();
	Keys::KeyCode key = static_cast<Keys::KeyCode>
		(int(keyCode) & int(Keys::K_MASK));
	Unicode::unicode1_char unicode;

	bool isOnKeypad = (
		(key >= Keys::K_KP0 && key <= Keys::K_KP9) ||
		(key == Keys::K_KP_PERIOD) ||
		(key == Keys::K_KP_DIVIDE) ||
		(key == Keys::K_KP_MULTIPLY) ||
		(key == Keys::K_KP_MINUS) ||
		(key == Keys::K_KP_PLUS)
	);

	if (isOnKeypad && !hasKeypad &&
	    !keyboardSettings->getAlwaysEnableKeypad().getValue()) {
		// User entered on host keypad but this MSX model does not have one
		// Ignore the keypress/release
		return;
	}

	if (down) {
		if ((userKeyMatrix[6] & 2) == 0 || isOnKeypad) {
			// CTRL-key is active or user entered a key on numeric
			// keypad. This maps to same unicode as some other
			// key combinations (e.g. digit on main keyboard or TAB/DEL)
			// Use unicode to handle the more common combination
			// and use direct matrix to matrix mapping for the exceptional
			// cases (CTRL+character or numeric keypad usage)
			unicode = 0;
		} else {
			unicode = keyEvent.getUnicode();
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
			// (e.g. altr-gr + = on azerty keyboard)
			// Perform direct SDL matrix to MSX matrix mapping
			// But only when it is not a first keystroke of a
			// composed key
			if ((keyCode & Keys::KM_MODE) == 0) {
				processSdlKey(down, key);
			}
		} else {
			// It is a unicode character; map it to the right key-combination
			pressUnicodeByUser(unicode, key, true);
		}
	} else {
		// key was released
		if (key < MAX_KEYSYM) {
			unicode=dynKeymap[key]; // Get the unicode that was derived from this key
		} else {
			unicode=0;
		}
		if (unicode == 0) {
			// It was a special key, perform matrix to matrix mapping
			// But only when it is not a first keystroke of a
			// composed key
			if ((keyCode & Keys::KM_MODE) == 0) {
				processSdlKey(down, key);
			}
		} else {
			// It was a unicode character; map it to the right key-combination
			pressUnicodeByUser(unicode, key, false);
		}
	}
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
	bool changedSomething;
	do {
		changedSomething = false;
		for (unsigned i = 0; i < NR_KEYROWS - 1; i++) {
			byte row1 = keyMatrix[i];
			for (unsigned j = i + 1; j < NR_KEYROWS; j++) {
				byte row2 = keyMatrix[j];
				if ((row1 != row2) && ((row1 | row2) != 0xff)) {
					// not same and some common zero's
					//  --> inherit other zero's
					byte newRow = row1 & row2;
					keyMatrix[i] = newRow;
					keyMatrix[j] = newRow;
					row1 = newRow;
					changedSomething = true;
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
void Keyboard::pressUnicodeByUser(Unicode::unicode1_char unicode, int key, bool down)
{
	byte shiftkeymask = ( key >= Keys::K_A && key <= Keys::K_Z) ? 0x00 : 0x01;
	byte row = unicodeTab[unicode][0];
	byte mask = unicodeTab[unicode][1];
	byte modmask = unicodeTab[unicode][2];
	if (down) {
		userKeyMatrix[row] &= ~mask;
		userKeyMatrix[6] |= shiftkeymask;
		userKeyMatrix[6] &= ~(modmask & (0xfe | shiftkeymask));
	} else {
		userKeyMatrix[row] |= mask;
		// Do not simply unpress code, graph, ctrl (and shift)
		// but restore them to the values currently pressed
		// by the user
		userKeyMatrix[6] &= (msxmodifiers | ~0x17);
		userKeyMatrix[6] |= (msxmodifiers & 0x17);
	}
	keysChanged = true;
}

/*
 * Press an ASCII character. It is used by the 'Insert characters'
 * function that is exposed to the console.
 * The characters are inserted in a separate keyboard matrix, to prevent
 * interference with the keypresses of the user on the MSX itself
 */
void Keyboard::pressAscii(Unicode::unicode1_char unicode, bool down)
{
	byte row = unicodeTab[unicode][0];
	byte mask = unicodeTab[unicode][1];
	byte modmask = unicodeTab[unicode][2];
	if (down) {
		if (keyboardSettings->getTraceKeyPresses().getValue()) {
			fprintf(stderr,
				"Key pasted, unicode codepoint: 0x%04x, row: %02d, mask: %02x, modmask: %02x\n",
				unicode, row, mask, modmask);
		}
		cmdKeyMatrix[row] &= ~mask;
		cmdKeyMatrix[6] &= ~modmask;
	} else {
		cmdKeyMatrix[row] |= mask;
		cmdKeyMatrix[6] |= modmask;
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
bool Keyboard::commonKeys(Unicode::unicode1_char unicode1, Unicode::unicode1_char unicode2)
{
	// get row / mask of key (note: ignore modifier mask)
	byte row1 = unicodeTab[unicode1][0];
	byte mask1 = unicodeTab[unicode1][1];

	byte row2 = unicodeTab[unicode2][0];
	byte mask2 = unicodeTab[unicode2][1];

	// common key on common row?
	return ((row1 == row2) && (mask1 & mask2));
}

void Keyboard::update(const Setting& setting)
{
	assert(&setting == &keyboardSettings->getKeymapFile());
	(void)setting;

	string keymapFile = keyboardSettings->getKeymapFile().getValueString();
	if (!keymapFile.empty()) {
		SystemFileContext context;
		loadKeymapfile(context.resolve(keymapFile));
	}
}

// class KeyMatrixUpCmd

KeyMatrixUpCmd::KeyMatrixUpCmd(MSXCommandController& msxCommandController,
		MSXEventDistributor& msxEventDistributor,
		Scheduler& scheduler, Keyboard& keyboard_)
	: RecordedCommand(msxCommandController, msxEventDistributor,
		scheduler, "keymatrixup")
	, keyboard(keyboard_)
{
}

string KeyMatrixUpCmd::execute(const vector<string>& tokens, const EmuTime& /*time*/)
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

KeyMatrixDownCmd::KeyMatrixDownCmd(MSXCommandController& msxCommandController,
		MSXEventDistributor& msxEventDistributor,
		Scheduler& scheduler, Keyboard& keyboard_)
	: RecordedCommand(msxCommandController, msxEventDistributor,
		scheduler, "keymatrixdown")
	, keyboard(keyboard_)
{
}

string KeyMatrixDownCmd::execute(const vector<string>& tokens, const EmuTime& /*time*/)
{
	return keyboard.processCmd(tokens, false);
}

string KeyMatrixDownCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText=
		"keymatrixdown <row> <bitmask>  press a key in the keyboardmatrix\n";
	return helpText;
}


// class KeyInserter

KeyInserter::KeyInserter(MSXCommandController& msxCommandController,
		MSXEventDistributor& msxEventDistributor,
		Scheduler& scheduler, Keyboard& keyboard_)
	: RecordedCommand(msxCommandController, msxEventDistributor,
		scheduler, "type")
	, Schedulable(scheduler)
	, keyboard(keyboard_)
	, releaseLast(false)
{
}

string KeyInserter::execute(const vector<string>& tokens, const EmuTime& /*time*/)
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
	if (text.empty()) {
		reschedule(getScheduler().getCurrentTime());
	}
	text += Unicode::utf8ToUnicode1(str);
}

void KeyInserter::executeUntil(const EmuTime& time, int /*userData*/)
{
	if (releaseLast) {
		keyboard.pressAscii(last, false); // release previous character
	}
	if (text.empty()) {
		releaseLast = false;
		return;
	}
	Unicode::unicode1_char current = text[0];
	if (releaseLast == true && keyboard.commonKeys(last, current)) {
		// There are common keys between previous and current character
		// Do not immediately press again but give MSX the time to notice
		// that the keys have been released
		releaseLast = false;
	} else {
		// All keys in current char differ from previous char. The new keys
		// can immediately be pressed
		keyboard.pressAscii(current, true);
		last = current;
		releaseLast = true;
		text = text.substr(1);
	}
	reschedule(time);
}

void KeyInserter::reschedule(const EmuTime& time)
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
	const EmuTime& time = getScheduler().getCurrentTime();
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

void CapsLockAligner::executeUntil(const EmuTime& time, int /*userData*/)
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
void CapsLockAligner::alignCapsLock(const EmuTime& time)
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
byte Keyboard::keyTab[MAX_KEYSYM][2] = {
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

