#include "Keyboard.hh"
#include "Keys.hh"
#include "DeviceConfig.hh"
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
#include "TclArgParser.hh"
#include "checked_cast.hh"
#include "enumerate.hh"
#include "openmsx.hh"
#include "one_of.hh"
#include "outer.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "serialize_meta.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "utf8_checked.hh"
#include "view.hh"
#include "xrange.hh"
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

// How does the CAPSLOCK key behave?
#ifdef __APPLE__
// See the comments in this issue:
//    https://github.com/openMSX/openMSX/issues/1261
// Basically it means on apple:
//   when the host capslock key is pressed,       SDL sends capslock-pressed
//   when the host capslock key is released,      SDL sends nothing
//   when the host capslock key is pressed again, SDL sends capslock-released
//   when the host capslock key is released,      SDL sends nothing
static constexpr bool SANE_CAPSLOCK_BEHAVIOR = false;
#else
// We get sane capslock events from SDL:
//  when the host capslock key is pressed,  SDL sends capslock-pressed
//  when the host capslock key is released, SDL sends capslock-released
static constexpr bool SANE_CAPSLOCK_BEHAVIOR = true;
#endif


static constexpr int TRY_AGAIN = 0x80; // see pressAscii()

using KeyInfo = UnicodeKeymap::KeyInfo;

class KeyMatrixState final : public StateChange
{
public:
	KeyMatrixState() = default; // for serialize
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
	[[nodiscard]] byte getRow()     const { return row; }
	[[nodiscard]] byte getPress()   const { return press; }
	[[nodiscard]] byte getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("row",     row,
		             "press",   press,
		             "release", release);
	}
private:
	byte row, press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, KeyMatrixState, "KeyMatrixState");


constexpr const char* const defaultKeymapForMatrix[] = {
	"int", // MATRIX_MSX
	"svi", // MATRIX_SVI
	"cvjoy", // MATRIX_CVJOY
};

constexpr std::array<KeyMatrixPosition, UnicodeKeymap::KeyInfo::NUM_MODIFIERS>
		modifierPosForMatrix[] = {
	{ // MATRIX_MSX
		KeyMatrixPosition(6, 0), // SHIFT
		KeyMatrixPosition(6, 1), // CTRL
		KeyMatrixPosition(6, 2), // GRAPH
		KeyMatrixPosition(6, 3), // CAPS
		KeyMatrixPosition(6, 4), // CODE
	},
	{ // MATRIX_SVI
		KeyMatrixPosition(6, 0), // SHIFT
		KeyMatrixPosition(6, 1), // CTRL
		KeyMatrixPosition(6, 2), // LGRAPH
		KeyMatrixPosition(8, 3), // CAPS
		KeyMatrixPosition(6, 3), // RGRAPH
	},
	{ // MATRIX_CVJOY
	},
};

/** Keyboard bindings ****************************************/

// Mapping from SDL keys to emulated keys, ordered by MatrixType
constexpr KeyMatrixPosition x = KeyMatrixPosition();
static constexpr KeyMatrixPosition keyTabs[][Keyboard::MAX_KEYSYM] = {
  {
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
// 0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
   x  , x  , x  , x  , x  , x  , x  , x  ,0x75,0x73, x  , x  , x  ,0x77, x  , x  , //000
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  ,0x72, x  , x  , x  , x  , //010
  0x80, x  , x  , x  , x  , x  , x  ,0x20, x  , x  , x  , x  ,0x22,0x12,0x23,0x24, //020
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x10,0x11, x  ,0x17, x  ,0x13, x  , x  , //030
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //040
   x  ,0x84,0x85,0x87,0x86, x  , x  , x  , x  , x  , x  ,0x15,0x14,0x16, x  , x  , //050
  0x21,0x26,0x27,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x40,0x41,0x42,0x43,0x44, //060
  0x45,0x46,0x47,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57, x  , x  , x  , x  ,0x83, //070
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
  0x76,0x74, x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  ,0x60, //120
  0x60,0x25,0x61, x  , x  ,0xB3,0xB1,0xB3,0xB1,0xB1,0xB3, x  , x  , x  , x  , x  , //130
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //140
  },
  {
// SVI Keyboard Matrix
//
// row/bit  7     6     5     4     3     2     1     0
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   0   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
//   1   |  /  |  .  |  =  |  ,  |  '  |  :  |  9  |  8  |
//   2   |  G  |  F  |  E  |  D  |  C  |  B  |  A  |  -  |
//   3   |  O  |  N  |  M  |  L  |  K  |  J  |  I  |  H  |
//   4   |  W  |  V  |  U  |  T  |  S  |  R  |  Q  |  P  |
//   5   | UP  | BS  |  ]  |  \  |  [  |  Z  |  Y  |  X  |
//   6   |LEFT |ENTER|STOP | ESC |RGRAP|LGRAP|CTRL |SHIFT|
//   7   |DOWN | INS | CLS | F5  | F4  | F3  | F2  | F1  |
//   8   |RIGHT|     |PRINT| SEL |CAPS | DEL | TAB |SPACE|
//   9   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |  Numerical keypad
//  10   |  ,  |  .  |  /  |  *  |  -  |  +  |  9  |  8  |   SVI-328 only
//       +-----+-----+-----+-----+-----+-----+-----+-----+
// 0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
   x  , x  , x  , x  , x  , x  , x  , x  ,0x56,0x81, x  , x  , x  ,0x66, x  , x  , //000
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  ,0x64, x  , x  , x  , x  , //010
  0x80, x  , x  , x  , x  , x  , x  ,0x20, x  , x  , x  , x  ,0x14,0x20,0x16,0x17, //020
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x10,0x11,0x12, x  , x  ,0x15, x  , x  , //030
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //040
   x  ,0x67,0x57,0x87,0x77, x  , x  , x  , x  , x  , x  ,0x53,0x54,0x55, x  , x  , //050
   x  ,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37, //060
  0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x50,0x51,0x52, x  , x  , x  , x  ,0x82, //070
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //080
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //090
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0A0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0B0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0C0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0D0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0E0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0F0
  0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0xA0,0xA1,0xA6,0xA5,0xA4,0xA3,0xA2,0xA7, //100
   x  ,0x57,0x77,0x87,0x67,0x76, x  , x  , x  , x  ,0x70,0x71,0x72,0x73,0x74, x  , //110
  0x75,0x65, x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  ,0x60, //120
  0x60, x  ,0x61, x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //130
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //140
  },
  {
// ColecoVision Joystick "Matrix"
//
// The hardware consists of 2 controllers that each have 2 triggers
// and a 12-key keypad. They're not actually connected in a matrix,
// but a ghosting-free matrix is the easiest way to model it in openMSX.
//
// row/bit  7     6     5     4     3     2     1     0
//       +-----+-----+-----+-----+-----+-----+-----+-----+
//   0   |TRIGB|TRIGA|     |     |LEFT |DOWN |RIGHT| UP  |  controller 1
//   1   |TRIGB|TRIGA|     |     |LEFT |DOWN |RIGHT| UP  |  controller 2
//   2   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |  controller 1
//   3   |     |     |     |     |  #  |  *  |  9  |  8  |  controller 1
//   4   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |  controller 2
//   5   |     |     |     |     |  #  |  *  |  9  |  8  |  controller 2
//       +-----+-----+-----+-----+-----+-----+-----+-----+
// 0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //000
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //010
  0x06, x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  ,0x32, x  , x  , //020
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x30,0x31, x  , x  , x  ,0x33, x  , x  , //030
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //040
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //050
   x  ,0x13,0x42, x  ,0x11, x  ,0x44,0x45,0x46, x  ,0x52, x  , x  ,0x53,0x43, x  , //060
   x  , x  ,0x47,0x12,0x50,0x40,0x41,0x10, x  ,0x51, x  , x  , x  , x  , x  , x  , //070
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //080
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //090
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0A0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0B0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0C0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0D0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0E0
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0F0
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x30,0x31, x  ,0x33,0x32,0x32,0x33, x  , //100
   x  ,0x00,0x02,0x01,0x03, x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //110
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  ,0x07, //120
  0x17,0x06,0x16,0x07,0x07, x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //130
   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //140
  }
};

Keyboard::Keyboard(MSXMotherBoard& motherBoard,
                   Scheduler& scheduler_,
                   CommandController& commandController_,
                   EventDistributor& eventDistributor,
                   MSXEventDistributor& msxEventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   MatrixType matrix,
                   const DeviceConfig& config)
	: Schedulable(scheduler_)
	, commandController(commandController_)
	, msxEventDistributor(msxEventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, keyTab(keyTabs[matrix])
	, modifierPos(modifierPosForMatrix[matrix])
	, keyMatrixUpCmd  (commandController, stateChangeDistributor, scheduler_)
	, keyMatrixDownCmd(commandController, stateChangeDistributor, scheduler_)
	, keyTypeCmd      (commandController, stateChangeDistributor, scheduler_)
	, capsLockAligner(eventDistributor, scheduler_)
	, keyboardSettings(commandController)
	, msxKeyEventQueue(scheduler_, commandController.getInterpreter())
	, keybDebuggable(motherBoard)
	, unicodeKeymap(config.getChildData(
		"keyboard_type", defaultKeymapForMatrix[matrix]))
	, hasKeypad(config.getChildDataAsBool("has_keypad", true))
	, blockRow11(matrix == MATRIX_MSX
		&& !config.getChildDataAsBool("has_yesno_keys", false))
	, keyGhosting(config.getChildDataAsBool("key_ghosting", true))
	, keyGhostingSGCprotected(config.getChildDataAsBool(
		"key_ghosting_sgc_protected", true))
	, modifierIsLock(KeyInfo::CAPS_MASK
		| (config.getChildDataAsBool("code_kana_locks", false) ? KeyInfo::CODE_MASK : 0)
		| (config.getChildDataAsBool("graph_locks", false) ? KeyInfo::GRAPH_MASK : 0))
	, locksOn(0)
{
	keysChanged = false;
	msxModifiers = 0xff;
	ranges::fill(keyMatrix,     255);
	ranges::fill(cmdKeyMatrix,  255);
	ranges::fill(typeKeyMatrix, 255);
	ranges::fill(userKeyMatrix, 255);
	ranges::fill(hostKeyMatrix, 255);
	ranges::fill(dynKeymap,       0);

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

template<unsigned NUM_ROWS>
static constexpr void doKeyGhosting(byte (&matrix)[NUM_ROWS], bool protectRow6)
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
	bool changedSomething = false;
	do {
		changedSomething = false;
		for (auto i : xrange(NUM_ROWS - 1)) {
			auto row1 = matrix[i];
			for (auto j : xrange(i + 1, NUM_ROWS)) {
				auto row2 = matrix[j];
				if ((row1 != row2) && ((row1 | row2) != 0xff)) {
					auto rowIold = matrix[i];
					auto rowJold = matrix[j];
					// TODO: The shift/graph/code key ghosting protection
					//       implementation is only correct for MSX.
					if (protectRow6 && i == 6) {
						matrix[i] = row1 & row2;
						matrix[j] = (row1 | 0x15) & row2;
						row1 &= row2;
					} else if (protectRow6 && j == 6) {
						matrix[i] = row1 & (row2 | 0x15);
						matrix[j] = row1 & row2;
						row1 &= (row2 | 0x15);
					} else {
						// not same and some common zero's
						//  --> inherit other zero's
						auto newRow = row1 & row2;
						matrix[i] = newRow;
						matrix[j] = newRow;
						row1 = newRow;
					}
					if (rowIold != matrix[i] ||
					    rowJold != matrix[j]) {
						changedSomething = true;
					}
				}
			}
		}
	} while (changedSomething);
}

const byte* Keyboard::getKeys() const
{
	if (keysChanged) {
		keysChanged = false;
		const auto* matrix = keyTypeCmd.isActive() ? typeKeyMatrix : userKeyMatrix;
		for (auto row : xrange(KeyMatrixPosition::NUM_ROWS)) {
			keyMatrix[row] = cmdKeyMatrix[row] & matrix[row];
		}
		if (keyGhosting) {
			doKeyGhosting(keyMatrix, keyGhostingSGCprotected);
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
	// transfer that to the new keyboard(s) that get created for reverse.
	// When replay is stopped we restore this host keyboard state, see
	// stopReplay().

	for (auto row : xrange(KeyMatrixPosition::NUM_ROWS)) {
		hostKeyMatrix[row] = source.hostKeyMatrix[row];
	}
}

/* Received an MSX event
 * Following events get processed:
 *  OPENMSX_KEY_DOWN_EVENT
 *  OPENMSX_KEY_UP_EVENT
 */
void Keyboard::signalMSXEvent(const shared_ptr<const Event>& event,
                              EmuTime::param time)
{
	if (event->getType() == one_of(OPENMSX_KEY_DOWN_EVENT, OPENMSX_KEY_UP_EVENT)) {
		// Ignore possible console on/off events:
		// we do not rescan the keyboard since this may lead to
		// an unwanted pressing of <return> in MSX after typing
		// "set console off" in the console.
		msxKeyEventQueue.process_asap(time, event);
	}
}

void Keyboard::signalStateChange(const shared_ptr<StateChange>& event)
{
	const auto* kms = dynamic_cast<const KeyMatrixState*>(event.get());
	if (!kms) return;

	userKeyMatrix[kms->getRow()] &= ~kms->getPress();
	userKeyMatrix[kms->getRow()] |=  kms->getRelease();
	keysChanged = true; // do ghosting at next getKeys()
}

void Keyboard::stopReplay(EmuTime::param time)
{
	for (auto [row, hkm] : enumerate(hostKeyMatrix)) {
		changeKeyMatrixEvent(time, byte(row), hkm);
	}
	msxModifiers = 0xff;
	msxKeyEventQueue.clear();
	memset(dynKeymap, 0, sizeof(dynKeymap));
}

byte Keyboard::needsLockToggle(const UnicodeKeymap::KeyInfo& keyInfo) const
{
	return modifierIsLock
	     & (locksOn ^ keyInfo.modmask)
	     & unicodeKeymap.getRelevantMods(keyInfo);
}

void Keyboard::pressKeyMatrixEvent(EmuTime::param time, KeyMatrixPosition pos)
{
	if (!pos.isValid()) {
		// No such key.
		return;
	}
	auto row = pos.getRow();
	auto press = pos.getMask();
	if (((hostKeyMatrix[row] & press) == 0) &&
	    ((userKeyMatrix[row] & press) == 0)) {
		// Won't have any effect, ignore.
		return;
	}
	changeKeyMatrixEvent(time, row, hostKeyMatrix[row] & ~press);
}

void Keyboard::releaseKeyMatrixEvent(EmuTime::param time, KeyMatrixPosition pos)
{
	if (!pos.isValid()) {
		// No such key.
		return;
	}
	auto row = pos.getRow();
	auto release = pos.getMask();
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

/*
 * @return True iff a release event for the CODE/KANA key must be scheduled.
 */
bool Keyboard::processQueuedEvent(const Event& event, EmuTime::param time)
{
	auto mode = keyboardSettings.getMappingMode();

	const auto& keyEvent = checked_cast<const KeyEvent&>(event);
	bool down = event.getType() == OPENMSX_KEY_DOWN_EVENT;
	auto code = (mode == KeyboardSettings::POSITIONAL_MAPPING)
	          ? keyEvent.getScanCode() : keyEvent.getKeyCode();
	auto key = static_cast<Keys::KeyCode>(int(code) & int(Keys::K_MASK));

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
		ad_printf("Key released, keyCode: 0x%05x, keyName: %s\n",
		      keyEvent.getKeyCode(),
		      Keys::getName(keyEvent.getKeyCode()).c_str());
		debug("Key released, keyCode: 0x%05x, keyName: %s\n",
		      keyEvent.getKeyCode(),
		      Keys::getName(keyEvent.getKeyCode()).c_str());
	}

	// Process deadkeys.
	if (mode == KeyboardSettings::CHARACTER_MAPPING) {
		for (auto n : xrange(3)) {
			if (key == keyboardSettings.getDeadkeyHostKey(n)) {
				UnicodeKeymap::KeyInfo deadkey = unicodeKeymap.getDeadkey(n);
				if (deadkey.isValid()) {
					updateKeyMatrix(time, down, deadkey.pos);
					return false;
				}
			}
		}
	}

	if (key == Keys::K_CAPSLOCK) {
		processCapslockEvent(time, down);
		return false;
	} else if (key == keyboardSettings.getCodeKanaHostKey()) {
		processCodeKanaChange(time, down);
		return false;
	} else if (key == Keys::K_LALT) {
		processGraphChange(time, down);
		return false;
	} else if (key == Keys::K_KP_ENTER) {
		processKeypadEnterKey(time, down);
		return false;
	} else {
		return processKeyEvent(time, down, keyEvent);
	}
}

/*
 * Process a change (up or down event) of the CODE/KANA key
 * It presses or releases the key in the MSX keyboard matrix
 * and changes the kanalock state in case of a press
 */
void Keyboard::processCodeKanaChange(EmuTime::param time, bool down)
{
	if (down) {
		locksOn ^= KeyInfo::CODE_MASK;
	}
	updateKeyMatrix(time, down, modifierPos[KeyInfo::CODE]);
}

/*
 * Process a change (up or down event) of the GRAPH key
 * It presses or releases the key in the MSX keyboard matrix
 * and changes the graphlock state in case of a press
 */
void Keyboard::processGraphChange(EmuTime::param time, bool down)
{
	if (down) {
		locksOn ^= KeyInfo::GRAPH_MASK;
	}
	updateKeyMatrix(time, down, modifierPos[KeyInfo::GRAPH]);
}

/*
 * Process a change (up or down event) of the CAPSLOCK key
 * It presses or releases the key in the MSX keyboard matrix
 * and changes the capslock state in case of a press
 */
void Keyboard::processCapslockEvent(EmuTime::param time, bool down)
{
	if (SANE_CAPSLOCK_BEHAVIOR) {
		debug("Changing CAPS lock state according to SDL request\n");
		if (down) {
			locksOn ^= KeyInfo::CAPS_MASK;
		}
		updateKeyMatrix(time, down, modifierPos[KeyInfo::CAPS]);
	} else {
		debug("Pressing CAPS lock and scheduling a release\n");
		locksOn ^= KeyInfo::CAPS_MASK;
		updateKeyMatrix(time, true, modifierPos[KeyInfo::CAPS]);
		setSyncPoint(time + EmuDuration::hz(10)); // 0.1s (in MSX time)
	}
}

void Keyboard::executeUntil(EmuTime::param time)
{
	debug("Releasing CAPS lock\n");
	updateKeyMatrix(time, false, modifierPos[KeyInfo::CAPS]);
}

void Keyboard::processKeypadEnterKey(EmuTime::param time, bool down)
{
	if (!hasKeypad && !keyboardSettings.getAlwaysEnableKeypad()) {
		// User entered on host keypad but this MSX model does not have one
		// Ignore the keypress/release
		return;
	}
	processSdlKey(time, down,
		keyboardSettings.getKpEnterMode() == KeyboardSettings::MSX_KP_COMMA
		? Keys::K_KP_ENTER : Keys::K_RETURN);
}

/*
 * Process an SDL key press/release event. It concerns a
 * special key (e.g. SHIFT, UP, DOWN, F1, F2, ...) that can not
 * be unambiguously derived from a unicode character;
 *  Map the SDL key to an equivalent MSX key press/release event
 */
void Keyboard::processSdlKey(EmuTime::param time, bool down, Keys::KeyCode key)
{
	if (key < MAX_KEYSYM) {
		auto pos = keyTab[key];
		if (pos.isValid()) {
			if (pos.getRow() == 11 && blockRow11) {
				// do not process row 11 if we have no Yes/No keys
				return;
			}
			updateKeyMatrix(time, down, pos);
		}
	}
}

/*
 * Update the MSX keyboard matrix
 */
void Keyboard::updateKeyMatrix(EmuTime::param time, bool down, KeyMatrixPosition pos)
{
	if (!pos.isValid()) {
		// No such key.
		return;
	}
	if (down) {
		pressKeyMatrixEvent(time, pos);
		// Keep track of the MSX modifiers.
		// The MSX modifiers sometimes get overruled by the unicode character
		// processing, in which case the unicode processing must be able to
		// restore them to the real key-combinations pressed by the user.
		for (auto [i, mp] : enumerate(modifierPos)) {
			if (pos == mp) {
				msxModifiers &= ~(1 << i);
			}
		}
	} else {
		releaseKeyMatrixEvent(time, pos);
		for (auto [i, mp] : enumerate(modifierPos)) {
			if (pos == mp) {
				msxModifiers |= 1 << i;
			}
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
 * @return True iff a release event for the CODE/KANA key must be scheduled.
 */
bool Keyboard::processKeyEvent(EmuTime::param time, bool down, const KeyEvent& keyEvent)
{
	auto mode = keyboardSettings.getMappingMode();

	auto keyCode  = keyEvent.getKeyCode();
	auto scanCode = keyEvent.getScanCode();
	auto code = (mode == KeyboardSettings::POSITIONAL_MAPPING) ? scanCode : keyCode;
	auto key = static_cast<Keys::KeyCode>(int(code) & int(Keys::K_MASK));

	bool isOnKeypad =
		(key >= Keys::K_KP0 && key <= Keys::K_KP9) ||
		(key == one_of(Keys::K_KP_PERIOD, Keys::K_KP_DIVIDE, Keys::K_KP_MULTIPLY,
		               Keys::K_KP_MINUS,  Keys::K_KP_PLUS));

	if (isOnKeypad && !hasKeypad &&
	    !keyboardSettings.getAlwaysEnableKeypad()) {
		// User entered on host keypad but this MSX model does not have one
		// Ignore the keypress/release
		return false;
	}

	if (down) {
		UnicodeKeymap::KeyInfo keyInfo;
		unsigned unicode;
		if (isOnKeypad ||
		    mode == one_of(KeyboardSettings::KEY_MAPPING,
		                   KeyboardSettings::POSITIONAL_MAPPING)) {
			// User entered a key on numeric keypad or the driver is in
			// KEY/POSITIONAL mapping mode.
			// First option (keypad) maps to same unicode as some other key
			// combinations (e.g. digit on main keyboard or TAB/DEL).
			// Use unicode to handle the more common combination and use direct
			// matrix to matrix mapping for the exceptional cases (keypad).
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
			} else if (utf8::is_pua(unicode)) {
				// Code point in Private Use Area: undefined by Unicode,
				// so we rely on SDL's interpretation instead.
				// For example the Mac's cursor keys are in this range.
				unicode = 0;
			} else {
				keyInfo = unicodeKeymap.get(unicode);
				if (!keyInfo.isValid()) {
					// Unicode does not exist in our mapping; try to process
					// the key using its keycode instead.
					unicode = 0;
				}
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
			return false;
		} else {
			// It is a unicode character; map it to the right key-combination
			return pressUnicodeByUser(time, keyInfo, unicode, true);
		}
	} else {
		// key was released
#if defined(__APPLE__)
		if ((keyCode & (Keys::K_MASK | Keys::KM_META))
				== (Keys::K_I | Keys::KM_META)) {
			keyCode = key = Keys::K_INSERT;
		}
#endif
		unsigned unicode = (key < MAX_KEYSYM)
			? dynKeymap[key] // Get the unicode that was derived from this key
			: 0;
		if (unicode == 0) {
			// It was a special key, perform matrix to matrix mapping
			// But only when it is not a first keystroke of a
			// composed key
			if ((keyCode & Keys::KM_MODE) == 0) {
				processSdlKey(time, down, key);
			}
		} else {
			// It was a unicode character; map it to the right key-combination
			pressUnicodeByUser(time, unicodeKeymap.get(unicode), unicode, false);
		}
		return false;
	}
}

void Keyboard::processCmd(Interpreter& interp, span<const TclObject> tokens, bool up)
{
	unsigned row  = tokens[1].getInt(interp);
	unsigned mask = tokens[2].getInt(interp);
	if (row >= KeyMatrixPosition::NUM_ROWS) {
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
bool Keyboard::pressUnicodeByUser(
		EmuTime::param time, UnicodeKeymap::KeyInfo keyInfo, unsigned unicode,
		bool down)
{
	bool insertCodeKanaRelease = false;
	if (down) {
		if ((needsLockToggle(keyInfo) & KeyInfo::CODE_MASK) &&
				keyboardSettings.getAutoToggleCodeKanaLock()) {
			// Code Kana locks, is in wrong state and must be auto-toggled:
			// Toggle it by pressing the lock key and scheduling a
			// release event
			locksOn ^= KeyInfo::CODE_MASK;
			pressKeyMatrixEvent(time, modifierPos[KeyInfo::CODE]);
			insertCodeKanaRelease = true;
		} else {
			// Press the character key and related modifiers
			// Ignore the CODE key in case that Code Kana locks
			// (e.g. do not press it).
			// Ignore the GRAPH key in case that Graph locks
			// Always ignore CAPSLOCK mask (assume that user will
			// use real CAPS lock to switch/ between hiragana and
			// katakana on japanese model)
			pressKeyMatrixEvent(time, keyInfo.pos);

			byte modmask = keyInfo.modmask & ~modifierIsLock;
			if (('A' <= unicode && unicode <= 'Z') || ('a' <= unicode && unicode <= 'z')) {
				// For a-z and A-Z, leave SHIFT unchanged, this to cater
				// for difference in behaviour between host and emulated
				// machine with respect to how the combination of CAPSLOCK
				// and SHIFT is interpreted for these characters.
				modmask &= ~KeyInfo::SHIFT_MASK;
			} else {
				// Release SHIFT if our character does not require it.
				if (~modmask & KeyInfo::SHIFT_MASK) {
					releaseKeyMatrixEvent(time, modifierPos[KeyInfo::SHIFT]);
				}
			}
			// Press required modifiers for our character.
			// Note that these modifiers are only pressed, never released.
			for (auto [i, mp] : enumerate(modifierPos)) {
				if ((modmask >> i) & 1) {
					pressKeyMatrixEvent(time, mp);
				}
			}
		}
	} else {
		releaseKeyMatrixEvent(time, keyInfo.pos);

		// Restore non-lock modifier keys.
		for (auto [i, mp] : enumerate(modifierPos)) {
			if (!((modifierIsLock >> i) & 1)) {
				// Do not simply unpress graph, ctrl, code and shift but
				// restore them to the values currently pressed by the user.
				if ((msxModifiers >> i) & 1) {
					releaseKeyMatrixEvent(time, mp);
				} else {
					pressKeyMatrixEvent(time, mp);
				}
			}
		}
	}
	keysChanged = true;
	return insertCodeKanaRelease;
}

/*
 * Press an ASCII character. It is used by the 'Insert characters'
 * function that is exposed to the console.
 * The characters are inserted in a separate keyboard matrix, to prevent
 * interference with the keypresses of the user on the MSX itself.
 *
 * @returns:
 *   zero  : handling this character is done
 * non-zero: typing this character needs additional actions
 *    bits 0-4: release these modifiers and call again
 *    bit   7 : simply call again (used by SHIFT+GRAPH heuristic)
 */
int Keyboard::pressAscii(unsigned unicode, bool down)
{
	int releaseMask = 0;
	UnicodeKeymap::KeyInfo keyInfo = unicodeKeymap.get(unicode);
	if (!keyInfo.isValid()) {
		return releaseMask;
	}
	byte modmask = keyInfo.modmask & ~modifierIsLock;
	if (down) {
		// check for modifier toggles
		byte toggleLocks = needsLockToggle(keyInfo);
		for (auto [i, mp] : enumerate(modifierPos)) {
			if ((toggleLocks >> i) & 1) {
				debug("Toggling lock %d\n", i);
				locksOn ^= 1 << i;
				releaseMask |= 1 << i;
				typeKeyMatrix[mp.getRow()] &= ~mp.getMask();
			}
		}
		if (releaseMask == 0) {
			debug("Key pasted, unicode: 0x%04x, row: %02d, col: %d, modmask: %02x\n",
			      unicode, keyInfo.pos.getRow(), keyInfo.pos.getColumn(), modmask);
			// Workaround MSX-BIOS(?) bug/limitation:
			//
			// Under these conditions:
			// - Typing a graphical MSX character 00-1F (such a char 'x' gets
			//   printed as chr$(1) followed by chr$(x+64)).
			// - Typing this character requires pressing SHIFT+GRAPH and one
			//   'regular' key.
			// - And all 3 keys are immediately pressed simultaneously.
			// Then, from time to time, instead of the intended character 'x'
			// (00-1F), the wrong character 'x+64' gets printed.
			// When first SHIFT+GRAPH is pressed, and only one frame later the
			// other keys is pressed (additionally), this problem seems to
			// disappear.
			//
			// After implementing the above we found that a similar problem
			// occurs when:
			// - a GRAPH + <x> (without SHIFT) key combo is typed
			// - immediately after a key combo with GRAPH + SHIFT + <x>.
			// For example:
			//   type "\u2666\u266a"
			// from time to time 2nd character got wrongly typed as a
			// 'M' (instead of a musical note symbol). But typing a sequence
			// of \u266a chars without a preceding \u2666 just works.
			//
			// To fix both these problems (and possibly still undiscovered
			// variations), I'm now extending the workaround to all characters
			// that are typed via a key combination that includes GRAPH.
			if (modmask & KeyInfo::GRAPH_MASK) {
				auto isPressed = [&](auto& key) {
					return (typeKeyMatrix[key.getRow()] & key.getMask()) == 0;
				};
				if (!isPressed(modifierPos[KeyInfo::GRAPH])) {
					// GRAPH not yet pressed ->
					//  first press it before adding the non-modifier key
					releaseMask = TRY_AGAIN;
				}
			}
			// press modifiers
			for (auto [i, mp] : enumerate(modifierPos)) {
				if ((modmask >> i) & 1) {
					typeKeyMatrix[mp.getRow()] &= ~mp.getMask();
				}
			}
			if (releaseMask == 0) {
				// press key
				typeKeyMatrix[keyInfo.pos.getRow()] &= ~keyInfo.pos.getMask();
			}
		}
	} else {
		typeKeyMatrix[keyInfo.pos.getRow()] |= keyInfo.pos.getMask();
		for (auto [i, mp] : enumerate(modifierPos)) {
			if ((modmask >> i) & 1) {
				typeKeyMatrix[mp.getRow()] |= mp.getMask();
			}
		}
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
void Keyboard::pressLockKeys(byte lockKeysMask, bool down)
{
	for (auto [i, mp] : enumerate(modifierPos)) {
		if ((lockKeysMask >> i) & 1) {
			if (down) {
				// press lock key
				typeKeyMatrix[mp.getRow()] &= ~mp.getMask();
			} else {
				// release lock key
				typeKeyMatrix[mp.getRow()] |= mp.getMask();
			}
		}
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
	auto keyPos1 = unicodeKeymap.get(unicode1).pos;
	auto keyPos2 = unicodeKeymap.get(unicode2).pos;

	return keyPos1 == keyPos2 && keyPos1.isValid();
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
	span<const TclObject> tokens, TclObject& /*result*/, EmuTime::param /*time*/)
{
	checkNumArgs(tokens, 3, Prefix{1}, "row mask");
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

void Keyboard::KeyMatrixDownCmd::execute(span<const TclObject> tokens,
                                         TclObject& /*result*/, EmuTime::param /*time*/)
{
	checkNumArgs(tokens, 3, Prefix{1}, "row mask");
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
	oldLocksOn = 0;
	releaseBeforePress = false;
	typingFrequency = 15;
}

void Keyboard::KeyInserter::execute(
	span<const TclObject> tokens, TclObject& /*result*/, EmuTime::param /*time*/)
{
	checkNumArgs(tokens, AtLeast{2}, "?-release? ?-freq hz? text");

	releaseBeforePress = false;
	typingFrequency = 15;

	// for full backwards compatibility: one option means type it...
	if (tokens.size() == 2) {
		type(tokens[1].getString());
		return;
	}

	ArgsInfo info[] = {
		flagArg("-release", releaseBeforePress),
		valueArg("-freq", typingFrequency),
	};
	auto arguments = parseTclArgs(getInterpreter(), tokens.subspan(1), info);

	if (typingFrequency <= 0) {
		throw CommandException("Wrong argument for -freq (should be a positive number)");
	}
	if (arguments.size() != 1) throw SyntaxError();

	type(arguments[0].getString());
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

void Keyboard::KeyInserter::type(std::string_view str)
{
	if (str.empty()) {
		return;
	}
	auto& keyboard = OUTER(Keyboard, keyTypeCmd);
	oldLocksOn = keyboard.locksOn;
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
		keyboard.debug("Restoring locks: %02X -> %02X\n", keyboard.locksOn, oldLocksOn);
		auto diff = oldLocksOn ^ keyboard.locksOn;
		lockKeysMask = diff;
		if (diff != 0) {
			// press CAPS, GRAPH and/or Code/Kana Lock keys
			keyboard.locksOn ^= diff;
			keyboard.pressLockKeys(diff, true);
			reschedule(time);
		}
		return;
	}

	try {
		auto it = begin(text_utf8);
		unsigned current = utf8::next(it, end(text_utf8));
		if (releaseLast && (releaseBeforePress || keyboard.commonKeys(last, current))) {
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
			} else if (lockKeysMask & TRY_AGAIN) {
				lockKeysMask &= ~TRY_AGAIN;
				releaseLast = false;
			} else if (releaseBeforePress) {
				releaseLast = true;
			}
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
	if (!SANE_CAPSLOCK_BEHAVIOR) {
		// don't even try
		return 0;
	}

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
	if (bool(keyboard.locksOn & KeyInfo::CAPS_MASK) != hostCapsLockOn) {
		keyboard.debug("Resyncing host and MSX CAPS lock\n");
		// note: send out another event iso directly calling
		// processCapslockEvent() because we want this to be recorded
		auto event = make_shared<KeyDownEvent>(Keys::K_CAPSLOCK);
		keyboard.msxEventDistributor.distributeEvent(event, time);
		keyboard.debug("Sending fake CAPS release\n");
		state = MUST_DISTRIBUTE_KEY_RELEASE;
		setSyncPoint(time + EmuDuration::hz(10)); // 0.1s (MSX time)
	} else {
		state = IDLE;
	}
}


// class KeybDebuggable

Keyboard::KeybDebuggable::KeybDebuggable(MSXMotherBoard& motherBoard_)
	: SimpleDebuggable(motherBoard_, "keymatrix", "MSX Keyboard Matrix",
	                   KeyMatrixPosition::NUM_ROWS)
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
	ar.serialize("text", text_utf8,
	             "last", last,
	             "lockKeysMask", lockKeysMask,
	             "releaseLast", releaseLast);

	bool oldCodeKanaLockOn, oldGraphLockOn, oldCapsLockOn;
	if (!ar.isLoader()) {
		oldCodeKanaLockOn = oldLocksOn & KeyInfo::CODE_MASK;
		oldGraphLockOn = oldLocksOn & KeyInfo::GRAPH_MASK;
		oldCapsLockOn = oldLocksOn & KeyInfo::CAPS_MASK;
	}
	ar.serialize("oldCodeKanaLockOn", oldCodeKanaLockOn,
	             "oldGraphLockOn",    oldGraphLockOn,
	             "oldCapsLockOn",     oldCapsLockOn);
	if (ar.isLoader()) {
		oldLocksOn = (oldCodeKanaLockOn ? KeyInfo::CODE_MASK : 0)
		           | (oldGraphLockOn ? KeyInfo::GRAPH_MASK : 0)
		           | (oldCapsLockOn ? KeyInfo::CAPS_MASK : 0);
	}
}

// version 1: Initial version: {userKeyMatrix, dynKeymap, msxModifiers,
//            msxKeyEventQueue} was intentionally not serialized. The reason
//            was that after a loadstate, you want the MSX keyboard to reflect
//            the state of the host keyboard. So any pressed MSX keys from the
//            time the savestate was created are cleared.
// version 2: For reverse-replay it is important that snapshots contain the
//            full state of the MSX keyboard, so now we do serialize it.
// version 3: split cmdKeyMatrix into cmdKeyMatrix + typeKeyMatrix
// TODO Is the assumption in version 1 correct (clear keyb state on load)?
//      If it is still useful for 'regular' loadstate, then we could implement
//      it by explicitly clearing the keyb state from the actual loadstate
//      command. (But let's only do this when experience shows it's really
//      better).
template<typename Archive>
void Keyboard::serialize(Archive& ar, unsigned version)
{
	ar.serialize("keyTypeCmd", keyTypeCmd,
	             "cmdKeyMatrix", cmdKeyMatrix);
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("typeKeyMatrix", typeKeyMatrix);
	} else {
		ranges::copy(cmdKeyMatrix, typeKeyMatrix);
	}

	bool msxCapsLockOn, msxCodeKanaLockOn, msxGraphLockOn;
	if (!ar.isLoader()) {
		msxCapsLockOn = locksOn & KeyInfo::CAPS_MASK;
		msxCodeKanaLockOn = locksOn & KeyInfo::CODE_MASK;
		msxGraphLockOn = locksOn & KeyInfo::GRAPH_MASK;
	}
	ar.serialize("msxCapsLockOn",     msxCapsLockOn,
	             "msxCodeKanaLockOn", msxCodeKanaLockOn,
	             "msxGraphLockOn",    msxGraphLockOn);
	if (ar.isLoader()) {
		locksOn = (msxCapsLockOn ? KeyInfo::CAPS_MASK : 0)
		        | (msxCodeKanaLockOn ? KeyInfo::CODE_MASK : 0)
		        | (msxGraphLockOn ? KeyInfo::GRAPH_MASK : 0);
	}

	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("userKeyMatrix",    userKeyMatrix,
		             "dynKeymap",        dynKeymap,
		             "msxmodifiers",     msxModifiers,
		             "msxKeyEventQueue", msxKeyEventQueue);
	}
	// don't serialize hostKeyMatrix

	if (ar.isLoader()) {
		// force recalculation of keyMatrix
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
	// constness, collections of shared_ptr to polymorphic objects are
	// not a problem). Worked around this by serializing the events in
	// ascii format. (In all practical cases this queue will anyway be
	// empty or contain very few elements).
	//ar.serialize("eventQueue", eventQueue);
	vector<string> eventStrs;
	if (!ar.isLoader()) {
		eventStrs = to_vector(view::transform(
			eventQueue, [](auto& e) { return e->toString(); }));
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

} // namespace openmsx
