// $Id$

#include "FilenameSetting.hh"
#include "File.hh"
#include "FileContext.hh"
#include "CliCommOutput.hh"
#include "CommandController.hh"
#include "SettingsManager.hh"

namespace openmsx {

// class FilenameSettingBase

FilenameSettingBase::FilenameSettingBase(
	const string& name, const string& description,
	const string& initialValue)
	: StringSettingBase(name, description, initialValue)
{
}

void FilenameSettingBase::setValue(const string& newValue)
{
	try {
		UserFileContext context;
		string resolved = newValue.empty() ? newValue
		                                   : context.resolve(newValue);
		if (checkFile(resolved)) {
			StringSettingBase::setValue(newValue);
		}
	} catch (FileException& e) {
		// File not found.
		if (!newValue.empty()) {
			CliCommOutput::instance().printWarning(
				"couldn't find file: \"" + newValue + "\"");
		}
	}
}

void FilenameSettingBase::tabCompletion(vector<string>& tokens) const
{
	CommandController::completeFileName(tokens);
}


// class FilenameSetting

FilenameSetting::FilenameSetting(
	const string& name, const string& description,
	const string& initialValue)
	: FilenameSettingBase(name, description, initialValue)
{
	SettingsManager::instance().registerSetting(*this);
}

FilenameSetting::~FilenameSetting()
{
	SettingsManager::instance().unregisterSetting(*this);
}

bool FilenameSetting::checkFile(const string& /*filename*/)
{
	return true;
}

} // namespace openmsx
