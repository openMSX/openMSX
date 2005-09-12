// $Id$

#ifndef DISKCHANGER_HH
#define DISKCHANGER_HH

#include "Command.hh"
#include "DiskContainer.hh"
#include "Disk.hh"
#include <memory>

namespace openmsx {

class XMLElement;
class CommandController;
class FileManipulator;

class DiskChanger : private Command, public DiskContainer
{
public:
	DiskChanger(const std::string& driveName,
	            CommandController& commandController,
	            FileManipulator& manipulator);
	~DiskChanger();

	const std::string& getDriveName() const;
	const std::string& getDiskName() const;
	bool diskChanged();
	bool peekDiskChanged() const;
	Disk& getDisk();

	// DiskContainer
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();

private:
	void insertDisk(const std::string& disk,
	                const std::vector<std::string>& patches);
	void ejectDisk();

	// Command interface
	virtual void execute(const std::vector<TclObject*>& tokens,
	                     TclObject& result);
	virtual std::string help   (const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	std::string driveName;
	FileManipulator& manipulator;
	std::auto_ptr<Disk> disk;
	XMLElement* diskElem;
	bool diskChangedFlag;
};

} // namespace openmsx

#endif
