#ifdef _WIN32

#ifndef SOCKETSETTINGSMANAGER_HH
#define SOCKETSETTINGSMANAGER_HH

#include "Subject.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"

namespace openmsx {

enum SocketAuthenticationMode { SOCKAUTH_NONE, SOCKAUTH_SSPI };
enum SocketSelectionMode { SOCKSEL_RANDOM, SOCKSEL_FIXED };

class CommandController;

/**
* Manages the Windows socket connections setting.
*/
class SocketSettingsManager final : public Subject<SocketSettingsManager>
                                  , private Observer<Setting>
{
	public:
		explicit SocketSettingsManager(CommandController& commandController);
		~SocketSettingsManager();

		[[nodiscard]] SocketAuthenticationMode getSocketAuthenticationMode() const { return socketAuthenticationModeSetting.getEnum(); }
		[[nodiscard]] SocketSelectionMode getSocketSelectionMode() const { return socketSelectionModeSetting.getEnum(); }
		[[nodiscard]] int getConfiguredSocketPortNumber() const { return socketPortNumberSetting.getInt(); }
		[[nodiscard]] int getCurrentSocketPortNumber() const { return currentSocketPortNumber; }
		[[nodiscard]] void setCurrentSocketPortNumber(int port) { currentSocketPortNumber = port; }
		void setSocketAuthenticationMode(const SocketAuthenticationMode mode);

	private:
		const int DEFAULT_SOCKET_PORT_NUMBER = 9938;

		EnumSetting<SocketAuthenticationMode> socketAuthenticationModeSetting;
		EnumSetting<SocketSelectionMode> socketSelectionModeSetting;
		IntegerSetting socketPortNumberSetting;
		int currentSocketPortNumber;

		// Observer<Setting>
		void update(const Setting& setting) noexcept override;
	};

} // namespace openmsx

#endif

#endif //_WIN32

