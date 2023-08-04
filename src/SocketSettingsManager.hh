#ifdef _WIN32

#ifndef SOCKETSETTINGSMANAGER_HH
#define SOCKETSETTINGSMANAGER_HH

#include "EnumSetting.hh"

namespace openmsx {

enum SocketAuthenticationMode { SOCKAUTH_NONE, SOCKAUTH_SSPI };

class CommandController;

/**
* Manages the Windows socket connections setting.
*/
class SocketSettingsManager final
{
	public:
		explicit SocketSettingsManager(CommandController& commandController);

		[[nodiscard]] SocketAuthenticationMode getSocketAuthenticationMode() const { return socketAuthenticationModeSetting.getEnum(); }
		[[nodiscard]] int getCurrentSocketPortNumber() const { return currentSocketPortNumber; }
		[[nodiscard]] void setCurrentSocketPortNumber(int port) { currentSocketPortNumber = port; }
		void setSocketAuthenticationMode(const SocketAuthenticationMode mode);

	private:
		EnumSetting<SocketAuthenticationMode> socketAuthenticationModeSetting;
		int currentSocketPortNumber;
	};

} // namespace openmsx

#endif

#endif //_WIN32

