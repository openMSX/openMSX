// $Id$

#ifndef KEYBOARD_HH
#define KEYBOARD_HH

#include <string>
#include "openmsx.hh"
#include "UserInputEventListener.hh"
#include "Command.hh"
#include "Schedulable.hh"

namespace openmsx {

class Scheduler;
class CommandController;
class UserInputEventDistributor;
class EmuTime;

class Keyboard : private UserInputEventListener
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

	void doKeyGhosting();
	void parseKeymapfile(const byte* buf, unsigned size);
	void loadKeymapfile(const std::string& filename);
	std::string processCmd(const std::vector<std::string>& tokens, bool up);
	void pressAscii(char asciiCode, bool down);
	bool commonKeys(char asciiCode1, char asciiCode2);

	class KeyMatrixUpCmd : public SimpleCommand {
	public:
		KeyMatrixUpCmd(CommandController& commandController,
		               Keyboard& keyboard);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		Keyboard& keyboard;
	} keyMatrixUpCmd;

	class KeyMatrixDownCmd : public SimpleCommand {
	public:
		KeyMatrixDownCmd(CommandController& commandController,
		                 Keyboard& keyboard);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		Keyboard& keyboard;
	} keyMatrixDownCmd;

	class KeyInserter : public SimpleCommand, private Schedulable
	{
	public:
		KeyInserter(Scheduler& scheduler,
		            CommandController& commandController,
		            Keyboard& keyboard);
		virtual ~KeyInserter();

	private:
		void type(const std::string& str);
		void reschedule(const EmuTime& time);

		// Command
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;

		// Schedulable
		virtual void executeUntil(const EmuTime& time, int userData);
		virtual const std::string& schedName() const;

		Keyboard& keyboard;
		char last;
		std::string text;
	} keyTypeCmd;

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
