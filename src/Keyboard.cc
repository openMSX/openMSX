// $Id$

#include "Keyboard.hh"
#include "EventDistributor.hh"


Keyboard::Keyboard(bool keyG)
{
	keyGhosting = keyG;
	keysChanged = false;
	memset(keyMatrix , 255, sizeof(keyMatrix) );
	memset(keyMatrix2, 255, sizeof(keyMatrix2));
	EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this, 1);
	EventDistributor::instance()->registerEventListener(SDL_KEYUP,   this, 1);
}

Keyboard::~Keyboard()
{
	EventDistributor::instance()->unregisterEventListener(SDL_KEYDOWN, this, 1);
	EventDistributor::instance()->unregisterEventListener(SDL_KEYUP,   this, 1);
}


const byte* Keyboard::getKeys()
{
	if (!keyGhosting) {
		return keyMatrix;
	} else {
		if (keysChanged) {
			keysChanged = false;
			memcpy(keyMatrix2, keyMatrix, sizeof(keyMatrix));
			doKeyGhosting();
		}
		return keyMatrix2;
	}
}


bool Keyboard::signalEvent(SDL_Event &event)
{
	int key;
	switch (event.type) {
	case SDL_KEYDOWN:
		// Key pressed: reset bit in keyMatrix
		key=event.key.keysym.sym;
		if (key < 0x150)
			keyMatrix[Keys[key][0]] &= ~Keys[key][1];
		break;
	case SDL_KEYUP:
		// Key released: set bit in keyMatrix
		key=event.key.keysym.sym;
		if (key < 0x150) 
			keyMatrix[Keys[key][0]] |= Keys[key][1];
		break;
	default:
		assert(false);
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
		for (int i = 0; i < NR_KEYROWS-1; i++) {
			byte row1 = keyMatrix2[i];
			for (int j = i + 1; j < NR_KEYROWS; j++) {
				byte row2 = keyMatrix2[j];
				if ((row1 != row2) && ((row1 | row2) != 0xff)) {
					// not same and some common zero's
					//  --> inherit other zero's
					byte newRow = row1 & row2;
					keyMatrix2[i] = newRow;
					keyMatrix2[j] = newRow;
					row1 = newRow;
					changedSomething = true;
				}
			}
		}
	} while (changedSomething);
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

const byte Keyboard::Keys[336][2] = {
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
  {6,0x01},{2,0x20},{6,0x02},{6,0x10},{6,0x04},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0140 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00}
};

