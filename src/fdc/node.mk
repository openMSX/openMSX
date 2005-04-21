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
	DriveMultiplexer \
	Disk \
	SectorBasedDisk \
	DummyDisk \
	DSKDiskImage \
	XSADiskImage \
	CRC16 \
	FDC_DirAsDSK \
	EmptyDiskPatch \
	RamDSKDiskImage \
	MSXtar \
	SectorAccessibleDisk \
	FileManipulator

include build/node-end.mk

