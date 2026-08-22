#ifndef SERIALPORT_HH
#define SERIALPORT_HH

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace openmsx::serial {

#ifdef _WIN32
using serial_handle_t = HANDLE;
#else
using serial_handle_t = int;
#endif

// Platform-specific error value, captured at the moment of failure (a copy
// of errno on POSIX, GetLastError() on Windows). It is safe to store the
// value and format it later with to_string(): no other call can overwrite
// the error state in between.
struct ErrorCode {
#ifdef _WIN32
	unsigned long value = 0;
#else
	int value = 0;
#endif
};

[[nodiscard]] std::string to_string(ErrorCode ec);

struct SerialParams {
	unsigned baudRate = 115200;
	int dataBits = 8;
	int stopBits = 1; // 1, 2 (1.5 is not supported on most platforms)
	int parity = 0;   // 0=none, 1=odd, 2=even, 3=mark, 4=space
};

// Result of a read()/write(): success carries the number of bytes
// transferred (read() may return 0 = no data available within the port's
// timeouts), failure carries the ErrorCode.
using IoResult = std::expected<size_t, ErrorCode>;

// An open serial port. Move-only RAII: the destructor closes the port, so
// a Handle cannot outlive its open port (there is no explicit close()).
// Not thread-safe by itself: callers must synchronize concurrent access
// (RS232Raw joins its worker thread before closing the port).
struct Handle {
	Handle(const Handle&) = delete;
	Handle& operator=(const Handle&) = delete;
	Handle(Handle&& other) noexcept;
	Handle& operator=(Handle&& other) noexcept;
	~Handle();

	// Blocking reads/writes.
	[[nodiscard]] IoResult read(std::span<char> buf) const;
	[[nodiscard]] IoResult write(std::span<const char> buf) const;

	// The underlying OS handle/fd, e.g. for integration with openMSX's
	// Poller on POSIX.
	[[nodiscard]] serial_handle_t native_handle() const { return handle; }

	[[nodiscard]] std::expected<void, ErrorCode> set_params(const SerialParams& params) const;
	[[nodiscard]] std::expected<void, ErrorCode> set_dtr(bool on) const;
	[[nodiscard]] std::expected<void, ErrorCode> set_rts(bool on) const;

	struct ModemStatus {
		bool cts, dsr, dcd, ri;
	};
	// Currently unused by openMSX (RS232Raw keeps fixed modem-status
	// values instead of polling the hardware); kept for future use.
	[[nodiscard]] std::expected<ModemStatus, ErrorCode> get_modem_status() const;

private:
	friend std::expected<Handle, ErrorCode> open(std::string_view portName, const SerialParams& params);
	explicit Handle(serial_handle_t handle_);
	void release() noexcept;
	serial_handle_t handle;
};

// Opens the port with the given parameters. On failure the returned
// ErrorCode is valid and no Handle is produced.
[[nodiscard]] std::expected<Handle, ErrorCode> open(std::string_view portName, const SerialParams& params);

[[nodiscard]] std::vector<std::string> list_ports();

} // namespace openmsx::serial

#endif
