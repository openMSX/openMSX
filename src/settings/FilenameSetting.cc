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
	const string& initialValue, XMLElement* node)
	: StringSettingBase(name, description, initialValue, node)
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
	const string& initialValue, XMLElement* node)
	: FilenameSettingBase(name, description, initialValue, node)
{
	initSetting();
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
