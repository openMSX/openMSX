#include "SocketSettingsManager.hh"

namespace openmsx {

// class SocketSettingsManager:

SocketSettingsManager::SocketSettingsManager(CommandController& commandController)
	: socketAuthenticationModeSetting(commandController, "socket_auth_mode",
	       "authentication mode for socket connections, one of 'none' or 'sspi'",
	       SOCKAUTH_SSPI,
	       EnumSetting<SocketAuthenticationMode>::Map{
	           { "none", SOCKAUTH_NONE },
	           { "sspi", SOCKAUTH_SSPI }
})
{
	setCurrentSocketPortNumber(0);
}

void SocketSettingsManager::setSocketAuthenticationMode(const SocketAuthenticationMode mode)
{
	socketAuthenticationModeSetting.setEnum(mode);
}

} // namespace openmsx
