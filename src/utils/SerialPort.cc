#include "SerialPort.hh"

#include "utf8_checked.hh"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#ifndef _WIN32
#include <dirent.h>
#include <sys/ioctl.h>
#ifdef __APPLE__
#include <IOKit/serial/ioss.h>
#endif
#endif

namespace openmsx {

std::string serial_error()
{
#ifdef _WIN32
	wchar_t* s = nullptr;
	FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, GetLastError(), 0, reinterpret_cast<LPWSTR>(&s),
		0, nullptr);
	std::string result = utf8::utf16to8(s);
	LocalFree(s);
	return result;
#else
	return strerror(errno);
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

bool serial_open(serial_handle_t& handle, std::string_view portName, const SerialParams& params)
{
	std::string fullName = "\\\\.\\";
	fullName += portName;
	handle = CreateFileA(
		fullName.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0, nullptr,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		return false;
	}

	if (!SetupComm(handle, 4096, 4096)) {
		serial_close(handle);
		return false;
	}

	DCB dcb = serialParamsToDCB(params);
	if (!SetCommState(handle, &dcb)) {
		serial_close(handle);
		return false;
	}

	COMMTIMEOUTS timeouts = {};
	timeouts.ReadIntervalTimeout = 0;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 0;
	if (!SetCommTimeouts(handle, &timeouts)) {
		serial_close(handle);
		return false;
	}

	return true;
}

void serial_close(serial_handle_t& handle)
{
	if (handle != INVALID_HANDLE_VALUE) {
		CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
	}
}

ptrdiff_t serial_read(serial_handle_t handle, char* buf, size_t count)
{
	DWORD bytesRead = 0;
	OVERLAPPED ov = {};
	ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!ov.hEvent) return -1;

	if (!ReadFile(handle, buf, static_cast<DWORD>(count), &bytesRead, &ov)) {
		if (GetLastError() == ERROR_IO_PENDING) {
			if (WaitForSingleObject(ov.hEvent, INFINITE) == WAIT_OBJECT_0) {
				if (!GetOverlappedResult(handle, &ov, &bytesRead, FALSE)) {
					bytesRead = 0;
				}
			}
		} else {
			bytesRead = 0;
		}
	}
	CloseHandle(ov.hEvent);
	return bytesRead > 0 ? static_cast<ptrdiff_t>(bytesRead) : (bytesRead == 0 ? 0 : -1);
}

ptrdiff_t serial_write(serial_handle_t handle, const char* buf, size_t count)
{
	DWORD bytesWritten = 0;
	OVERLAPPED ov = {};
	ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!ov.hEvent) return -1;

	if (!WriteFile(handle, buf, static_cast<DWORD>(count), &bytesWritten, &ov)) {
		if (GetLastError() == ERROR_IO_PENDING) {
			if (WaitForSingleObject(ov.hEvent, INFINITE) == WAIT_OBJECT_0) {
				if (!GetOverlappedResult(handle, &ov, &bytesWritten, FALSE)) {
					bytesWritten = 0;
				}
			}
		} else {
			bytesWritten = 0;
		}
	}
	CloseHandle(ov.hEvent);
	return bytesWritten > 0 ? static_cast<ptrdiff_t>(bytesWritten) : -1;
}

bool serial_set_params(serial_handle_t handle, const SerialParams& params)
{
	DCB dcb = {};
	dcb.DCBlength = sizeof(dcb);
	if (!GetCommState(handle, &dcb)) return false;

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
	}

	return SetCommState(handle, &dcb) != 0;
}

bool serial_set_dtr(serial_handle_t handle, bool on)
{
	return EscapeCommFunction(handle, on ? SETDTR : CLRDTR) != 0;
}

bool serial_set_rts(serial_handle_t handle, bool on)
{
	return EscapeCommFunction(handle, on ? SETRTS : CLRRTS) != 0;
}

bool serial_get_modem_status(serial_handle_t handle, bool& cts, bool& dsr, bool& dcd, bool& ri)
{
	DWORD status;
	if (!GetCommModemStatus(handle, &status)) return false;
	cts = (status & MS_CTS_ON) != 0;
	dsr = (status & MS_DSR_ON) != 0;
	dcd = (status & MS_RLSD_ON) != 0;
	ri  = (status & MS_RING_ON) != 0;
	return true;
}

std::vector<std::string> serial_list_ports()
{
	std::vector<std::string> ports;
	for (int i = 1; i <= 256; ++i) {
		std::string name = "COM" + std::to_string(i);
		std::string fullName = "\\\\.\\" + name;
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

static constexpr int BAUDRATE_MAP[][2] = {
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
};

#ifdef __linux__
#ifndef BOTHER
#define BOTHER 0010000
#endif
#endif

static speed_t baudToSpeedExact(unsigned baud)
{
	for (auto [rate, speed] : BAUDRATE_MAP) {
		if (rate == static_cast<int>(baud)) return static_cast<speed_t>(speed);
	}
	return B0;
}

static unsigned baudToNearest(unsigned baud)
{
	unsigned bestRate = 0;
	unsigned bestDiff = 0;
	bool first = true;
	for (auto [rate, speed] : BAUDRATE_MAP) {
		unsigned diff = abs(rate - static_cast<int>(baud));
		if (first || diff < bestDiff) {
			bestRate = static_cast<unsigned>(rate);
			bestDiff = diff;
			first = false;
			if (diff == 0) break;
		}
	}
	return first ? 115200 : bestRate;
}

static void set_termios_baud(struct termios& tio, unsigned baud)
{
	speed_t speed = baudToSpeedExact(baud);
	if (speed != B0) {
		cfsetispeed(&tio, speed);
		cfsetospeed(&tio, speed);
	} else {
#ifdef __linux__
		cfsetispeed(&tio, BOTHER);
		cfsetospeed(&tio, BOTHER);
		tio.c_ispeed = static_cast<speed_t>(baud);
		tio.c_ospeed = static_cast<speed_t>(baud);
#elif defined(__APPLE__)
		unsigned nearest = baudToNearest(baud);
		speed = baudToSpeedExact(nearest);
		cfsetispeed(&tio, speed);
		cfsetospeed(&tio, speed);
#else
		unsigned nearest = baudToNearest(baud);
		speed = baudToSpeedExact(nearest);
		cfsetispeed(&tio, speed);
		cfsetospeed(&tio, speed);
#endif
	}
}

static bool apply_custom_baud_post(int fd, unsigned baud)
{
	if (baudToSpeedExact(baud) != B0) return true;
#ifdef __APPLE__
	int speed = static_cast<int>(baud);
	return ioctl(fd, IOSSIOSPEED, &speed) >= 0;
#else
	return true;
#endif
}

bool serial_open(serial_handle_t& handle, std::string_view portName, const SerialParams& params)
{
	auto name = std::string(portName);
	handle = open(name.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (handle < 0) return false;

	struct termios tio = {};
	if (tcgetattr(handle, &tio) < 0) {
		serial_close(handle);
		return false;
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
	if (tcsetattr(handle, TCSANOW, &tio) < 0) {
		serial_close(handle);
		return false;
	}
	if (!apply_custom_baud_post(handle, params.baudRate)) {
		serial_close(handle);
		return false;
	}

	// Clear non-blocking for regular I/O
	int flags = fcntl(handle, F_GETFL, 0);
	if (flags >= 0) {
		fcntl(handle, F_SETFL, flags & ~O_NONBLOCK);
	}

	tcflush(handle, TCIOFLUSH);

	return true;
}

void serial_close(serial_handle_t& handle)
{
	if (handle >= 0) {
		::close(handle);
		handle = -1;
	}
}

ptrdiff_t serial_read(serial_handle_t handle, char* buf, size_t count)
{
	auto n = ::read(handle, buf, count);
	if (n > 0) return n;
	if (n == 0) return 0;
	return -1;
}

ptrdiff_t serial_write(serial_handle_t handle, const char* buf, size_t count)
{
	auto n = ::write(handle, buf, count);
	if (n > 0) return n;
	return -1;
}

bool serial_set_params(serial_handle_t handle, const SerialParams& params)
{
	struct termios tio = {};
	if (tcgetattr(handle, &tio) < 0) return false;

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
	if (tcsetattr(handle, TCSANOW, &tio) < 0) return false;
	return apply_custom_baud_post(handle, params.baudRate);
}

bool serial_set_dtr(serial_handle_t handle, bool on)
{
	int status = TIOCM_DTR;
	if (on) {
		return ioctl(handle, TIOCMBIS, &status) >= 0;
	} else {
		return ioctl(handle, TIOCMBIC, &status) >= 0;
	}
}

bool serial_set_rts(serial_handle_t handle, bool on)
{
	int status = TIOCM_RTS;
	if (on) {
		return ioctl(handle, TIOCMBIS, &status) >= 0;
	} else {
		return ioctl(handle, TIOCMBIC, &status) >= 0;
	}
}

bool serial_get_modem_status(serial_handle_t handle, bool& cts, bool& dsr, bool& dcd, bool& ri)
{
	int status;
	if (ioctl(handle, TIOCMGET, &status) < 0) return false;
	cts = (status & TIOCM_CTS) != 0;
	dsr = (status & TIOCM_DSR) != 0;
	dcd = (status & TIOCM_CD) != 0;
	ri  = (status & TIOCM_RI) != 0;
	return true;
}

std::vector<std::string> serial_list_ports()
{
	std::vector<std::string> ports;
	DIR* dir = opendir("/dev");
	if (!dir) return ports;

	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr) {
		const char* prefixes[] = {"ttyS", "ttyUSB", "ttyACM", "ttyAMA", 
		                          "tty.usb", "tty.USA", "cu.usb", "cu.USA",
		                          "ttyXRUSB", "ttyO", "ttyCH"}; // OMAP, CH340/341/343
		for (const auto& pfx : prefixes) {
			if (strncmp(entry->d_name, pfx, strlen(pfx)) == 0) {
				ports.push_back("/dev/" + std::string(entry->d_name));
				break;
			}
		}
	}
	closedir(dir);
	std::ranges::sort(ports);
	return ports;
}

#endif

} // namespace openmsx
