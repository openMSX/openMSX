// $Id$

#include "FilenameSetting.hh"
#include "File.hh"
#include "FileContext.hh"
#include "CliCommOutput.hh"
#include "CommandController.hh"

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
	string resolved;
	if (!newValue.empty() && xmlNode) {
		try {
			resolved = xmlNode->getFileContext().resolve(newValue);
		} catch (FileException& e) {
			// ignore
		}
	}
	if (!newValue.empty() && resolved.empty()) {
		try {
			UserFileContext context;
			resolved = context.resolve(newValue);
		} catch (FileException& e) {
			// File not found
			CliCommOutput::instance().printWarning(
				"couldn't find file: \"" + newValue + "\"");
			return;
		}
	}
	if (checkFile(resolved)) {
		StringSettingBase::setValue(newValue);
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
	initSetting(SAVE_SETTING);
}

FilenameSetting::~FilenameSetting()
{
	exitSetting();
}

bool FilenameSetting::checkFile(const string& /*filename*/)
{
	return true;
}

} // namespace openmsx
