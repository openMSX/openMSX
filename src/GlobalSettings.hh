// $Id$

#ifndef GLOBALSETTINGS_HH
#define GLOBALSETTINGS_HH

#include "Observer.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class CommandController;
class IntegerSetting;
class BooleanSetting;
class StringSetting;
class ThrottleManager;
class Setting;
template <class T> class EnumSetting;

/**
 * This class contains settings that are used by several other class
 * (including some singletons). This class was introduced to solve
 * lifetime management issues.
 */
class GlobalSettings : private Observer<Setting>, private noncopyable
{
public:
	enum ResampleType { RESAMPLE_HQ, RESAMPLE_LQ, RESAMPLE_BLIP };
	typedef enum SyncMode { SYNC_READONLY, SYNC_CACHEDWRITE, SYNC_NODELETE, SYNC_FULL } SyncMode_t ;

	explicit GlobalSettings(CommandController& commandController);
	~GlobalSettings();

	IntegerSetting& getSpeedSetting();
	BooleanSetting& getPauseSetting();
	BooleanSetting& getPowerSetting();
	BooleanSetting& getAutoSaveSetting();
	BooleanSetting& getConsoleSetting();
	StringSetting&  getUserDirSetting();
	StringSetting&  getUMRCallBackSetting();
	EnumSetting<bool>& getBootSectorSetting();
	EnumSetting<SyncMode>& getSyncDirAsDSKSetting();
	EnumSetting<ResampleType>& getResampleSetting();
	ThrottleManager& getThrottleManager();

private:
	// Observer<Setting>
	virtual void update(const Setting& setting);

	CommandController& commandController;

	std::auto_ptr<IntegerSetting> speedSetting;
	std::auto_ptr<BooleanSetting> pauseSetting;
	std::auto_ptr<BooleanSetting> powerSetting;
	std::auto_ptr<BooleanSetting> autoSaveSetting;
	std::auto_ptr<BooleanSetting> consoleSetting;
	std::auto_ptr<StringSetting>  userDirSetting;
	std::auto_ptr<StringSetting>  umrCallBackSetting;
	std::auto_ptr<EnumSetting<bool> > bootSectorSetting;
	std::auto_ptr<EnumSetting<SyncMode> > syncDirAsDSKSetting;
	std::auto_ptr<EnumSetting<ResampleType> > resampleSetting;
	std::auto_ptr<ThrottleManager> throttleManager;
};

} // namespace openmsx

#endif
