# $Id$

include build/node-start.mk

SRC_HDR:= \
	MSXFDC \
	WD2793BasedFDC \
	PhilipsFDC \
	NationalFDC \
	MicrosolFDC \
	WD2793 \
	TurboRFDC \
	TC8566AF \
	DiskImageCLI \
	DiskDrive \
	RealDrive \
	DiskChanger \
	DiskFactory \
	DriveMultiplexer \
	Disk \
	DiskName \
	SectorBasedDisk \
	DummyDisk \
	DSKDiskImage \
	XSADiskImage \
	DirAsDSK \
	EmptyDiskPatch \
	RamDSKDiskImage \
	DiskPartition \
	MSXtar \
	DiskImageUtils \
	DiskContainer \
	SectorAccessibleDisk \
	DiskManipulator \
	BootBlocks \
	NowindInterface NowindHost NowindRomDisk NowindCommand

HDR_ONLY:= \
	DiskExceptions \

include build/node-end.mk

