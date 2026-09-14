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

/** MSXPi interface, modelled on the CPLD v1.6 firmware (MSXPi.vhd) at the
  * I/O-port level.
  *
  * The Raspberry Pi is replaced by msxpi-server.py over TCP. On real hardware
  * the Pi drives every transfer: it raises RPI_READY with the byte it wants to
  * send, waits for the CPLD to lower SPI_CS, and clocks one full-duplex byte.
  * The TCP link carries exactly that ("virtual SPI"), in two-byte frames:
  *
  *   server -> device   01 d   offer: READY up, Pi sends d, READY drops after
  *                      02 d   offer: as 01, but READY stays up for the next
  *                      03 00  cancel: withdraw offers not clocked yet
  *                      7E v   hello, protocol version v (sent on accept)
  *   device -> server   01 m   an offer was clocked; the CPLD sent m
  *                      03 00  cancel acknowledged
  *                      7E v   hello reply
  *
  * A run of 02 offers only becomes visible once its closing 01 offer has
  * arrived, so READY is never up without a byte to go with it and no port
  * access ever waits for the socket. Offers are taken into account at the port
  * accesses themselves, which is enough because the CPLD's state can only be
  * observed through its ports. The /WAIT stall is charged in emulated time.
  *
  * A server that sends no hello is too old to speak this protocol. It is
  * treated like an absent Pi: READY stays low.
  */
class MSXPiDevice final : public MSXDevice
{
public:
	explicit MSXPiDevice(const DeviceConfig& config);
	~MSXPiDevice() override;

	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

private:
	enum class Link : uint8_t { DOWN, PROBING, FRAMED, INCOMPATIBLE };
	struct Offer {
		byte miso;
		bool hold; // READY stays up after this transfer
	};

	// CPLD model (emulation thread)
	void update(EmuTime time);
	void startTransfer(EmuTime time);
	void tryBind(EmuTime time);
	void complete();
	[[nodiscard]] bool ready(EmuTime time) const;
	[[nodiscard]] bool inReleaseTail(EmuTime time) const;
	[[nodiscard]] bool inReleaseGap(EmuTime time) const;
	[[nodiscard]] byte shiftRegister(EmuTime time) const;
	[[nodiscard]] EmuTime stall(EmuTime time);
	void cpldReset();
	void waitForPeer(EmuTime time);

	// transport
	void readLoop();
	[[nodiscard]] bool connectSocket();
	void handleFrame(byte op, byte arg);
	void sendFrame(byte op, byte arg); // mtx must be held
	void closeSocket();

	// --- CPLD state (emulation thread only) ---
	byte latch = 0xFF;          // D_buff_msx, the transparent write latch
	bool busy = false;          // SPI_en_s
	bool waitMode = false;      // wait_mode
	byte srValue = 0;           // pi_sr(7 downto 0) when no transfer runs
	EmuTime startTime = EmuTime::zero();  // E1 of the bound transfer
	std::optional<Offer> bound; // the offer clocking the current transfer
	EmuTime doneTime = EmuTime::zero();   // E10 of the bound transfer
	bool haveLast = false;      // READY tail after the last E10
	bool lastHold = false;
	EmuTime lastDone = EmuTime::zero();
	unsigned boundEpoch = 0;

	// timing of the emulated Pi, in emulated time
	EmuDuration transferTime;   // E1 -> E10
	EmuDuration readyTail;      // E10 -> READY low, for a releasing offer
	EmuDuration readyGap;       // READY low before the Pi offers again

	// --- shared with the reader thread, under mtx ---
	mutable std::mutex mtx;
	std::deque<Offer> offers;   // visible offers
	std::condition_variable offerCv; // signalled when offers become visible
	std::atomic<size_t> offerCount = 0;
	std::atomic<Link> link = Link::DOWN;
	std::atomic<unsigned> epoch = 0;

	// --- reader thread only ---
	std::deque<Offer> pendingHold; // a run whose closing offer is still due
	std::optional<byte> partial;   // first byte of a frame

	// thread & connection
	SocketActivator socketActivator; // ensure windows sockets are initialized
	std::thread thread;
	Poller poller; // to abort read-thread in a portable way
	std::atomic<SOCKET> sock = OPENMSX_INVALID_SOCKET;
	std::atomic<bool> shouldStop = false;
};

} // namespace openmsx

#endif
