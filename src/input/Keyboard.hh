// $Id$

#ifndef __INPUTS_HH__
#define __INPUTS_HH__

#include <string>
#include "openmsx.hh"
#include "EmuTime.hh"
#include "EventListener.hh"
#include "Command.hh"
#include "Schedulable.hh"

using std::string;

namespace openmsx {

class Keyboard : public EventListener
{
public:
	/**
	 * Constructs a new Keyboard object.
	 * @param keyGhosting turn keyGhosting on/off
	 */
	Keyboard(bool keyGhosting);

	virtual ~Keyboard();

	/**
	 * Returns a pointer to the current KeyBoard matrix
	 */
	const byte* getKeys();

	//EventListener
	virtual bool signalEvent(const Event& event);

	static const unsigned NR_KEYROWS = 16;

private:
	void allUp();
	void doKeyGhosting();
	void parseKeymapfile(const byte* buf, unsigned size);
	void loadKeymapfile(const string& filename);
	string processCmd(const vector<string>& tokens, bool up);
	void pressAscii(char asciiCode, bool up);

	class KeyMatrixUpCmd : public SimpleCommand {
	public:
		KeyMatrixUpCmd(Keyboard& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
	private:
		Keyboard& parent;
	} keyMatrixUpCmd;

	class KeyMatrixDownCmd : public SimpleCommand {
	public:
		KeyMatrixDownCmd(Keyboard& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
	private:
		Keyboard& parent;
	} keyMatrixDownCmd;

	class KeyInserter : public SimpleCommand, private Schedulable
	{
	public:
		KeyInserter(Keyboard& parent);
		virtual ~KeyInserter();

	private:
		void type(const string& str);
		void reschedule(const EmuTime& time);

		// Command
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;

		// Schedulable
		virtual void executeUntil(const EmuTime& time, int userData);
		virtual const string& schedName() const;

		Keyboard& parent;
		bool down;
		string text;
	} keyTypeCmd;

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

