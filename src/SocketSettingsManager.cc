#ifdef _WIN32

#include "SocketSettingsManager.hh"

namespace openmsx {

// class SocketSettingsManager:

SocketSettingsManager::SocketSettingsManager(CommandController& commandController)
	: socketAuthenticationModeSetting(commandController, "socket_auth_mode",
	       "authentication mode for socket connections, one of 'none' or 'sspi'",
	       SOCKAUTH_SSPI,
	       EnumSetting<SocketAuthenticationMode>::Map{
	           { "none", SOCKAUTH_NONE },
	           { "sspi", SOCKAUTH_SSPI }})
	, socketPortNumberSetting(commandController, "socket_port_number",
		   "port number (or base port number, see 'set socket_selection_mode') for socket connections",
		   DEFAULT_SOCKET_PORT_NUMBER, 1, 65535)
    , socketSelectionModeSetting(commandController, "socket_selection_mode",
           "selection mode for the TCP port for socket connections, one of 'random' or 'fixed'",
		   SOCKSEL_RANDOM,
		   EnumSetting<SocketAuthenticationMode>::Map{
			   { "random", SOCKSEL_RANDOM },
		       { "fixed", SOCKSEL_FIXED }})
{
	setCurrentSocketPortNumber(0);

	socketPortNumberSetting.attach(*this);
	socketSelectionModeSetting.attach(*this);

	//No need to attach socketAuthenticationModeSetting, that setting is explicitly checked
	//whenever a client attempts to connect.
}

SocketSettingsManager::~SocketSettingsManager()
{
	socketPortNumberSetting.detach(*this);
	socketSelectionModeSetting.detach(*this);
}

void SocketSettingsManager::setSocketAuthenticationMode(const SocketAuthenticationMode mode)
{
	socketAuthenticationModeSetting.setEnum(mode);
}

void SocketSettingsManager::update(const Setting& /*setting*/) noexcept
{
	notify();
}

} // namespace openmsx

#endif //_WIN32
