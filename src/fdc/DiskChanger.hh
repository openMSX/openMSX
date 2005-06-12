// $Id$

#ifndef DISKCHANGER_HH
#define DISKCHANGER_HH

#include "Command.hh"
#include "DiskContainer.hh"
#include "Disk.hh"
#include <memory>

namespace openmsx {

class XMLElement;
class FileManipulator;

class DiskChanger : private SimpleCommand, public DiskContainer
{
public:
	DiskChanger(const std::string& driveName, FileManipulator& manipulator);
	~DiskChanger();

	const std::string& getDriveName() const;
	const std::string& getDiskName() const;
	bool diskChanged();
	Disk& getDisk();

	// DiskContainer
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();

private:
	void insertDisk(const std::string& disk,
	                const std::vector<std::string>& patches);
	void ejectDisk();

	// Command interface
	virtual std::string execute(const std::vector<std::string>& tokens);
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
