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
	DriveMultiplexer \
	Disk \
	SectorBasedDisk \
	DummyDisk \
	DSKDiskImage \
	XSADiskImage \
	DirAsDSK \
	EmptyDiskPatch \
	RamDSKDiskImage \
	MSXtar \
	DiskImageUtils \
	DiskContainer \
	SectorAccessibleDisk \
	DiskManipulator \
	BootBlocks \
	NowindInterface NowindHost

HDR_ONLY:= \
	DiskExceptions \
	WriteProtectableDisk

include build/node-end.mk

