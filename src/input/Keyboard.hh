#ifndef KEYBOARD_HH
#define KEYBOARD_HH

#include "KeyboardSettings.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "UnicodeKeymap.hh"

#include "Event.hh"
#include "EventListener.hh"
#include "RecordedCommand.hh"
#include "Schedulable.hh"
#include "SimpleDebuggable.hh"
#include "serialize_meta.hh"

#include <array>
#include <cstdint>
#include <deque>
#include <span>
#include <string_view>
#include <vector>

namespace openmsx {

class MSXMotherBoard;
class Scheduler;
class CommandController;
class DeviceConfig;
class EventDistributor;
class MSXEventDistributor;
class StateChangeDistributor;
class StateChange;
class TclObject;
class Interpreter;

struct KeyCodeMsxMapping {
	SDL_Keycode hostKeyCode;
	KeyMatrixPosition msx;
};
struct ScanCodeMsxMapping {
	SDL_Scancode hostScanCode;
	KeyMatrixPosition msx;
};

class Keyboard final : private MSXEventListener, private StateChangeListener
                     , private Schedulable
{
public:
	static constexpr int MAX_KEYSYM = 0x150;
	enum class Matrix { MSX, SVI, CVJOY, SEGA, NUM };

	/** Constructs a new Keyboard object.
	 * @param motherBoard ref to the motherBoard
	 * @param scheduler ref to the scheduler
	 * @param commandController ref to the command controller
	 * @param eventDistributor ref to the emu event distributor
	 * @param msxEventDistributor ref to the user input event distributor
	 * @param stateChangeDistributor ref to the state change distributor
	 * @param matrix which system's keyboard matrix to use
	 * @param config ref to the device configuration
	 */
	Keyboard(MSXMotherBoard& motherBoard, Scheduler& scheduler,
	         CommandController& commandController,
	         EventDistributor& eventDistributor,
	         MSXEventDistributor& msxEventDistributor,
	         StateChangeDistributor& stateChangeDistributor,
	         Matrix matrix, const DeviceConfig& config);

	~Keyboard();

	[[nodiscard]] const MsxChar2Unicode& getMsxChar2Unicode() const;

	/** Returns a pointer to the current KeyBoard matrix
	 */
	[[nodiscard]] std::span<const uint8_t, KeyMatrixPosition::NUM_ROWS> getKeys() const;

	void transferHostKeyMatrix(const Keyboard& source);
	void setFocus(bool newFocus, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// MSXEventListener
	void signalMSXEvent(const Event& event,
	                    EmuTime::param time) noexcept override;
	// StateChangeListener
	void signalStateChange(const StateChange& event) override;
	void stopReplay(EmuTime::param time) noexcept override;

	// Schedulable
	void executeUntil(EmuTime::param time) override;

	void syncHostKeyMatrix(EmuTime::param time);
	void pressKeyMatrixEvent(EmuTime::param time, KeyMatrixPosition pos);
	void releaseKeyMatrixEvent(EmuTime::param time, KeyMatrixPosition pos);
	void changeKeyMatrixEvent (EmuTime::param time, uint8_t row, uint8_t newValue);

	void processCapslockEvent(EmuTime::param time, bool down);
	void processCodeKanaChange(EmuTime::param time, bool down);
	void processGraphChange(EmuTime::param time, bool down);
	void processKeypadEnterKey(EmuTime::param time, bool down);
	void processSdlKey(EmuTime::param time, SDLKey key);
	bool processQueuedEvent(const Event& event, EmuTime::param time);
	bool processKeyEvent(EmuTime::param time, bool down, const KeyEvent& keyEvent);
	void updateKeyMatrix(EmuTime::param time, bool down, KeyMatrixPosition pos);
	void processCmd(Interpreter& interp, std::span<const TclObject> tokens, bool up);
	bool pressUnicodeByUser(
			EmuTime::param time, UnicodeKeymap::KeyInfo keyInfo, unsigned unicode,
			bool down);
	uint8_t pressAscii(unsigned unicode, bool down);
	void pressLockKeys(uint8_t lockKeysMask, bool down);
	[[nodiscard]] bool commonKeys(unsigned unicode1, unsigned unicode2) const;
	void debug(const char* format, ...) const;

	/** Returns a bit vector in which the bit for a modifier is set iff that
	  * modifier is a lock key and must be toggled before the given key input
	  * can be produced.
	  */
	uint8_t needsLockToggle(const UnicodeKeymap::KeyInfo& keyInfo) const;

private:
	CommandController& commandController;
	MSXEventDistributor& msxEventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	std::vector<KeyCodeMsxMapping> keyCodeTab;
	std::vector<ScanCodeMsxMapping> scanCodeTab;

	const array_with_enum_index<UnicodeKeymap::KeyInfo::Modifier, KeyMatrixPosition>& modifierPos;

	struct KeyMatrixUpCmd final : RecordedCommand {
		KeyMatrixUpCmd(CommandController& commandController,
			       StateChangeDistributor& stateChangeDistributor,
			       Scheduler& scheduler);
		void execute(std::span<const TclObject> tokens, TclObject& result,
			     EmuTime::param time) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} keyMatrixUpCmd;

	struct KeyMatrixDownCmd final : RecordedCommand {
		KeyMatrixDownCmd(CommandController& commandController,
				 StateChangeDistributor& stateChangeDistributor,
				 Scheduler& scheduler);
		void execute(std::span<const TclObject> tokens, TclObject& result,
			     EmuTime::param time) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} keyMatrixDownCmd;

	class KeyInserter final : public RecordedCommand, public Schedulable {
	public:
		KeyInserter(CommandController& commandController,
			    StateChangeDistributor& stateChangeDistributor,
			    Scheduler& scheduler);
		[[nodiscard]] bool isActive() const { return pendingSyncPoint(); }
		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		void type(std::string_view str);
		void reschedule(EmuTime::param time);

		// Command
		void execute(std::span<const TclObject> tokens, TclObject& result,
			     EmuTime::param time) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

		// Schedulable
		void executeUntil(EmuTime::param time) override;

	private:
		std::string text_utf8;
		unsigned last = 0;
		uint8_t lockKeysMask = 0;
		bool releaseLast = false;
		uint8_t oldLocksOn = 0;

		bool releaseBeforePress = false;
		int typingFrequency = 15;
	} keyTypeCmd;

	struct Msxcode2UnicodeCmd final : public Command {
		explicit Msxcode2UnicodeCmd(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} msxcode2UnicodeCmd;

	struct Unicode2MsxcodeCmd final : public Command {
		explicit Unicode2MsxcodeCmd(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} unicode2MsxcodeCmd;

	class CapsLockAligner final : private EventListener, private Schedulable {
	public:
		CapsLockAligner(EventDistributor& eventDistributor,
				Scheduler& scheduler);
		~CapsLockAligner();

	private:
		// EventListener
		bool signalEvent(const Event& event) override;

		// Schedulable
		void executeUntil(EmuTime::param time) override;

		void alignCapsLock(EmuTime::param time);

	private:
		EventDistributor& eventDistributor;

		enum CapsLockAlignerStateType {
			MUST_ALIGN_CAPSLOCK, MUST_DISTRIBUTE_KEY_RELEASE, IDLE
		} state = IDLE;
	} capsLockAligner;

	KeyboardSettings keyboardSettings;

	class MsxKeyEventQueue final : public Schedulable {
	public:
		MsxKeyEventQueue(Scheduler& scheduler, Interpreter& interp);
		void process_asap(EmuTime::param time,
		                  const Event& event);
		void clear();
		template<typename Archive>
		void serialize(Archive& ar, unsigned version);
	private:
		// Schedulable
		void executeUntil(EmuTime::param time) override;
	private:
		std::deque<Event> eventQueue;
		Interpreter& interp;
	} msxKeyEventQueue;

	struct KeybDebuggable final : SimpleDebuggable {
		explicit KeybDebuggable(MSXMotherBoard& motherBoard);
		[[nodiscard]] uint8_t read(unsigned address) override;
		void write(unsigned address, uint8_t value) override;
	} keybDebuggable;

	UnicodeKeymap unicodeKeymap;
	// Remembers the last unicode for a key-press-keycode. To be used later
	// on the corresponding key-release, because those don't have unicode info.
	std::vector<std::pair<SDL_Keycode, uint32_t>> lastUnicodeForKeycode; // sorted on SDL_keycode

	/** Keyboard matrix state for 'keymatrix' command. */
	std::array<uint8_t, KeyMatrixPosition::NUM_ROWS> cmdKeyMatrix;
	/** Keyboard matrix state for 'type' command. */
	std::array<uint8_t, KeyMatrixPosition::NUM_ROWS> typeKeyMatrix;
	/** Keyboard matrix state for pressed user keys (live or replay). */
	std::array<uint8_t, KeyMatrixPosition::NUM_ROWS> userKeyMatrix;
	/** Keyboard matrix state that is always in sync with host keyb, also during replay. */
	std::array<uint8_t, KeyMatrixPosition::NUM_ROWS> hostKeyMatrix;
	/** Combination of 'cmdKeyMatrix', 'typeKeyMatrix' and 'userKeyMatrix'. */
	mutable std::array<uint8_t, KeyMatrixPosition::NUM_ROWS> keyMatrix;

	uint8_t msxModifiers = 0xff;

	/** True iff keyboard includes a numeric keypad. */
	const bool hasKeypad;
	/** True iff changes to keyboard row 11 must be rejected.
	  * On MSX, row 11 contains the (Japanese) Yes/No keys.
	  */
	const bool blockRow11;
	/** True iff pressing multiple keys at once can add ghost key presses. */
	const bool keyGhosting;
	/** True iff Shift, Graph and Code are protected against key ghosting. */
	const bool keyGhostingSGCprotected;
	/** Bit vector where each modifier's bit (using KeyInfo::Modifier's
	  * numbering) is set iff it is a lock key.
	  */
	const uint8_t modifierIsLock;
	mutable bool keysChanged = false;
	/** Bit vector where each modifier's bit (using KeyInfo::Modifier's
	  * numbering) is set iff it is a lock key that is currently on in
	  * the emulated machine.
	  */
	uint8_t locksOn = 0;

	bool focus = true;
};
SERIALIZE_CLASS_VERSION(Keyboard, 4);

} // namespace openmsx

#endif
