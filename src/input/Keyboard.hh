#ifndef KEYBOARD_HH
#define KEYBOARD_HH

#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "Schedulable.hh"
#include "RecordedCommand.hh"
#include "serialize_meta.hh"
#include "array_ref.hh"
#include "string_ref.hh"
#include "openmsx.hh"
#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class Scheduler;
class CommandController;
class EventDistributor;
class MSXEventDistributor;
class StateChangeDistributor;
class KeyEvent;
class CapsLockAligner;
class KeyboardSettings;
class MsxKeyEventQueue;
class UnicodeKeymap;
class KeybDebuggable;
class StateChange;
class TclObject;
class Interpreter;

class Keyboard final : private MSXEventListener, private StateChangeListener
                     , private Schedulable
{
public:
	static const unsigned NR_KEYROWS = 16;

	/** Constructs a new Keyboard object.
	 * @param motherBoard ref to the motherBoard
	 * @param scheduler ref to the scheduler
	 * @param commandController ref to the command controller
	 * @param eventDistributor ref to the emu event distributor
	 * @param msxEventDistributor ref to the user input event distributor
	 * @param stateChangeDistributor ref to the state change distributor
	 * @param keyboardType contains filename extension of unicode keymap file
	 * @param hasKeypad turn MSX keypad on/off
	 * @param hasYesNoKeys this keyboard has (Japanese) Yes/No keys
	 * @param keyGhosting turn keyGhosting on/off
	 * @param keyGhostingSGCprotected Shift, Graph and Code are keyGhosting protected
	 * @param codeKanaLocks CodeKana key behave as a lock key on this machine
	 * @param graphLocks Graph key behave as a lock key on this machine
	 */
	Keyboard(MSXMotherBoard& motherBoard, Scheduler& scheduler,
	         CommandController& commandController,
	         EventDistributor& eventDistributor,
	         MSXEventDistributor& msxEventDistributor,
	         StateChangeDistributor& stateChangeDistributor,
	         string_ref keyboardType, bool hasKeypad,
	         bool hasYesNoKeys, bool keyGhosting,
	         bool keyGhostingSGCprotected, bool codeKanaLocks,
	         bool graphLocks);

	~Keyboard();

	/** Returns a pointer to the current KeyBoard matrix
	 */
	const byte* getKeys();

	void transferHostKeyMatrix(const Keyboard& source);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// MSXEventListener
	void signalEvent(const std::shared_ptr<const Event>& event,
	                 EmuTime::param time) override;
	// StateChangeListener
	void signalStateChange(const std::shared_ptr<StateChange>& event) override;
	void stopReplay(EmuTime::param time) override;

	// Schedulable
	void executeUntil(EmuTime::param time) override;

	void pressKeyMatrixEvent  (EmuTime::param time, byte row, byte press);
	void releaseKeyMatrixEvent(EmuTime::param time, byte row, byte release);
	void changeKeyMatrixEvent (EmuTime::param time, byte row, byte newValue);

	void processDeadKeyEvent(unsigned n, EmuTime::param time, bool down);
	void processCapslockEvent(EmuTime::param time, bool down);
	void processCodeKanaChange(EmuTime::param time, bool down);
	void processGraphChange(EmuTime::param time, bool down);
	void processKeypadEnterKey(EmuTime::param time, bool down);
	void processSdlKey(EmuTime::param time, bool down, int key);
	bool processQueuedEvent(const Event& event, EmuTime::param time);
	bool processKeyEvent(EmuTime::param time, bool down, const KeyEvent& keyEvent);
	void updateKeyMatrix(EmuTime::param time, bool down, int row, byte mask);
	void doKeyGhosting();
	void processCmd(Interpreter& interp, array_ref<TclObject> tokens, bool up);
	bool pressUnicodeByUser(EmuTime::param time, unsigned unicode, bool down);
	int pressAscii(unsigned unicode, bool down);
	void pressLockKeys(int lockKeysMask, bool down);
	bool commonKeys(unsigned unicode1, unsigned unicode2);
	void debug(const char* format, ...);

	CommandController& commandController;
	MSXEventDistributor& msxEventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	friend class CapsLockAligner;
	friend class MsxKeyEventQueue;

	static const int MAX_KEYSYM = 0x150;
	static const byte keyTab[MAX_KEYSYM];

	class KeyMatrixUpCmd final : public RecordedCommand {
	public:
		KeyMatrixUpCmd(CommandController& commandController,
			       StateChangeDistributor& stateChangeDistributor,
			       Scheduler& scheduler, Keyboard& keyboard);
		void execute(array_ref<TclObject> tokens, TclObject& result,
			     EmuTime::param time) override;
		std::string help(const std::vector<std::string>& tokens) const override;
	private:
		Keyboard& keyboard;
	} keyMatrixUpCmd;

	class KeyMatrixDownCmd final : public RecordedCommand {
	public:
		KeyMatrixDownCmd(CommandController& commandController,
				 StateChangeDistributor& stateChangeDistributor,
				 Scheduler& scheduler, Keyboard& keyboard);
		void execute(array_ref<TclObject> tokens, TclObject& result,
			     EmuTime::param time) override;
		std::string help(const std::vector<std::string>& tokens) const override;
	private:
		Keyboard& keyboard;
	} keyMatrixDownCmd;

	class KeyInserter final : public RecordedCommand, public Schedulable {
	public:
		KeyInserter(CommandController& commandController,
			    StateChangeDistributor& stateChangeDistributor,
			    Scheduler& scheduler, Keyboard& keyboard);
		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		void type(string_ref str);
		void reschedule(EmuTime::param time);

		// Command
		void execute(array_ref<TclObject> tokens, TclObject& result,
			     EmuTime::param time) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

		// Schedulable
		void executeUntil(EmuTime::param time) override;

		Keyboard& keyboard;
		std::string text_utf8;
		unsigned last;
		int lockKeysMask;
		bool releaseLast;
		bool oldCodeKanaLockOn;
		bool oldGraphLockOn;
		bool oldCapsLockOn;

		bool releaseBeforePress;
		unsigned typingFrequency;
	} keyTypeCmd;

	const std::unique_ptr<CapsLockAligner>  capsLockAligner;
	const std::unique_ptr<KeyboardSettings> keyboardSettings;
	const std::unique_ptr<MsxKeyEventQueue> msxKeyEventQueue;
	const std::unique_ptr<KeybDebuggable>   keybDebuggable;

	const std::unique_ptr<UnicodeKeymap> unicodeKeymap;
	unsigned dynKeymap[MAX_KEYSYM];
	byte cmdKeyMatrix [NR_KEYROWS]; // for keymatrix/type command
	byte userKeyMatrix[NR_KEYROWS]; // pressed user keys (live or replay)
	byte hostKeyMatrix[NR_KEYROWS]; // always in sync with host keyb, also during replay
	byte keyMatrix    [NR_KEYROWS]; // combination of cmdKeyMatrix and userKeyMatrix
	byte msxmodifiers;
	const bool hasKeypad;
	const bool hasYesNoKeys;
	const bool keyGhosting;
	const bool keyGhostingSGCprotected;
	const bool codeKanaLocks;
	const bool graphLocks;
	const bool sdlReleasesCapslock;
	bool keysChanged;
	bool msxCapsLockOn;
	bool msxCodeKanaLockOn;
	bool msxGraphLockOn;
};
SERIALIZE_CLASS_VERSION(Keyboard, 2);

} // namespace openmsx

#endif
