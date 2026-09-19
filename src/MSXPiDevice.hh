#ifndef MSXPIDEVICE_HH
#define MSXPIDEVICE_HH

#include "MSXDevice.hh"
#include "EmuTime.hh"
#include "Poller.hh"
#include "Socket.hh"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>

namespace openmsx {

class DeviceConfig;

/** MSXPi interface, emulated at I/O-port level after its CPLD (firmware v1.6,
  * MSXPi.vhd in the MSXPi repository).
  *
  * THE HARDWARE
  *
  * The MSX side is a CPLD with a shift register; the Raspberry Pi is the SPI
  * master. The MSX cannot pull a byte out of the Pi, it can only start a
  * transfer, which the Pi then clocks. Every transfer moves one byte in each
  * direction at once: the byte the MSX last wrote goes to the Pi, and the Pi's
  * byte comes in. The Pi signals with its READY line that it has a byte and
  * is about to clock; with no Pi attached a pull-down keeps READY low.
  *
  * Port 0x56, read:  bit 0 is 1 while a transfer is running or the Pi is not
  *                   ready, 0 when the last transfer completed and the Pi is
  *                   ready. The other bits read 0.
  * Port 0x56, write: 0xFF resets the interface: no transfer running, port
  *                   0x5A reads 0x00, wait mode off. Any other value starts a
  *                   transfer that sends that value to the Pi.
  * Port 0x57, read:  0x0E (the firmware version) with bit 7 set when wait
  *                   mode is on: 0x0E or 0x8E.
  * Port 0x57, write: 0x01 turns wait mode on, 0x00 turns it off, other values
  *                   are ignored.
  * Port 0x5A, read:  the byte received from the Pi by the last transfer. In
  *                   wait mode a read also starts a transfer, but only when
  *                   the Pi is ready.
  * Port 0x5A, write: starts a transfer that sends the value to the Pi.
  *
  * The value written is held in a latch that the Pi copies when it starts
  * clocking, not when the MSX writes it. While a transfer is running, a
  * further write only replaces that latch.
  *
  * Software in normal (polled) mode reads a byte with: wait until port 0x56
  * reads 0, OUT (0x56),0, wait until port 0x56 reads 0, IN (0x5A). It writes
  * one with: wait until port 0x56 reads 0, OUT (0x5A),byte.
  *
  * In wait mode the interface holds the Z80's /WAIT line for the duration of
  * a transfer, so a read or write only completes once the byte has moved and
  * INIR/OTIR need no polling. For writes this is what keeps the next OUT from
  * replacing the latch before the Pi took the previous byte. /WAIT is only
  * held while the Pi's READY is up: with the Pi not ready, a wait-mode read
  * returns the previous byte at once and starts nothing, which is why drivers
  * check port 0x56 before a burst.
  *
  * Mid-transfer the shift register holds a partly shifted value, which
  * port 0x5A returns. Correct software never observes it (it waits for 0x56,
  * or is held by /WAIT); it is emulated only because the hardware does it.
  *
  * THE EMULATION
  *
  * The Pi is replaced by msxpi-server.py over TCP. The link carries what the
  * Pi's pins carry ("virtual SPI"), in two-byte frames:
  *
  *   server -> device   01 d   offer: READY up, Pi sends d, READY drops after
  *                      02 d   offer: as 01, but READY stays up for the next
  *                      03 00  cancel: withdraw offers not clocked yet
  *                      7E v   hello, protocol version v (sent on accept)
  *   device -> server   01 m   an offer was clocked; the MSX sent m
  *                      03 00  cancel acknowledged
  *                      7E v   hello reply
  *
  * A run of 02 offers only becomes visible once its closing 01 offer has
  * arrived, so READY is never up without a byte to go with it and no port
  * access ever waits for the socket. Offers are taken into account at the port
  * accesses themselves, which is enough because the interface's state can only
  * be observed through its ports. A /WAIT stall is charged in emulated time.
  *
  * A server that sends no hello is too old to speak this protocol. It is
  * treated like an absent Pi: READY stays low.
  */
class MSXPiDevice final : public MSXDevice
{
public:
	explicit MSXPiDevice(const DeviceConfig& config);
	~MSXPiDevice() override;

	// The interface has no reset input: an MSX reset leaves it untouched,
	// only power-up (and OUT (0x56),0xFF) clears it.
	void powerUp(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

private:
	enum class Link : uint8_t { DOWN, PROBING, FRAMED, INCOMPATIBLE };
	struct Offer {
		byte miso; // the byte the Pi sends
		bool hold; // READY stays up after this transfer
	};

	// interface model (emulation thread)
	void update(EmuTime time);
	void startTransfer(EmuTime time);
	void tryBind(EmuTime time);
	void complete();
	[[nodiscard]] bool ready() const;
	[[nodiscard]] byte shiftRegister(EmuTime time) const;
	EmuTime stall(EmuTime time);
	void interfaceReset();
	void waitForPeer(EmuTime time);

	// transport
	void readLoop();
	[[nodiscard]] bool connectSocket();
	void handleFrame(byte op, byte arg);
	void sendFrame(byte op, byte arg); // mtx must be held
	void closeSocket();

	// --- Emulation thread only -------------------------------------------
	byte latch = 0xFF;          // last value written to port 0x56 or 0x5A
	bool busy = false;          // a transfer has been started, not completed
	bool waitMode = false;
	byte srValue = 0;           // port 0x5A value when no transfer runs
	std::optional<Offer> bound; // the offer the Pi clocks the transfer with
	EmuTime startTime = EmuTime::zero(); // the Pi started clocking 'bound'
	EmuTime doneTime = EmuTime::zero();  // ... and completes it here
	unsigned boundEpoch = 0;    // connection 'bound' was offered on

	// --- Reader thread only ----------------------------------------------
	std::deque<Offer> pendingHold; // a run whose closing offer is still due
	std::optional<byte> partial;   // first byte of a frame

	// --- Shared, only accessed while holding mtx ---------------------------
	// Both threads take mtx for 'offers'; sending a frame on the socket also
	// happens under mtx, so frames from both threads never interleave.
	mutable std::mutex mtx;
	std::deque<Offer> offers;        // visible offers, oldest first
	std::condition_variable offerCv; // notified when offers become visible

	// --- Shared, atomic (no lock needed) ------------------------------------
	// offerCount always equals offers.size(): it is written only under mtx,
	// together with every change to 'offers'. It exists so the status poll on
	// port 0x56 - tens of thousands of reads per second during a transfer -
	// can test for offers without taking the lock; anything that then uses an
	// offer takes mtx and checks 'offers' itself.
	std::atomic<size_t> offerCount = 0;
	std::atomic<Link> link = Link::DOWN;
	std::atomic<unsigned> epoch = 0; // incremented on every new connection
	std::atomic<SOCKET> sock = OPENMSX_INVALID_SOCKET;
	std::atomic<bool> shouldStop = false;

	// thread & connection
	SocketActivator socketActivator; // ensure windows sockets are initialized
	std::thread thread;
	Poller poller; // to abort read-thread in a portable way
};

} // namespace openmsx

#endif
