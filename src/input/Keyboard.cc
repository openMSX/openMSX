#include "Keyboard.hh"
#include "Keys.hh"
#include "EventDistributor.hh"
#include "InputEventFactory.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "MSXMotherBoard.hh"
#include "ReverseManager.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "InputEvents.hh"
#include "StateChange.hh"
#include "utf8_checked.hh"
#include "checked_cast.hh"
#include "unreachable.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "serialize_meta.hh"
#include "openmsx.hh"
#include "outer.hh"
#include "stl.hh"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdarg>

using std::string;
using std::vector;
using std::shared_ptr;
using std::make_shared;

namespace openmsx {

static const byte SHIFT_MASK = 0x01;
static const byte CTRL_MASK  = 0x02;
static const byte GRAPH_MASK = 0x04;
static const byte CAPS_MASK  = 0x08;
static const byte CODE_MASK  = 0x10;

class KeyMatrixState final : public StateChange
{
public:
	KeyMatrixState() {} // for serialize
	KeyMatrixState(EmuTime::param time_, byte row_, byte press_, byte release_)
		: StateChange(time_)
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

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("row", row);
		ar.serialize("press", press);
		ar.serialize("release", release);
	}
private:
	byte row, press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, KeyMatrixState, "KeyMatrixState");


static bool checkSDLReleasesCapslock()
{
	const SDL_version* v = SDL_Linked_Version();
	if (SDL_VERSIONNUM(v->major, v->minor, v->patch) < SDL_VERSIONNUM(1, 2, 14)) {
		// Feature was introduced in SDL 1.2.14.
		return false;
	} else {
		// Check whether feature was enabled by envvar.
		char *val = SDL_getenv("SDL_DISABLE_LOCK_KEYS");
		return val && (strcmp(val, "1") == 0 || strcmp(val, "2") == 0);
	}
}

Keyboard::Keyboard(MSXMotherBoard& motherBoard,
                   Scheduler& scheduler_,
                   CommandController& commandController_,
                   EventDistributor& eventDistributor,
                   MSXEventDistributor& msxEventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   string_ref keyboardType, bool hasKP, bool hasYNKeys,
                   bool keyGhosting_, bool keyGhostSGCprotected,
                   bool codeKanaLocks_, bool graphLocks_)
	: Schedulable(scheduler_)
	, commandController(commandController_)
	, msxEventDistributor(msxEventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, keyMatrixUpCmd  (commandController, stateChangeDistributor, scheduler_)
	, keyMatrixDownCmd(commandController, stateChangeDistributor, scheduler_)
	, keyTypeCmd      (commandController, stateChangeDistributor, scheduler_)
	, capsLockAligner(eventDistributor, scheduler_)
	, keyboardSettings(commandController)
	, msxKeyEventQueue(scheduler_, commandController.getInterpreter())
	, keybDebuggable(motherBoard)
	, unicodeKeymap(keyboardType)
	, hasKeypad(hasKP)
	, hasYesNoKeys(hasYNKeys)
	, keyGhosting(keyGhosting_)
	, keyGhostingSGCprotected(keyGhostSGCprotected)
	, codeKanaLocks(codeKanaLocks_)
	, graphLocks(graphLocks_)
	, sdlReleasesCapslock(checkSDLReleasesCapslock())
{
	// SDL version >= 1.2.14 releases caps-lock key when SDL_DISABLED_LOCK_KEYS
	// environment variable is already set in main.cc (because here it
	// would be too late)

	keysChanged = false;
	msxCapsLockOn = false;
	msxCodeKanaLockOn = false;
	msxGraphLockOn = false;
	msxmodifiers = 0xff;
	memset(keyMatrix,     255, sizeof(keyMatrix));
	memset(cmdKeyMatrix,  255, sizeof(cmdKeyMatrix));
	memset(userKeyMatrix, 255, sizeof(userKeyMatrix));
	memset(hostKeyMatrix, 255, sizeof(hostKeyMatrix));
	memset(dynKeymap,       0, sizeof(dynKeymap));

	msxEventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
	// We do not listen for CONSOLE_OFF_EVENTS because rescanning the
	// keyboard can have unwanted side effects

	motherBoard.getReverseManager().registerKeyboard(*this);
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

void Keyboard::transferHostKeyMatrix(const Keyboard& source)
{
	// This mechanism exists to solve the following problem:
	//   - play a game where the spacebar is constantly pressed (e.g.
	//     Road Fighter)
	//   - go back in time (press the reverse hotkey) while keeping the
	//     spacebar pressed
	//   - interrupt replay by pressing the cursor keys, still while
	//     keeping spacebar pressed
	// At the moment replay is interrupted, we need to resynchronize the
	// msx keyboard with the host keyboard. In the past we assumed the host
	// keyboard had no keys pressed. But this is wrong in the above
	// scenario. Now we remember the state of the host keyboard and
	// transfer that to the new keyboard(s) that get created for reverese.
	// When replay is stopped we restore this host keyboard state, see
	// stopReplay().

	for (unsigned row = 0; row < NR_KEYROWS; ++row) {
		hostKeyMatrix[row] = source.hostKeyMatrix[row];
	}
}

/* Received an MSX event
 * Following events get processed:
 *  OPENMSX_KEY_DOWN_EVENT
 *  OPENMSX_KEY_UP_EVENT
 */
void Keyboard::signalEvent(const shared_ptr<const Event>& event,
                           EmuTime::param time)
{
	EventType type = event->getType();
	if ((type == OPENMSX_KEY_DOWN_EVENT) ||
	    (type == OPENMSX_KEY_UP_EVENT)) {
		// Ignore possible console on/off events:
		// we do not rescan the keyboard since this may lead to
		// an unwanted pressing of <return> in MSX after typing
		// "set console off" in the console.
		msxKeyEventQueue.process_asap(time, event);
	}
}

void Keyboard::signalStateChange(const shared_ptr<StateChange>& event)
{
	auto kms = dynamic_cast<KeyMatrixState*>(event.get());
	if (!kms) return;

	userKeyMatrix[kms->getRow()] &= ~kms->getPress();
	userKeyMatrix[kms->getRow()] |=  kms->getRelease();
	keysChanged = true; // do ghosting at next getKeys()
}

void Keyboard::stopReplay(EmuTime::param time)
{
	for (unsigned row = 0; row < NR_KEYROWS; ++row) {
		changeKeyMatrixEvent(time, row, hostKeyMatrix[row]);
	}
	msxmodifiers = 0xff;
	msxKeyEventQueue.clear();
	memset(dynKeymap, 0, sizeof(dynKeymap));
}

void Keyboard::pressKeyMatrixEvent(EmuTime::param time, byte row, byte press)
{
	assert(press);
	if (((hostKeyMatrix[row] & press) == 0) &&
	    ((userKeyMatrix[row] & press) == 0)) {
		// Won't have any effect, ignore.
		return;
	}
	changeKeyMatrixEvent(time, row, hostKeyMatrix[row] & ~press);
}
void Keyboard::releaseKeyMatrixEvent(EmuTime::param time, byte row, byte release)
{
	assert(release);
	if (((hostKeyMatrix[row] & release) == release) &&
	    ((userKeyMatrix[row] & release) == release)) {
		// Won't have any effect, ignore.
		// Test scenario: during replay, exit the openmsx console with
		// the 'toggle console' command. The 'enter,release' event will
		// end up here. But it shouldn't stop replay.
		return;
	}
	changeKeyMatrixEvent(time, row, hostKeyMatrix[row] | release);
}

void Keyboard::changeKeyMatrixEvent(EmuTime::param time, byte row, byte newValue)
{
	// This method already updates hostKeyMatrix[],
	// userKeyMatrix[] will soon be updated via KeyMatrixState events.
	hostKeyMatrix[row] = newValue;

	byte diff = userKeyMatrix[row] ^ newValue;
	if (diff == 0) return;
	byte press   = userKeyMatrix[row] & diff;
	byte release = newValue           & diff;
	stateChangeDistributor.distributeNew(make_shared<KeyMatrixState>(
		time, row, press, release));
}

bool Keyboard::processQueuedEvent(const Event& event, EmuTime::param time)
{
	bool insertCodeKanaRelease = false;
	auto& keyEvent = checked_cast<const KeyEvent&>(event);
	bool down = event.getType() == OPENMSX_KEY_DOWN_EVENT;
	auto key = static_cast<Keys::KeyCode>(
		int(keyEvent.getKeyCode()) & int(Keys::K_MASK));
	if (down) {
		// TODO: refactor debug(...) method to expect a std::string and then adapt
		// all invocations of it to provide a properly formatted string, using the C++
		// features for it.
		// Once that is done, debug(...) can pass the c_str() version of that string
		// to ad_printf(...) so that I don't have to make an explicit ad_printf(...)
		// invocation for each debug(...) invocation
		ad_printf("Key pressed, unicode: 0x%04x, keyCode: 0x%05x, keyName: %s\n",
		      keyEvent.getUnicode(),
		      keyEvent.getKeyCode(),
		      Keys::getName(keyEvent.getKeyCode()).c_str());
		debug("Key pressed, unicode: 0x%04x, keyCode: 0x%05x, keyName: %s\n",
		      keyEvent.getUnicode(),
		      keyEvent.getKeyCode(),
		      Keys::getName(keyEvent.getKeyCode()).c_str());
	} else {
		ad_printf("Key released, unicode: 0x%04x, keyCode: 0x%05x, keyName: %s\n",
		      keyEvent.getUnicode(),
		      keyEvent.getKeyCode(),
		      Keys::getName(keyEvent.getKeyCode()).c_str());
		debug("Key released, unicode: 0x%04x, keyCode: 0x%05x, keyName: %s\n",
		      keyEvent.getUnicode(),
		      keyEvent.getKeyCode(),
		      Keys::getName(keyEvent.getKeyCode()).c_str());
	}
	if (key == keyboardSettings.getDeadkeyHostKey(0) &&
	    keyboardSettings.getMappingMode() == KeyboardSettings::CHARACTER_MAPPING) {
		processDeadKeyEvent(0, time, down);
	} else if (key == keyboardSettings.getDeadkeyHostKey(1) &&
	    keyboardSettings.getMappingMode() == KeyboardSettings::CHARACTER_MAPPING) {
		processDeadKeyEvent(1, time, down);
	} else if (key == keyboardSettings.getDeadkeyHostKey(2) &&
	    keyboardSettings.getMappingMode() == KeyboardSettings::CHARACTER_MAPPING) {
		processDeadKeyEvent(2, time, down);
	} else if (key == Keys::K_CAPSLOCK) {
		processCapslockEvent(time, down);
	} else if (key == keyboardSettings.getCodeKanaHostKey()) {
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
 * Process deadkey N by pressing or releasing the deadkey
 * at the correct location in the keyboard matrix
 */
void Keyboard::processDeadKeyEvent(unsigned n, EmuTime::param time, bool down)
{
	UnicodeKeymap::KeyInfo deadkey = unicodeKeymap.getDeadkey(n);
	if (deadkey.keymask) {
		updateKeyMatrix(time, down, deadkey.row, deadkey.keymask);
	}
}

/*
 * Process a change event of the CAPSLOCK *STATUS*;
 *  SDL up to version 1.2.13 sends a CAPSLOCK press event at the moment that
 *  the host CAPSLOCK status goes 'on' and it sends the release event only when
 *  the host CAPSLOCK status goes 'off'. However, the emulated MSX must see a
 *  press and release event when CAPSLOCK status goes on and another press and
 *  release event when it goes off again. This is achieved by pressing CAPSLOCK
 *  key at the moment that the host CAPSLOCK status changes and releasing the
 *  CAPSLOCK key shortly after (via a timed event)
 *
 * SDL as of version 1.2.14 can send a press and release event at the moment
 * that the user presses and releases the CAPS lock. Though, this changed
 * behaviour is only enabled when a special environment variable is set.
 *
 * This version of openMSX supports both behaviours; when SDL version is at
 * least 1.2.14, it will set the environment variable to trigger the new
 * behaviour and simply process the press and release events as they come in.
 * For older SDL versions, it will still treat each change as a press that must
 * be followed by a scheduled release event
 */
void Keyboard::processCapslockEvent(EmuTime::param time, bool down)
{
	if (sdlReleasesCapslock) {
		debug("Changing CAPS lock state according to SDL request\n");
		if (down) {
			msxCapsLockOn = !msxCapsLockOn;
		}
		updateKeyMatrix(time, down, 6, CAPS_MASK);
	} else {
		debug("Pressing CAPS lock and scheduling a release\n");
		msxCapsLockOn = !msxCapsLockOn;
		updateKeyMatrix(time, true, 6, CAPS_MASK);
		setSyncPoint(time + EmuDuration::hz(10)); // 0.1s (in MSX time)
	}
}

void Keyboard::executeUntil(EmuTime::param time)
{
	debug("Releasing CAPS lock\n");
	updateKeyMatrix(time, false, 6, CAPS_MASK);
}

void Keyboard::processKeypadEnterKey(EmuTime::param time, bool down)
{
	if (!hasKeypad && !keyboardSettings.getAlwaysEnableKeypad()) {
		// User entered on host keypad but this MSX model does not have one
		// Ignore the keypress/release
		return;
	}
	int row;
	byte mask;
	if (keyboardSettings.getKpEnterMode() == KeyboardSettings::MSX_KP_COMMA) {
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
		int row   = keyTab[key] >> 4;
		byte mask = 1 << (keyTab[key] & 0xf);
		if (keyTab[key] == 0xff) assert(mask == 0);

		if ((row == 11) && !hasYesNoKeys) {
			// do not process row 11 if we have no Yes/No keys
			return;
		}
		if (mask) {
			updateKeyMatrix(time, down, row, mask);
		}
	}
}

/*
 * Update the MSX keyboard matrix
 */
void Keyboard::updateKeyMatrix(EmuTime::param time, bool down, int row, byte mask)
{
	assert(mask);
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
	auto key = static_cast<Keys::KeyCode>(
		int(keyCode) & int(Keys::K_MASK));
	unsigned unicode;

	bool isOnKeypad = (
		(key >= Keys::K_KP0 && key <= Keys::K_KP9) ||
		(key == Keys::K_KP_PERIOD) ||
		(key == Keys::K_KP_DIVIDE) ||
		(key == Keys::K_KP_MULTIPLY) ||
		(key == Keys::K_KP_MINUS) ||
		(key == Keys::K_KP_PLUS));

	if (isOnKeypad && !hasKeypad &&
	    !keyboardSettings.getAlwaysEnableKeypad()) {
		// User entered on host keypad but this MSX model does not have one
		// Ignore the keypress/release
		return false;
	}

	if (down) {
		if (/*___(userKeyMatrix[6] & 2) == 0 || */
		    isOnKeypad ||
		    keyboardSettings.getMappingMode() == KeyboardSettings::KEY_MAPPING) {
			// /*CTRL-key is active,*/ user entered a key on numeric
			// keypad or the driver is in KEY mapping mode.
			// First /*two*/ option/*s*/ (/*CTRL key active,*/ keypad keypress) maps
			// to same unicode as some other key combinations (e.g. digit
			// on main keyboard or TAB/DEL)
			// Use unicode to handle the more common combination
			// and use direct matrix to matrix mapping for the exceptional
			// cases (/*CTRL+character or*/ numeric keypad usage)
			unicode = 0;
#if defined(__APPLE__)
		} else if ((keyCode & (Keys::K_MASK | Keys::KM_META))
				== (Keys::K_I | Keys::KM_META)) {
			// Apple keyboards don't have an Insert key, use Cmd+I as an alternative.
			keyCode = key = Keys::K_INSERT;
			unicode = 0;
#endif
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
			insertCodeKanaRelease = pressUnicodeByUser(time, unicode, true);
		}
	} else {
		// key was released
#if defined(__APPLE__)
		if ((keyCode & (Keys::K_MASK | Keys::KM_META))
				== (Keys::K_I | Keys::KM_META)) {
			keyCode = key = Keys::K_INSERT;
		}
#endif
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
			pressUnicodeByUser(time, unicode, false);
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

void Keyboard::processCmd(Interpreter& interp, array_ref<TclObject> tokens, bool up)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	unsigned row  = tokens[1].getInt(interp);
	unsigned mask = tokens[2].getInt(interp);
	if (row >= NR_KEYROWS) {
		throw CommandException("Invalid row");
	}
	if (mask >= 256) {
		throw CommandException("Invalid mask");
	}
	if (up) {
		cmdKeyMatrix[row] |= mask;
	} else {
		cmdKeyMatrix[row] &= ~mask;
	}
	keysChanged = true;
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
bool Keyboard::pressUnicodeByUser(EmuTime::param time, unsigned unicode, bool down)
{
	bool insertCodeKanaRelease = false;
	UnicodeKeymap::KeyInfo keyInfo = unicodeKeymap.get(unicode);
	if (keyInfo.keymask == 0) {
		return insertCodeKanaRelease;
	}
	if (down) {
		if (codeKanaLocks &&
		    keyboardSettings.getAutoToggleCodeKanaLock() &&
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
			assert(keyInfo.keymask);
			pressKeyMatrixEvent(time, keyInfo.row, keyInfo.keymask);

			byte modmask = keyInfo.modmask & ~CAPS_MASK;
			if (codeKanaLocks) modmask &= ~CODE_MASK;
			if (graphLocks)    modmask &= ~GRAPH_MASK;
			if (('A' <= unicode && unicode <= 'Z') || ('a' <= unicode && unicode <= 'z')) {
				// for a-z and A-Z, leave shift unchanged, this to cater
				// for difference in behaviour between host and emulated
				// machine with respect to how the combination of CAPSLOCK
				// and shift-key is interpreted for these characters.
				// Note that other modifiers are only pressed, never released
				byte press = modmask & ~SHIFT_MASK;
				if (press) {
					pressKeyMatrixEvent(time, 6, press);
				}
			} else {
				// for other keys, set shift according to modmask
				// so also release shift when required (other
				// modifiers are only pressed, never released)
				byte newRow = (userKeyMatrix[6] | SHIFT_MASK) & ~modmask;
				changeKeyMatrixEvent(time, 6, newRow);
			}
		}
	} else {
		assert(keyInfo.keymask);
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
	UnicodeKeymap::KeyInfo keyInfo = unicodeKeymap.get(unicode);
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
	UnicodeKeymap::KeyInfo keyInfo1 = unicodeKeymap.get(unicode1);
	UnicodeKeymap::KeyInfo keyInfo2 = unicodeKeymap.get(unicode2);

	return ((keyInfo1.row == keyInfo2.row) &&
	        (keyInfo1.keymask & keyInfo2.keymask));
}

void Keyboard::debug(const char* format, ...)
{
	if (keyboardSettings.getTraceKeyPresses()) {
		va_list args;
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
	}
}


// class KeyMatrixUpCmd

Keyboard::KeyMatrixUpCmd::KeyMatrixUpCmd(
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
		scheduler_, "keymatrixup")
{
}

void Keyboard::KeyMatrixUpCmd::execute(
	array_ref<TclObject> tokens, TclObject& /*result*/, EmuTime::param /*time*/)
{
	auto& keyboard = OUTER(Keyboard, keyMatrixUpCmd);
	return keyboard.processCmd(getInterpreter(), tokens, true);
}

string Keyboard::KeyMatrixUpCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText =
		"keymatrixup <row> <bitmask>  release a key in the keyboardmatrix\n";
	return helpText;
}


// class KeyMatrixDownCmd

Keyboard::KeyMatrixDownCmd::KeyMatrixDownCmd(CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
		scheduler_, "keymatrixdown")
{
}

void Keyboard::KeyMatrixDownCmd::execute(array_ref<TclObject> tokens,
                               TclObject& /*result*/, EmuTime::param /*time*/)
{
	auto& keyboard = OUTER(Keyboard, keyMatrixDownCmd);
	return keyboard.processCmd(getInterpreter(), tokens, false);
}

string Keyboard::KeyMatrixDownCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText =
		"keymatrixdown <row> <bitmask>  press a key in the keyboardmatrix\n";
	return helpText;
}


// class MsxKeyEventQueue

Keyboard::MsxKeyEventQueue::MsxKeyEventQueue(
		Scheduler& scheduler_, Interpreter& interp_)
	: Schedulable(scheduler_)
	, interp(interp_)
{
}

void Keyboard::MsxKeyEventQueue::process_asap(
	EmuTime::param time, const shared_ptr<const Event>& event)
{
	bool processImmediately = eventQueue.empty();
	eventQueue.push_back(event);
	if (processImmediately) {
		executeUntil(time);
	}
}

void Keyboard::MsxKeyEventQueue::clear()
{
	eventQueue.clear();
	removeSyncPoint();
}

void Keyboard::MsxKeyEventQueue::executeUntil(EmuTime::param time)
{
	// Get oldest event from the queue and process it
	shared_ptr<const Event> event = eventQueue.front();
	auto& keyboard = OUTER(Keyboard, msxKeyEventQueue);
	bool insertCodeKanaRelease = keyboard.processQueuedEvent(*event, time);

	if (insertCodeKanaRelease) {
		// The processor pressed the CODE/KANA key
		// Schedule a CODE/KANA release event, to be processed
		// before any of the other events in the queue
		eventQueue.push_front(make_shared<KeyUpEvent>(
			keyboard.keyboardSettings.getCodeKanaHostKey()));
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
		setSyncPoint(time + EmuDuration::hz(15));
	}
}


// class KeyInserter

Keyboard::KeyInserter::KeyInserter(
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
		scheduler_, "type_via_keyboard")
	, Schedulable(scheduler_)
	, lockKeysMask(0)
	, releaseLast(false)
{
	// avoid UMR
	last = 0;
	oldCodeKanaLockOn = false;
	oldGraphLockOn = false;
	oldCapsLockOn = false;
	releaseBeforePress = false;
	typingFrequency = 15;
}

void Keyboard::KeyInserter::execute(
	array_ref<TclObject> tokens, TclObject& /*result*/, EmuTime::param /*time*/)
{
	if (tokens.size() < 2) {
		throw SyntaxError();
	}

	releaseBeforePress = false;
	typingFrequency = 15;

	// for full backwards compatibility: one option means type it...
	if (tokens.size() == 2) {
		type(tokens[1].getString());
		return;
	}

        vector<string_ref> arguments;
	for (unsigned i = 1; i < tokens.size(); ++i) {
		string_ref t = tokens[i].getString();
		if (t == "-release") {
			releaseBeforePress = true;
		} else if (t == "-freq") {
			if (++i == tokens.size()) {
				throw CommandException("Missing argument");
			}
			int tmp = tokens[i].getInt(getInterpreter());
			if (tmp <= 0) {
				throw CommandException("Wrong argument for -freq (should be a positive number)");
			}
			typingFrequency = tmp;
		} else {
			arguments.push_back(t);
		}
	}

	if (arguments.size() != 1) throw SyntaxError();

	type(arguments[0]);
}

string Keyboard::KeyInserter::help(const vector<string>& /*tokens*/) const
{
	static const string helpText = "Type a string in the emulated MSX.\n" \
		"Use -release to make sure the keys are always released before typing new ones (necessary for some game input routines, but in general, this means typing is twice as slow).\n" \
		"Use -freq to tweak how fast typing goes and how long the keys will be pressed (and released in case -release was used). Keys will be typed at the given frequency and will remain pressed/released for 1/freq seconds";
	return helpText;
}

void Keyboard::KeyInserter::tabCompletion(vector<string>& tokens) const
{
	vector<const char*> options;
	if (!contains(tokens, "-release")) {
		options.push_back("-release");
	}
	if (!contains(tokens, "-freq")) {
		options.push_back("-freq");
	}
	completeString(tokens, options);
}

void Keyboard::KeyInserter::type(string_ref str)
{
	if (str.empty()) {
		return;
	}
	auto& keyboard = OUTER(Keyboard, keyTypeCmd);
	oldCodeKanaLockOn = keyboard.msxCodeKanaLockOn;
	oldGraphLockOn = keyboard.msxGraphLockOn;
	oldCapsLockOn = keyboard.msxCapsLockOn;
	if (text_utf8.empty()) {
		reschedule(getCurrentTime());
	}
	text_utf8.append(str.data(), str.size());
}

void Keyboard::KeyInserter::executeUntil(EmuTime::param time)
{
	auto& keyboard = OUTER(Keyboard, keyTypeCmd);
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
		auto it = begin(text_utf8);
		unsigned current = utf8::next(it, end(text_utf8));
		if (releaseLast == true && (releaseBeforePress || keyboard.commonKeys(last, current))) {
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
				text_utf8.erase(begin(text_utf8), it);
			}
			if (releaseBeforePress) releaseLast = true;
		}
		reschedule(time);
	} catch (std::exception&) {
		// utf8 encoding error
		text_utf8.clear();
	}
}

void Keyboard::KeyInserter::reschedule(EmuTime::param time)
{
	setSyncPoint(time + EmuDuration::hz(typingFrequency));
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
Keyboard::CapsLockAligner::CapsLockAligner(
		EventDistributor& eventDistributor_,
		Scheduler& scheduler_)
	: Schedulable(scheduler_)
	, eventDistributor(eventDistributor_)
{
	state = IDLE;
	eventDistributor.registerEventListener(OPENMSX_BOOT_EVENT,  *this);
	eventDistributor.registerEventListener(OPENMSX_FOCUS_EVENT, *this);
}

Keyboard::CapsLockAligner::~CapsLockAligner()
{
	eventDistributor.unregisterEventListener(OPENMSX_FOCUS_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_BOOT_EVENT,  *this);
}

int Keyboard::CapsLockAligner::signalEvent(const shared_ptr<const Event>& event)
{
	if (state == IDLE) {
		EmuTime::param time = getCurrentTime();
		EventType type = event->getType();
		if (type == OPENMSX_FOCUS_EVENT) {
			alignCapsLock(time);
		} else if (type == OPENMSX_BOOT_EVENT) {
			state = MUST_ALIGN_CAPSLOCK;
			setSyncPoint(time + EmuDuration::sec(2)); // 2s (MSX time)
		} else {
			UNREACHABLE;
		}
	}
	return 0;
}

void Keyboard::CapsLockAligner::executeUntil(EmuTime::param time)
{
	switch (state) {
		case MUST_ALIGN_CAPSLOCK:
			alignCapsLock(time);
			break;
		case MUST_DISTRIBUTE_KEY_RELEASE: {
			auto& keyboard = OUTER(Keyboard, capsLockAligner);
			assert(keyboard.sdlReleasesCapslock);
			auto event = make_shared<KeyUpEvent>(Keys::K_CAPSLOCK);
			keyboard.msxEventDistributor.distributeEvent(event, time);
			state = IDLE;
			break;
		}
		default:
			UNREACHABLE;
	}
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
void Keyboard::CapsLockAligner::alignCapsLock(EmuTime::param time)
{
	bool hostCapsLockOn = ((SDL_GetModState() & KMOD_CAPS) != 0);
	auto& keyboard = OUTER(Keyboard, capsLockAligner);
	if (keyboard.msxCapsLockOn != hostCapsLockOn) {
		keyboard.debug("Resyncing host and MSX CAPS lock\n");
		// note: send out another event iso directly calling
		// processCapslockEvent() because we want this to be recorded
		auto event = make_shared<KeyDownEvent>(Keys::K_CAPSLOCK);
		keyboard.msxEventDistributor.distributeEvent(event, time);
		if (keyboard.sdlReleasesCapslock) {
			keyboard.debug("Sending fake CAPS release\n");
			state = MUST_DISTRIBUTE_KEY_RELEASE;
			setSyncPoint(time + EmuDuration::hz(10)); // 0.1s (MSX time)
		} else {
			state = IDLE;
		}
	} else {
		state = IDLE;
	}
}


// class KeybDebuggable

Keyboard::KeybDebuggable::KeybDebuggable(MSXMotherBoard& motherBoard_)
	: SimpleDebuggable(motherBoard_, "keymatrix", "MSX Keyboard Matrix",
	                   Keyboard::NR_KEYROWS)
{
}

byte Keyboard::KeybDebuggable::read(unsigned address)
{
	auto& keyboard = OUTER(Keyboard, keybDebuggable);
	return keyboard.getKeys()[address];
}

void Keyboard::KeybDebuggable::write(unsigned /*address*/, byte /*value*/)
{
	// ignore
}


template<typename Archive>
void Keyboard::KeyInserter::serialize(Archive& ar, unsigned /*version*/)
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
	ar.serialize("keyTypeCmd", keyTypeCmd);
	ar.serialize("cmdKeyMatrix", cmdKeyMatrix);
	ar.serialize("msxCapsLockOn", msxCapsLockOn);
	ar.serialize("msxCodeKanaLockOn", msxCodeKanaLockOn);
	ar.serialize("msxGraphLockOn", msxGraphLockOn);

	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("userKeyMatrix", userKeyMatrix);
		ar.serialize("dynKeymap", dynKeymap);
		ar.serialize("msxmodifiers", msxmodifiers);
		ar.serialize("msxKeyEventQueue", msxKeyEventQueue);
	}
	// don't serialize hostKeyMatrix

	if (ar.isLoader()) {
		// force recalculation of keyMatrix
		// (from cmdKeyMatrix and userKeyMatrix)
		keysChanged = true;
	}
}
INSTANTIATE_SERIALIZE_METHODS(Keyboard);

template<typename Archive>
void Keyboard::MsxKeyEventQueue::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);

	// serialization of deque<shared_ptr<const Event>> is not directly
	// supported by the serialization framework (main problem is the
	// constness, collections of shared_ptr to polymorhpic objects are
	// not a problem). Worked around this by serializing the events in
	// ascii format. (In all practical cases this queue will anyway be
	// empty or contain very few elements).
	//ar.serialize("eventQueue", eventQueue);
	vector<string> eventStrs;
	if (!ar.isLoader()) {
		for (auto& e : eventQueue) {
			eventStrs.push_back(e->toString());
		}
	}
	ar.serialize("eventQueue", eventStrs);
	if (ar.isLoader()) {
		assert(eventQueue.empty());
		for (auto& s : eventStrs) {
			eventQueue.push_back(
				InputEventFactory::createInputEvent(s, interp));
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Keyboard::MsxKeyEventQueue);


/** Keyboard bindings ****************************************/

// MSX Key-Matrix table
//
// row/bit  7     6     5     4     3     2     1     0
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   0   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
//   1   |  ;  |  ]  |  [  |  \  |  =  |  -  |  9  |  8  |
//   2   |  B  |  A  | Acc |  /  |  .  |  ,  |  `  |  '  |
//   3   |  J  |  I  |  H  |  G  |  F  |  E  |  D  |  C  |
//   4   |  R  |  Q  |  P  |  O  |  N  |  M  |  L  |  K  |
//   5   |  Z  |  Y  |  X  |  W  |  V  |  U  |  T  |  S  |
//   6   |  F3 |  F2 |  F1 | code| caps|graph| ctrl|shift|
//   7   | ret |selec|  bs | stop| tab | esc |  F5 |  F4 |
//   8   |right| down|  up | left| del | ins | hom |space|
//   9   |  4  |  3  |  2  |  1  |  0  |  /  |  +  |  *  |
//  10   |  .  |  ,  |  -  |  9  |  8  |  7  |  6  |  5  |
//  11   |     |     |     |     | 'NO'|     |'YES'|     |
//       +-----+-----+-----+-----+-----+-----+-----+-----+

// Mapping from SDL keys to MSX keys
static const byte x = 0xff;
const byte Keyboard::keyTab[MAX_KEYSYM] = {
// 0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
   x  , x  , x  , x  , x  , x  , x  , x  ,0x75,0x73, x  , x  , x  ,0x77, x  , x  , //000
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  ,0x72, x  , x  , x  , x  , //010
  0x80, x  , x  , x  , x  , x  , x  ,0x20, x  , x  , x  , x  ,0x22,0x12,0x23,0x24, //020
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x10,0x11, x  ,0x17, x  ,0x13, x  , x  , //030
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //040
   x  ,0x84,0x85,0x87,0x86, x  , x  , x  , x  , x  , x  ,0x15,0x14,0x16, x  , x  , //050
  0x21,0x26,0x27,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x40,0x41,0x42,0x43,0x44, //060
  0x45,0x46,0x47,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57, x  , x  , x  ,0x62,0x83, //070
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //080
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //090
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0A0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0B0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0C0
   x  , x  , x  , x  , x  , x  , x  , x  ,0x81, x  , x  , x  , x  , x  , x  , x  , //0D0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0E0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0F0
  0x93,0x94,0x95,0x96,0x97,0xA0,0xA1,0xA2,0xA3,0xA4,0xA7,0x92,0x90,0xA5,0x91,0xA6, //100
   x  ,0x85,0x86,0x87,0x84,0x82,0x81, x  , x  , x  ,0x65,0x66,0x67,0x70,0x71, x  , //110
  0x76,0x74, x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  ,0x63, x  ,0x60, //120
  0x60,0x25,0x61,0x64,0x62,0xB3,0xB1,0xB3,0xB1,0xB1,0xB3, x  , x  , x  , x  , x  , //130
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //140
};

} // namespace openmsx
