// $Id$

#ifndef __INPUTS_HH__
#define __INPUTS_HH__

#include <string>
#include "openmsx.hh"
#include "EventListener.hh"
#include "Command.hh"

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

	/**
	 * Destructor
	 */
	virtual ~Keyboard();

	/**
	 * Returns a pointer to the current KeyBoard matrix
	 */
	const byte* getKeys();

	//EventListener
	virtual bool signalEvent(const Event& event) throw();

	static const unsigned NR_KEYROWS = 16;

private:
	void doKeyGhosting();
	void parseKeymapfile(const byte* buf, unsigned size);
	void loadKeymapfile(const string& filename);
	string processCmd(const vector<string>& tokens, bool up);

	class KeyMatrixUpCmd : public Command {
	public:
		KeyMatrixUpCmd(Keyboard& parent);
		virtual string execute(const vector<string> &tokens)
			throw(CommandException);
		virtual string help(const vector<string> &tokens) const
			throw();
	private:
		Keyboard& parent;
	} keyMatrixUpCmd;

	class KeyMatrixDownCmd : public Command {
	public:
		KeyMatrixDownCmd(Keyboard& parent);
		virtual string execute(const vector<string> &tokens)
			throw(CommandException);
		virtual string help(const vector<string> &tokens) const
			throw();
	private:
		Keyboard& parent;
	} keyMatrixDownCmd;

	byte cmdKeyMatrix[NR_KEYROWS];
	byte userKeyMatrix[NR_KEYROWS];
	byte keyMatrix[NR_KEYROWS];
	bool keyGhosting;
	bool keysChanged;
	static const int MAX_KEYSYM = 0x150;
	static byte Keys[MAX_KEYSYM][2];
};

} // namespace openmsx
#endif
