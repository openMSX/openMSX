// $Id$

#ifndef KEYBOARD_HH
#define KEYBOARD_HH

#include "UserInputEventListener.hh"
#include "Schedulable.hh"
#include "openmsx.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class Scheduler;
class CommandController;
class UserInputEventDistributor;
class EmuTime;
class KeyMatrixUpCmd;
class KeyMatrixDownCmd;
class KeyInserter;

class Keyboard : private UserInputEventListener, private Schedulable
{
public:
	/**
	 * Constructs a new Keyboard object.
	 * @param keyGhosting turn keyGhosting on/off
	 */
	Keyboard(Scheduler& scheduler, CommandController& commandController,
	         UserInputEventDistributor& eventDistributor, bool keyGhosting);

	virtual ~Keyboard();

	/**
	 * Returns a pointer to the current KeyBoard matrix
	 */
	const byte* getKeys();

	static const unsigned NR_KEYROWS = 16;

private:
	// UserInputEventListener
	virtual void signalEvent(const UserInputEvent& event);

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	void processKey(bool down, int key);
	void doKeyGhosting();
	void parseKeymapfile(const byte* buf, unsigned size);
	void loadKeymapfile(const std::string& filename);
	std::string processCmd(const std::vector<std::string>& tokens, bool up);
	void pressAscii(char asciiCode, bool down);
	bool commonKeys(char asciiCode1, char asciiCode2);

	friend class KeyMatrixUpCmd;
	friend class KeyMatrixDownCmd;
	friend class KeyInserter;
	const std::auto_ptr<KeyMatrixUpCmd>   keyMatrixUpCmd;
	const std::auto_ptr<KeyMatrixDownCmd> keyMatrixDownCmd;
	const std::auto_ptr<KeyInserter>      keyTypeCmd;
	
	UserInputEventDistributor& eventDistributor;
	byte cmdKeyMatrix[NR_KEYROWS];
	byte userKeyMatrix[NR_KEYROWS];
	byte keyMatrix[NR_KEYROWS];
	bool keyGhosting;
	bool keysChanged;
	static const int MAX_KEYSYM = 0x150;
	static byte keyTab[MAX_KEYSYM][2];
	static short asciiTab[256][2];
};

} // namespace openmsx

#endif
