// $Id$

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include "Clock.hh"
#include "Keyboard.hh"
#include "EventDistributor.hh"
#include "SettingsConfig.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "Keys.hh"
#include "InputEvents.hh"
#include "Scheduler.hh"
#include "Unicode.hh"

using std::string;
using std::vector;

namespace openmsx {

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
		file.munmap();
	} catch (FileException &e) {
		throw FatalError("Couldn't load keymap file: " + filename);
	}
}

Keyboard::Keyboard(bool keyG)
	: keyMatrixUpCmd(*this), keyMatrixDownCmd(*this), keyTypeCmd(*this)
{
	keyGhosting = keyG;
	keysChanged = false;
	memset(keyMatrix,     255, sizeof(keyMatrix));
	memset(cmdKeyMatrix,  255, sizeof(cmdKeyMatrix));
	memset(userKeyMatrix, 255, sizeof(userKeyMatrix));

	const XMLElement* config = SettingsConfig::instance().findChild("KeyMap");
	if (config) {
		string filename = config->getData();
		loadKeymapfile(config->getFileContext().resolve(filename));
	}

	EventDistributor & distributor(EventDistributor::instance());
	distributor.registerEventListener(KEY_DOWN_EVENT,   *this);
	distributor.registerEventListener(KEY_UP_EVENT,     *this);
	distributor.registerEventListener(CONSOLE_ON_EVENT, *this);
	// We do not listen for CONSOLE_OFF_EVENTS because rescanning the
	// keyboard can have unwanted side effects

	// We may do something with focus-change events later, but SDL does
	// not send them

	CommandController::instance().registerCommand(&keyMatrixUpCmd,   "keymatrixup");
	CommandController::instance().registerCommand(&keyMatrixDownCmd, "keymatrixdown");
	CommandController::instance().registerCommand(&keyTypeCmd,       "type");
}

Keyboard::~Keyboard()
{

	CommandController::instance().unregisterCommand(&keyTypeCmd,       "type");
	CommandController::instance().unregisterCommand(&keyMatrixDownCmd, "keymatrixdown");
	CommandController::instance().unregisterCommand(&keyMatrixUpCmd,   "keymatrixup");

	EventDistributor & distributor(EventDistributor::instance());
	distributor.unregisterEventListener(KEY_UP_EVENT,   *this);
	distributor.unregisterEventListener(KEY_DOWN_EVENT, *this);
	distributor.unregisterEventListener(CONSOLE_ON_EVENT, *this);
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

void Keyboard::allUp()
{
	for (unsigned int i=0 ; i<NR_KEYROWS ; ++i) 
		userKeyMatrix[i]=0xFF;
}

bool Keyboard::signalEvent(const Event& event)
{
	switch (event.getType()) {
	case CONSOLE_ON_EVENT: 
		allUp();
		break;
	case CONSOLE_OFF_EVENT: 
		// We do not rescan the keyboard since this may lead to an
		// unwanted pressing of <return> in MSX after typing "set console
		// off" in the console.
		break;
	default: // must be keyEvent 
		assert(dynamic_cast<const KeyEvent*>(&event));
		Keys::KeyCode key = (Keys::KeyCode)((int)((KeyEvent&)event).getKeyCode() &
	                                    (int)Keys::K_MASK);
		if (key < MAX_KEYSYM) {
			switch (event.getType()) {
			case KEY_DOWN_EVENT: {
				// Key pressed: reset bit in keyMatrix
				userKeyMatrix[keyTab[key][0]] &= ~keyTab[key][1];
				break;
			}
			case KEY_UP_EVENT: {
				// Key released: set bit in keyMatrix
				userKeyMatrix[keyTab[key][0]] |= keyTab[key][1];
				break;
			}
			default:
				assert(false);
			}
		}
	}
	keysChanged = true;	// do ghosting at next getKeys()
	return true;
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
	do {	changedSomething = false;
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

void Keyboard::pressAscii(char asciiCode, bool down)
{
  	for (int i = 0; i < 2; i++) {
		byte row  = asciiTab[asciiCode & 0xFF][i] >> 8;
		byte mask = asciiTab[asciiCode & 0xFF][i] & 0xFF;
		if (down) {
			cmdKeyMatrix[row] &= ~mask;
		} else {
			cmdKeyMatrix[row] |=  mask;
		}
	}
	keysChanged = true;
}

bool Keyboard::commonKeys(char asciiCode1, char asciiCode2)
{
	// get row / mask
	byte row10  = asciiTab[asciiCode1 & 0xFF][0] >> 8;
	byte row11  = asciiTab[asciiCode1 & 0xFF][1] >> 8;
	byte row20  = asciiTab[asciiCode2 & 0xFF][0] >> 8;
	byte row21  = asciiTab[asciiCode2 & 0xFF][1] >> 8;
	byte mask10 = asciiTab[asciiCode1 & 0xFF][0] & 0xFF;
	byte mask11 = asciiTab[asciiCode1 & 0xFF][1] & 0xFF;
	byte mask20 = asciiTab[asciiCode2 & 0xFF][0] & 0xFF;
	byte mask21 = asciiTab[asciiCode2 & 0xFF][1] & 0xFF;
	
	// ignore modifier keys (shift, ctrl, graph, code)
	if (row10 == 6) mask10 &= ~0x17;
	if (row11 == 6) mask11 &= ~0x17;

	// common key on common row?
	return ((row10 == row20) && (mask10 & mask20)) ||
	       ((row10 == row21) && (mask10 & mask21)) ||
	       ((row11 == row20) && (mask11 & mask20)) ||
	       ((row11 == row21) && (mask11 & mask21));
}


// class KeyMatrixUpCmd

Keyboard::KeyMatrixUpCmd::KeyMatrixUpCmd(Keyboard& parent_)
	: parent(parent_)
{
}

string Keyboard::KeyMatrixUpCmd::execute(const vector<string>& tokens)
{
	return parent.processCmd(tokens, true);
}

string Keyboard::KeyMatrixUpCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText = 
		"keymatrixup <row> <bitmask>  release a key in the keyboardmatrix\n";
	return helpText;
}


// class KeyMatrixDownCmd

Keyboard::KeyMatrixDownCmd::KeyMatrixDownCmd(Keyboard& parent_)
	: parent(parent_)
{
}

string Keyboard::KeyMatrixDownCmd::execute(const vector<string>& tokens)
{
	return parent.processCmd(tokens, false);
}

string Keyboard::KeyMatrixDownCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText= 
		"keymatrixdown <row> <bitmask>  press a key in the keyboardmatrix\n";
	return helpText;
}


// class KeyInserter

Keyboard::KeyInserter::KeyInserter(Keyboard& parent_)
	: parent(parent_), last(-1)
{
}

Keyboard::KeyInserter::~KeyInserter()
{
}

string Keyboard::KeyInserter::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	type(tokens[1]);
	return "";
}

string Keyboard::KeyInserter::help(const vector<string>& /*tokens*/) const
{
	static const string helpText = "Type a string in the emulated MSX.";
	return helpText;
}

void Keyboard::KeyInserter::type(const string& str)
{
	if (str.empty()) {
		return;
	}
	Scheduler& scheduler = Scheduler::instance();
	if (text.empty()) {
		reschedule(scheduler.getCurrentTime());
	}
	text += Unicode::utf8ToAscii(str);
}

void Keyboard::KeyInserter::executeUntil(const EmuTime& time, int /*userData*/)
{
	parent.pressAscii(last, false);
	if (text.empty()) {
		return;
	}
	char current = text[0];
	if (parent.commonKeys(last, current)) {
		last = -1;
	} else {
		parent.pressAscii(current, true);
		last = current;
		text = text.substr(1);
	}
	reschedule(time);
}

void Keyboard::KeyInserter::reschedule(const EmuTime& time)
{
	Clock<15> nextTime(time);
	Scheduler::instance().setSyncPoint(nextTime + 1, *this);
}

const string& Keyboard::KeyInserter::schedName() const
{
	static const string schedName = "KeyInserter";
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
  {10,0x08},{10,0x10},{10,0x80},{9,0x04},{9,0x01},{10,0x20},{9,0x02},{7,0x80},
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

// keypresses needed to type ascii characters 
// for each character there are shorts S[0], S[2]
// S[i] >> 8   adresses a row in the keyboard matrix
// S[i] & 0xFF gives the keys that need to be pressed in that row
// two shorts is enough because all modifier keys on MSX are in the same row
// table is for western keyboard layout
// TODO: provide a setting to chance to other layout
short Keyboard::asciiTab[256][2] =
{
	// 0x601, 0x	SHIFT
	// 0x610, 0x	CODE
	// 0x602, 0x	CTRL
	// 0x604, 0x	GRAPH
	// 0x611, 0x	SHIFT+CODE
	// 0x603, 0x	SHIFT+CTRL
	// 0x605, 0x	SHIFT+GRAPH

	// 0x00
	{0x603, 0x004}, {0x602, 0x240}, {0x602, 0x280}, {0x602, 0x301}, // ^@, ^A, ^B, ^C,
	{0x602, 0x302}, {0x602, 0x304}, {0x602, 0x308}, {0x602, 0x310}, // ^D, ^E, ^F, ^G,
	{0x720},        {0x708},        {0x602, 0x380}, {0x802},        // BS, TAB, LF, HOME,
	{0x601, 0x802}, {0x780},        {0x602, 0x408}, {0x602, 0x410}, // CLS(FF), CR, ^N, ^O,
	// 0x10
	{0x602, 0x420}, {0x602, 0x440}, {0x804},        {0x602, 0x501}, // ^P, ^Q, INS, ^S,
	{0x602, 0x502}, {0x602, 0x504}, {0x602, 0x508}, {0x602, 0x510}, // ^T, ^U, ^V, ^W,
	{0x740},        {0x602, 0x540}, {0x602, 0x580}, {0x704},        // SEL, ^Y, EOF, ESC,
	{0x880},        {0x810},        {0x820},        {0x840},        // RGT, LFT, UP, DWN,
	// 0x20
	{0x801},        {0x601, 0x002}, {0x601, 0x201}, {0x601, 0x008}, // SPC, !, ", #, 
	{0x601, 0x010}, {0x601, 0x020}, {0x601, 0x080}, {0x201},        // $, %, &, ',
	{0x601, 0x102}, {0x601, 0x001}, {0x601, 0x101}, {0x601, 0x108}, // (, ), *, +,
	{0x204},        {0x104},        {0x208},        {0x210},        // ,, -, ., /,
	// 0x30
	{0x001},        {0x002},        {0x004},        {0x008},        // 0, 1, 2, 3,
	{0x010},        {0x020},        {0x040},        {0x080},        // 4, 5, 6, 7,
	{0x101},        {0x102},        {0x601, 0x180}, {0x180},        // 8, 9, :, ;,
	{0x601, 0x204}, {0x108},        {0x601, 0x208}, {0x601, 0x210}, // <, =, >, ?,
	// 0x40
	{0x601, 0x004}, {0x601, 0x240}, {0x601, 0x280}, {0x601, 0x301}, // @, A, B, C,
	{0x601, 0x302}, {0x601, 0x304}, {0x601, 0x308}, {0x601, 0x310}, // D, E, F, G,
	{0x601, 0x320}, {0x601, 0x340}, {0x601, 0x380}, {0x601, 0x401}, // H, I, J, K,
	{0x601, 0x402}, {0x601, 0x404}, {0x601, 0x408}, {0x601, 0x410}, // L, M, N, O,
	// 0x50
	{0x601, 0x420}, {0x601, 0x440}, {0x601, 0x480}, {0x601, 0x501}, // P, Q, R, S,
	{0x601, 0x502}, {0x601, 0x504}, {0x601, 0x508}, {0x601, 0x510}, // T, U, V, W,
	{0x601, 0x520}, {0x601, 0x540}, {0x601, 0x580}, {0x120},        // X, Y, Z, [,
	{0x110},        {0x140},        {0x601, 0x040}, {0x601, 0x104}, // \, ], ^, _,
	// 0x60
	{0x202},        {0x240},        {0x280},        {0x301},        // `, a, b, c,
	{0x302},        {0x304},        {0x308},        {0x310},        // d, e, f, g,
	{0x320},        {0x340},        {0x380},        {0x401},        // h, i, j, k,
	{0x402},        {0x404},        {0x408},        {0x410},        // l, m, n, o,
	// 0x70
	{0x420},        {0x440},        {0x480},        {0x501},        // p, q, r, s,
	{0x502},        {0x504},        {0x508},        {0x510},        // t, u, v, w,
	{0x520},        {0x540},        {0x580},        {0x601, 0x120}, // x, y, z, {,
	{0x601, 0x110}, {0x601, 0x140}, {0x601, 0x202}, {0x808},        // |, }, ~, DEL,
	// 0x80
	{0x611, 0x102}, {0x610, 0x310}, {0x610, 0x504}, {0x610, 0x440}, // 128, 129, 130, 131,
	{0x610, 0x240}, {0x610, 0x580}, {0x610, 0x204}, {0x610, 0x102}, // 132, 133, 134, 135,
	{0x610, 0x510}, {0x610, 0x501}, {0x610, 0x520}, {0x610, 0x302}, // 136, 137, 138, 139,
	{0x610, 0x304}, {0x610, 0x301}, {0x611, 0x240}, {0x611, 0x204}, // 140, 141, 142, 143,
	// 0x90
	{0x611, 0x504}, {0x610, 0x380}, {0x611, 0x380}, {0x610, 0x480}, // 144, 145, 146, 147,
	{0x610, 0x308}, {0x610, 0x508}, {0x610, 0x502}, {0x610, 0x280}, // 148, 149, 150, 151,
	{0x610, 0x020}, {0x611, 0x308}, {0x611, 0x310}, {0x610, 0x010}, // 152, 153, 154, 155,
	{0x611, 0x010}, {0x611, 0x020}, {0x611, 0x004}, {0x610, 0x002}, // 156, 157, 158, 159,
	// 0xa0
	{0x610, 0x540}, {0x610, 0x340}, {0x610, 0x410}, {0x610, 0x420}, // 160, 161, 162, 163,
	{0x610, 0x408}, {0x611, 0x408}, {0x610, 0x208}, {0x610, 0x210}, // 164, 165, 166, 167,
	{0x611, 0x210}, {0x605, 0x480}, {0x605, 0x540}, {0x604, 0x004}, // 168, 169, 170, 171,
	{0x604, 0x002}, {0x611, 0x002}, {0x605, 0x204}, {0x605, 0x208}, // 172, 173, 174, 175,
	// 0xb0
	{0x611, 0x320}, {0x610, 0x320}, {0x611, 0x401}, {0x610, 0x401}, // 176, 177, 178, 179,
	{0x611, 0x402}, {0x610, 0x402}, {0x611, 0x180}, {0x610, 0x180}, // 180, 181, 182, 183,
	{0x611, 0x201}, {0x610, 0x201}, {0x604, 0x008}, {0x604, 0x202}, // 184, 185, 186, 187,
	{0x604, 0x301}, {0x604, 0x020}, {0x611, 0x008}, {0x610, 0x008}, // 188, 189, 190, 191,
	// 0xc0
	{0x604, 0x504}, {0x605, 0x302}, {0x604, 0x410}, {0x605, 0x410}, // 192, 193, 194, 195,
	{0x604, 0x240}, {0x605, 0x504}, {0x604, 0x380}, {0x604, 0x302}, // 196, 197, 198, 199,
	{0x604, 0x402}, {0x605, 0x402}, {0x605, 0x380}, {0x605, 0x440}, // 200, 201, 202, 203,
	{0x604, 0x440}, {0x604, 0x304}, {0x605, 0x304}, {0x604, 0x510}, // 204, 205, 206, 207,
	// 0xd0
	{0x605, 0x510}, {0x605, 0x501}, {0x604, 0x501}, {0x605, 0x408}, // 208, 209, 210, 211,
	{0x605, 0x308}, {0x605, 0x508}, {0x605, 0x320}, {0x605, 0x420}, // 212, 213, 214, 215,
	{0x611, 0x001}, {0x610, 0x004}, {0x610, 0x140}, {0x604, 0x420}, // 216, 217, 218, 219,
	{0x604, 0x340}, {0x604, 0x401}, {0x605, 0x401}, {0x605, 0x340}, // 220, 221, 222, 223,
	// 0xe0
	{0x610, 0x040}, {0x610, 0x080}, {0x611, 0x101}, {0x611, 0x420}, // 224, 225, 226, 227,
	{0x611, 0x202}, {0x610, 0x202}, {0x610, 0x404}, {0x610, 0x101}, // 228, 229, 230, 231,
	{0x611, 0x120}, {0x610, 0x108}, {0x611, 0x140}, {0x610, 0x001}, // 232, 233, 234, 235,
	{0x604, 0x101}, {0x610, 0x120}, {0x610, 0x104}, {0x604, 0x010}, // 236, 237, 238, 239,
	// 0xf0
	{0x605, 0x108}, {0x604, 0x108}, {0x604, 0x208}, {0x604, 0x204}, // 240, 241, 242, 243,
	{0x604, 0x040}, {0x605, 0x040}, {0x605, 0x210}, {0x605, 0x202}, // 244, 245, 246, 247,
	{0x605, 0x580}, {0x605, 0x520}, {0x605, 0x301}, {0x604, 0x080}, // 248, 249, 250, 251,
	{0x605, 0x008}, {0x605, 0x004}, {0x605, 0x240}, {},             // 252, 253, 254, 255,
};

} // namespace openmsx

/*
  // dual char (0x01, 0x40 -> 0x01, 0x 5f)
	{},             {0x604, 0x120}, {0x605, 0x120}, {0x605, 0x201}, // 00, 01, 02, 03,
	{0x605, 0x180}, {0x604, 0x201}, {0x604, 0x180}, {0x604, 0x102}, // 04, 05, 06, 07,
	{0x605, 0x102}, {0x604, 0x001}, {0x605, 0x001}, {0x604, 0x404}, // 08, 09, 10, 11,
	{0x605, 0x404}, {0x604, 0x140}, {0x605, 0x140}, {0x604, 0x580}, // 12, 13, 14, 15,

	{0x605, 0x310}, {0x604, 0x280}, {0x604, 0x502}, {0x604, 0x320}, // 16, 17, 18, 19,
	{0x604, 0x308}, {0x604, 0x310}, {0x605, 0x110}, {0x604, 0x104}, // 20, 21, 22, 23,
	{0x604, 0x480}, {0x604, 0x540}, {0x604, 0x508}, {0x604, 0x408}, // 24, 25, 26, 27,
	{0x604, 0x520}, {0x604, 0x210}, {0x604, 0x110}, {0x605, 0x104}, // 28, 29, 30, 31,
*/
