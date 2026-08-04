#ifndef SERIALPORT_HH
#define SERIALPORT_HH

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>
#else
#include <windows.h>
#endif

namespace openmsx {

#ifdef _WIN32
using serial_handle_t = HANDLE;
inline const serial_handle_t OPENMSX_INVALID_SERIAL_HANDLE = INVALID_HANDLE_VALUE;
#else
using serial_handle_t = int;
constexpr serial_handle_t OPENMSX_INVALID_SERIAL_HANDLE = -1;
#endif

struct SerialParams {
	unsigned baudRate = 115200;
	int dataBits = 8;
	int stopBits = 1; // 1, 2 (1.5 is not supported on most platforms)
	int parity = 0;   // 0=none, 1=odd, 2=even, 3=mark, 4=space
};

[[nodiscard]] std::string serial_error();
bool serial_open(serial_handle_t& handle, std::string_view portName, const SerialParams& params);
void serial_close(serial_handle_t& handle);
[[nodiscard]] ptrdiff_t serial_read(serial_handle_t handle, char* buf, size_t count);
[[nodiscard]] ptrdiff_t serial_write(serial_handle_t handle, const char* buf, size_t count);
bool serial_set_params(serial_handle_t handle, const SerialParams& params);
bool serial_set_dtr(serial_handle_t handle, bool on);
bool serial_set_rts(serial_handle_t handle, bool on);
bool serial_get_modem_status(serial_handle_t handle, bool& cts, bool& dsr, bool& dcd, bool& ri);
std::vector<std::string> serial_list_ports();

} // namespace openmsx

#endif
