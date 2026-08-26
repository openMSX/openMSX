#include "SerialPort.hh"

#include "ReadDir.hh"
#include "StringOp.hh"
#include "one_of.hh"
#include "strCat.hh"
#include "utf8_checked.hh"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#ifdef __APPLE__
#include <IOKit/serial/ioss.h>
#endif
#endif

namespace openmsx::serial {

// Windows already defines INVALID_HANDLE_VALUE as the HANDLE sentinel; on
// POSIX serial handles are plain ints with -1 as the invalid value.
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE -1
#endif

// Captures the platform error state at the moment of failure.
static ErrorCode currentError()
{
#ifdef _WIN32
	return ErrorCode{GetLastError()};
#else
	return ErrorCode{errno};
#endif
}

#ifdef _WIN32

static DCB serialParamsToDCB(const SerialParams& params)
{
	DCB dcb = {};
	dcb.DCBlength = sizeof(dcb);
	dcb.BaudRate = params.baudRate;
	dcb.fBinary = TRUE;
	dcb.fParity = params.parity != 0 ? TRUE : FALSE;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fDsrSensitivity = FALSE;
	dcb.fTXContinueOnXoff = FALSE;
	dcb.fOutX = FALSE;
	dcb.fInX = FALSE;
	dcb.fErrorChar = FALSE;
	dcb.fNull = FALSE;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.fAbortOnError = TRUE;
	dcb.ByteSize = static_cast<BYTE>(params.dataBits);
	switch (params.stopBits) {
	case 1:  dcb.StopBits = ONESTOPBIT; break;
	case 2:  dcb.StopBits = TWOSTOPBITS; break;
	default: dcb.StopBits = ONE5STOPBITS; break; // 1.5
	}
	switch (params.parity) {
	case 0:  dcb.Parity = NOPARITY; break;
	case 1:  dcb.Parity = ODDPARITY; break;
	case 2:  dcb.Parity = EVENPARITY; break;
	case 3:  dcb.Parity = MARKPARITY; break;
	case 4:  dcb.Parity = SPACEPARITY; break;
	default: dcb.Parity = NOPARITY; break;
	}
	return dcb;
}

std::expected<Handle, ErrorCode> open(zstring_view portName, const SerialParams& params)
{
	std::string fullName = R"(\\.\)";
	fullName += portName;
	HANDLE h = CreateFileA(
		fullName.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0, nullptr,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		nullptr);
	if (h == INVALID_HANDLE_VALUE) {
		return std::unexpected(currentError());
	}

	if (!SetupComm(h, 4096, 4096)) {
		auto ec = currentError();
		CloseHandle(h);
		return std::unexpected(ec);
	}

	if (DCB dcb = serialParamsToDCB(params);!SetCommState(h, &dcb)) {
		auto ec = currentError();
		CloseHandle(h);
		return std::unexpected(ec);
	}

	COMMTIMEOUTS timeouts = {};
	timeouts.ReadIntervalTimeout = 0;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 0;
	if (!SetCommTimeouts(h, &timeouts)) {
		auto ec = currentError();
		CloseHandle(h);
		return std::unexpected(ec);
	}

	return Handle(h);
}

IoResult Handle::read(std::span<uint8_t> buf) const
{
	DWORD bytesRead = 0;
	OVERLAPPED ov = {};
	ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!ov.hEvent) return std::unexpected(currentError());

	if (!ReadFile(handle, buf.data(), static_cast<DWORD>(buf.size()), &bytesRead, &ov)) {
		if ((GetLastError() == ERROR_IO_PENDING) && (WaitForSingleObject(ov.hEvent, INFINITE) == WAIT_OBJECT_0)) {
			if (!GetOverlappedResult(handle, &ov, &bytesRead, FALSE)) {
				auto ec = currentError();
				CloseHandle(ov.hEvent);
				return std::unexpected(ec);
			}
		} else {
			auto ec = currentError();
			CloseHandle(ov.hEvent);
			return std::unexpected(ec);
		}
	}
	CloseHandle(ov.hEvent);
	return IoResult(bytesRead);
}

IoResult Handle::write(std::span<const uint8_t> buf) const
{
	DWORD bytesWritten = 0;
	OVERLAPPED ov = {};
	ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!ov.hEvent) return std::unexpected(currentError());

	if (!WriteFile(handle, buf.data(), static_cast<DWORD>(buf.size()), &bytesWritten, &ov)) {
		if ((GetLastError() == ERROR_IO_PENDING) && (WaitForSingleObject(ov.hEvent, INFINITE) == WAIT_OBJECT_0)) {
			if (!GetOverlappedResult(handle, &ov, &bytesWritten, FALSE)) {
				auto ec = currentError();
				CloseHandle(ov.hEvent);
				return std::unexpected(ec);
			}
		} else {
			auto ec = currentError();
			CloseHandle(ov.hEvent);
			return std::unexpected(ec);
		}
	}
	CloseHandle(ov.hEvent);
	if (bytesWritten > 0) return IoResult(bytesWritten);
	return std::unexpected(ErrorCode{});
}

std::expected<void, ErrorCode> Handle::set_params(const SerialParams& params) const
{
	DCB dcb = {};
	dcb.DCBlength = sizeof(dcb);
	if (!GetCommState(handle, &dcb)) return std::unexpected(currentError());

	dcb.BaudRate = params.baudRate;
	dcb.fParity = params.parity != 0 ? TRUE : FALSE;
	dcb.ByteSize = static_cast<BYTE>(params.dataBits);
	switch (params.stopBits) {
	case 1:  dcb.StopBits = ONESTOPBIT; break;
	case 2:  dcb.StopBits = TWOSTOPBITS; break;
	default: dcb.StopBits = ONE5STOPBITS; break;
	}
	switch (params.parity) {
	case 0:  dcb.Parity = NOPARITY; break;
	case 1:  dcb.Parity = ODDPARITY; break;
	case 2:  dcb.Parity = EVENPARITY; break;
	case 3:  dcb.Parity = MARKPARITY; break;
	case 4:  dcb.Parity = SPACEPARITY; break;
	default: dcb.Parity = NOPARITY; break;
	}

	if (!SetCommState(handle, &dcb)) return std::unexpected(currentError());
	return {};
}

std::expected<void, ErrorCode> Handle::set_dtr(bool on) const
{
	if (!EscapeCommFunction(handle, on ? SETDTR : CLRDTR)) {
		return std::unexpected(currentError());
	}
	return {};
}

std::expected<void, ErrorCode> Handle::set_rts(bool on) const
{
	if (!EscapeCommFunction(handle, on ? SETRTS : CLRRTS)) {
		return std::unexpected(currentError());
	}
	return {};
}

std::expected<Handle::ModemStatus, ErrorCode> Handle::get_modem_status() const
{
	DWORD status;
	if (!GetCommModemStatus(handle, &status)) {
		return std::unexpected(currentError());
	}
	return ModemStatus{
		.cts = (status & MS_CTS_ON) != 0,
		.dsr = (status & MS_DSR_ON) != 0,
		.dcd = (status & MS_RLSD_ON) != 0,
		.ri  = (status & MS_RING_ON) != 0,
	};
}

std::vector<std::string> list_ports()
{
	std::vector<std::string> ports;
	for (int i = 1; i <= 256; ++i) {
		auto name = strCat("COM", i);
		std::string fullName = R"(\\.\)" + name;
		HANDLE h = CreateFileA(
			fullName.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			0, nullptr,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
			nullptr);
		if (h != INVALID_HANDLE_VALUE) {
			CloseHandle(h);
			ports.push_back(name);
		}
	}
	return ports;
}

#else // POSIX (Linux / macOS / BSD)

struct BaudMapItem {
	unsigned baud;
	speed_t speed;
};

static constexpr auto BAUDRATE_MAP = std::to_array<BaudMapItem>({
	{ 50, B50 },
	{ 75, B75 },
	{ 110, B110 },
	{ 134, B134 },
	{ 150, B150 },
	{ 200, B200 },
	{ 300, B300 },
	{ 600, B600 },
	{ 1200, B1200 },
	{ 1800, B1800 },
	{ 2400, B2400 },
	{ 4800, B4800 },
	{ 9600, B9600 },
	{ 19200, B19200 },
	{ 38400, B38400 },
	{ 57600, B57600 },
	{ 115200, B115200 },
	{ 230400, B230400 },
#ifdef B460800
	{ 460800, B460800 },
#endif
#ifdef B921600
	{ 921600, B921600 },
#endif
});

#ifdef __linux__
#ifndef BOTHER
#define BOTHER 0010000
#endif
#endif

static speed_t baudToSpeedExact(unsigned baud)
{
	for (auto& item : BAUDRATE_MAP) {
		if (item.baud == baud) return item.speed;
	}
	return B0;
}

#ifndef __linux__
static speed_t baudToSpeedNearest(unsigned baud)
{
	speed_t bestSpeed = B115200;
	unsigned bestDiff = UINT_MAX;
	for (auto& item : BAUDRATE_MAP) {
		unsigned diff = (item.baud > baud) ? item.baud - baud
		                                  : baud - item.baud;
		if (diff < bestDiff) {
			bestSpeed = item.speed;
			bestDiff = diff;
			if (diff == 0) break;
		}
	}
	return bestSpeed;
}
#endif

static void set_termios_baud(struct termios& tio, unsigned baud)
{
	if (speed_t speed = baudToSpeedExact(baud); speed != B0) {
		cfsetispeed(&tio, speed);
		cfsetospeed(&tio, speed);
	} else {
#ifdef __linux__
		cfsetispeed(&tio, BOTHER);
		cfsetospeed(&tio, BOTHER);
		tio.c_ispeed = static_cast<speed_t>(baud);
		tio.c_ospeed = static_cast<speed_t>(baud);
#else
		speed = baudToSpeedNearest(baud);
		cfsetispeed(&tio, speed);
		cfsetospeed(&tio, speed);
#endif
	}
}

static bool apply_custom_baud_post([[maybe_unused]] int fd, unsigned baud)
{
	if (baudToSpeedExact(baud) != B0) return true;
#ifdef __APPLE__
	int speed = static_cast<int>(baud);
	return ioctl(fd, IOSSIOSPEED, &speed) >= 0;
#else
	return true;
#endif
}

std::expected<Handle, ErrorCode> open(zstring_view portName, const SerialParams& params)
{
	int fd = ::open(portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) return std::unexpected(currentError());

	struct termios tio = {};
	if (tcgetattr(fd, &tio) < 0) {
		auto ec = currentError();
		::close(fd);
		return std::unexpected(ec);
	}

	// Input modes
	tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	// Output modes
	tio.c_oflag &= ~OPOST;
	// Control modes
	tio.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD | CRTSCTS);
	tio.c_cflag |= CS8 | CREAD | CLOCAL;
	// Local modes
	tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	// Apply data bits, parity, stop bits
	tio.c_cflag &= ~CSIZE;
	switch (params.dataBits) {
	case 5: tio.c_cflag |= CS5; break;
	case 6: tio.c_cflag |= CS6; break;
	case 7: tio.c_cflag |= CS7; break;
	default: tio.c_cflag |= CS8; break;
	}
	if (params.parity != 0) {
		tio.c_cflag |= PARENB;
		if (params.parity == 1) tio.c_cflag |= PARODD; // odd
	}
	if (params.stopBits >= 2) {
		tio.c_cflag |= CSTOPB;
	}

	// Set timeouts for non-blocking read
	tio.c_cc[VMIN] = 0;
	tio.c_cc[VTIME] = 1; // 100ms timeout

	set_termios_baud(tio, params.baudRate);
	if (tcsetattr(fd, TCSANOW, &tio) < 0) {
		auto ec = currentError();
		::close(fd);
		return std::unexpected(ec);
	}
	if (!apply_custom_baud_post(fd, params.baudRate)) {
		auto ec = currentError();
		::close(fd);
		return std::unexpected(ec);
	}

	// Clear non-blocking for regular I/O
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0) {
		fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
	}

	tcflush(fd, TCIOFLUSH);

	return Handle(fd);
}

IoResult Handle::read(std::span<uint8_t> buf) const
{
	auto n = ::read(handle, buf.data(), buf.size());
	if (n > 0) return IoResult(n);
	if (n == 0) return IoResult(0);
	return std::unexpected(currentError());
}

IoResult Handle::write(std::span<const uint8_t> buf) const
{
	auto n = ::write(handle, buf.data(), buf.size());
	if (n > 0) return IoResult(n);
	return std::unexpected(currentError());
}

std::expected<void, ErrorCode> Handle::set_params(const SerialParams& params) const
{
	struct termios tio = {};
	if (tcgetattr(handle, &tio) < 0) return std::unexpected(currentError());

	tio.c_cflag &= ~CSIZE;
	switch (params.dataBits) {
	case 5: tio.c_cflag |= CS5; break;
	case 6: tio.c_cflag |= CS6; break;
	case 7: tio.c_cflag |= CS7; break;
	default: tio.c_cflag |= CS8; break;
	}
	if (params.parity != 0) {
		tio.c_cflag |= PARENB;
		if (params.parity == 1) tio.c_cflag |= PARODD;
	} else {
		tio.c_cflag &= ~PARENB;
	}
	if (params.stopBits >= 2) {
		tio.c_cflag |= CSTOPB;
	} else {
		tio.c_cflag &= ~CSTOPB;
	}

	set_termios_baud(tio, params.baudRate);
	if (tcsetattr(handle, TCSANOW, &tio) < 0) return std::unexpected(currentError());
	if (!apply_custom_baud_post(handle, params.baudRate)) {
		return std::unexpected(currentError());
	}
	return {};
}

std::expected<void, ErrorCode> Handle::set_dtr(bool on) const
{
	int status = TIOCM_DTR;
	if (ioctl(handle, on ? TIOCMBIS : TIOCMBIC, &status) < 0) {
		return std::unexpected(currentError());
	}
	return {};
}

std::expected<void, ErrorCode> Handle::set_rts(bool on) const
{
	int status = TIOCM_RTS;
	if (ioctl(handle, on ? TIOCMBIS : TIOCMBIC, &status) < 0) {
		return std::unexpected(currentError());
	}
	return {};
}

std::expected<Handle::ModemStatus, ErrorCode> Handle::get_modem_status() const
{
	int status;
	if (ioctl(handle, TIOCMGET, &status) < 0) {
		return std::unexpected(currentError());
	}
	return ModemStatus{
		.cts = (status & TIOCM_CTS) != 0,
		.dsr = (status & TIOCM_DSR) != 0,
		.dcd = (status & TIOCM_CD) != 0,
		.ri  = (status & TIOCM_RI) != 0,
	};
}

// Serial device name prefixes in /dev (OMAP, CH340/341/343, ...)
static constexpr auto portPrefixes = std::to_array<std::string_view>({
	"ttyS", "ttyUSB", "ttyACM", "ttyAMA",
	"tty.usb", "tty.USA", "cu.usb", "cu.USA",
	"ttyXRUSB", "ttyO", "ttyCH"
});

std::vector<std::string> list_ports()
{
	std::vector<std::string> ports;
	ReadDir dir("/dev");
	while (auto* entry = dir.getEntry()) {
		for (auto pfx : portPrefixes) {
			if (std::string_view(entry->d_name).starts_with(pfx)) {
				ports.push_back(strCat("/dev/", entry->d_name));
				break;
			}
		}
	}
	std::ranges::sort(ports);
	return ports;
}

#endif

// Handle move/destructor code, shared between the Windows and POSIX
// implementations (only the "close" call and the invalid-handle sentinel
// are platform-specific).
Handle::Handle(serial_handle_t handle_)
	: handle(handle_)
{
}

Handle::Handle(Handle&& other) noexcept
	: handle(std::exchange(other.handle, INVALID_HANDLE_VALUE))
{
}

Handle& Handle::operator=(Handle&& other) noexcept
{
	if (this != &other) {
		release();
		handle = std::exchange(other.handle, INVALID_HANDLE_VALUE);
	}
	return *this;
}

Handle::~Handle()
{
	release();
}

void Handle::release() noexcept
{
	if (handle != INVALID_HANDLE_VALUE) {
#ifdef _WIN32
		CloseHandle(handle);
#else
		::close(handle);
#endif
		handle = INVALID_HANDLE_VALUE;
	}
}

std::string to_string(ErrorCode ec)
{
#ifdef _WIN32
	std::array<wchar_t, 512> buffer{};

	DWORD char_count = FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		static_cast<DWORD>(ec.value),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buffer.data(),
		static_cast<DWORD>(buffer.size()),
		nullptr
	);

	if (char_count == 0) {
		return "Unknown error (" + std::to_string(ec.value) + ")";
	}
	std::wstring_view sv(buffer.data(), char_count);

	while (!sv.empty() && sv.back() == one_of(L'\n', L'\r')) {
		sv.remove_suffix(1);
	}

	return utf8::utf16to8(sv);
#else
	return strerror(ec.value);
#endif
}

} // namespace openmsx::serial
