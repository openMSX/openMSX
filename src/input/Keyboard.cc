// $Id$

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include "Keyboard.hh"
#include "EventDistributor.hh"
#include "SettingsConfig.hh"
#include "Config.hh"
#include "File.hh"
#include "FileContext.hh"
#include "CommandController.hh"
#include "Keys.hh"
#include "InputEvents.hh"
#include "Scheduler.hh"

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
	try {
		Config* config = SettingsConfig::instance().getConfigById("KeyMap");
		string filename = config->getParameter("filename");
		filename = config->getContext().resolve(filename);
		loadKeymapfile(filename);
	} catch (ConfigException &e) {
		// no keymap settings.
	}
	EventDistributor::instance().registerEventListener(KEY_DOWN_EVENT, *this);
	EventDistributor::instance().registerEventListener(KEY_UP_EVENT,   *this);

	CommandController::instance().registerCommand(&keyMatrixUpCmd,   "keymatrixup");
	CommandController::instance().registerCommand(&keyMatrixDownCmd, "keymatrixdown");
	CommandController::instance().registerCommand(&keyTypeCmd,       "type");
}

Keyboard::~Keyboard()
{
	CommandController::instance().unregisterCommand(&keyTypeCmd,       "type");
	CommandController::instance().unregisterCommand(&keyMatrixDownCmd, "keymatrixdown");
	CommandController::instance().unregisterCommand(&keyMatrixUpCmd,   "keymatrixup");

	EventDistributor::instance().unregisterEventListener(KEY_UP_EVENT,   *this);
	EventDistributor::instance().unregisterEventListener(KEY_DOWN_EVENT, *this);
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


bool Keyboard::signalEvent(const Event& event) throw()
{
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

void Keyboard::pressAscii(char asciiCode, bool up)
{
	for (int i = 0; i < 2; i++) {
		byte row  = asciiTab[(unsigned)asciiCode][i] >> 8;
		byte mask = asciiTab[(unsigned)asciiCode][i] & 0xFF;
		if (up) {
			cmdKeyMatrix[row] |=  mask;
		} else {
			cmdKeyMatrix[row] &= ~mask;
		}
	}
	keysChanged = true;
}


// class KeyMatrixUpCmd

Keyboard::KeyMatrixUpCmd::KeyMatrixUpCmd(Keyboard& parent_)
	: parent(parent_)
{
}

string Keyboard::KeyMatrixUpCmd::execute(const vector<string> &tokens)
	throw(CommandException)
{
	return parent.processCmd(tokens, true);
}

string Keyboard::KeyMatrixUpCmd::help(const vector<string> &tokens) const
	throw()
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

string Keyboard::KeyMatrixDownCmd::execute(const vector<string> &tokens)
	throw(CommandException)
{
	return parent.processCmd(tokens, false);
}

string Keyboard::KeyMatrixDownCmd::help(const vector<string> &tokens) const
	throw()
{
	static const string helpText= 
		"keymatrixdown <row> <bitmask>  press a key in the keyboardmatrix\n";
	return helpText;
}


// class KeyInserter

Keyboard::KeyInserter::KeyInserter(Keyboard& parent_)
	: parent(parent_), down(true)
{
}

Keyboard::KeyInserter::~KeyInserter()
{
}

string Keyboard::KeyInserter::execute(const vector<string>& tokens)
	throw (CommandException)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	type(tokens[1]);
	return "";
}

string Keyboard::KeyInserter::help(const vector<string>& tokens) const
	throw(CommandException)
{
	// TODO
	static const string helpText = "TODO";
	return helpText;
}

void Keyboard::KeyInserter::type(const string& str)
{
	Scheduler& scheduler = Scheduler::instance();
	if (text.empty()) {
		scheduler.setSyncPoint(scheduler.getCurrentTime(), this);
	}
	text += str;
}

void Keyboard::KeyInserter::executeUntil(const EmuTime& time, int userData)
	throw()
{
	assert(!text.empty());
	if (down) {
		parent.pressAscii(text[0], false);
	} else {
		parent.pressAscii(text[0], true);
		text = text.substr(1);
	}
	down = !down;
	EmuTimeFreq<15> nextTime(time);
	if (!text.empty()) {
		Scheduler::instance().setSyncPoint(nextTime + 1, this);
	}
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

short Keyboard::asciiTab[256][2] =
{
	// TODO fill in missing keys, fix wrong ones
	// 0x00
	{},      {},      {}, {}, {},      {}, {}, {},
	{0x720}, {0x708}, {}, {}, {}, {0x780}, {}, {},
	// 0x10
	{}, {}, {}, {}, {},      {}, {}, {},
	{}, {}, {}, {}, {0x704}, {}, {}, {},
	// 0x20
	{0x801},          {0x601, /*K_1*/}, {0x601, /*K_QUOTE*/}, {0x601, /*K_3*/},
	{0x601, /*K_4*/}, {0x601, /*K_5*/}, {0x601, /*K_7*/},     {/*K_QUOTE*/},
	{0x601, /*K_9*/}, {0x601, /*K_0*/}, {0x601, /*K_8*/},     {0x601, /*K_EQUALS*/},
	{0x204},          {0x104},          {0x108},              {0x210},
	// 0x30
	{0x001}, {0x002}, {0x004}, {0x008}, {0x010}, {0x020}, {0x040}, {0x080},
	{0x101}, {0x102}, {0x201}, {/*K_SEMICOLON*/},
	{0x601, /*K_COMMA*/}, {/*K_EQUALS*/}, {0x601, /*K_PERIOD*/}, {0x601, /*K_SLASH*/},
	// 0x40
	{0x120},        {0x601, 0x240}, {0x601, 0x280}, {0x601, 0x301},
	{0x601, 0x302}, {0x601, 0x304}, {0x601, 0x308}, {0x601, 0x310},
	{0x601, 0x320}, {0x601, 0x340}, {0x601, 0x380}, {0x601, 0x401},
	{0x601, 0x402}, {0x601, 0x404}, {0x601, 0x408}, {0x601, 0x410},
	// 0x050
	{0x601, 0x420}, {0x601, 0x440}, {0x601, 0x480}, {0x601, 0x501},
	{0x601, 0x502}, {0x601, 0x504}, {0x601, 0x508}, {0x601, 0x510},
	{0x601, 0x520}, {0x601, 0x540}, {0x601, 0x580}, {0x140},
	{0x110},        {0x202},        {0x108},        {0x104},
	// 0x60
	{/*K_BACKQUOTE*/}, {0x240}, {0x280}, {0x301},
	{0x302},           {0x304}, {0x308}, {0x310},
	{0x320},           {0x340}, {0x380}, {0x401},
	{0x302},           {0x404}, {0x408}, {0x410},
	// 0x70
	{0x420}, {0x440}, {0x480}, {0x501},
	{0x502}, {0x504}, {0x508}, {0x510},
	{0x520}, {0x540}, {0x580}, {0x601, /*K_LEFTBRACKET*/},
	{0x601, /*K_BACKSLASH*/}, {0x601, /*K_RIGHTBRACKET*/},
	{0x601, /*K_BACKQUOTE*/}, {0x808},
	// 0x80 - 0xff
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
};

} // namespace openmsx
