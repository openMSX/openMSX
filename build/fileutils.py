# $Id$

from os import altsep, chmod, mkdir, remove, sep, stat, walk
from os.path import (
	dirname, exists, isdir, isfile, islink, join as joinpath, normpath
	)
from shutil import copyfile

try:
	from os import symlink
except ImportError:
	def symlink(src, dst):
		raise OSError('This platform does not support symbolic links')

def scanTree(baseDir, selection = None):
	'''Scans files and directories from the given base directory and iterates
	through the paths of the files and directories it finds; these paths are
	relative to the base directory.
	If no selection is given all files and directories inside the source
	directory are selected, otherwise the selection is the sequence of names of
	files and directories inside the source directory that will be included.
	Directories will always be returned before any of the files they contain.
	The base directory itself is not returned.
	Hidden files and directories are not returned.
	All paths returned use the OS native separator character (os.sep),
	regardless of which separator characters were used in the arguments.
	Raises IOError if there is an I/O error scanning the base directory.
	Raises ValueError if the selection is invalid.
	'''
	if altsep is not None:
		# Make sure all paths use the OS native separator, so we can safely
		# compare paths and have consistent error messages.
		baseDir = baseDir.replace(altsep, sep)
		if selection is not None:
			selection = [ path.replace(altsep, sep) for path in selection ]

	if not isdir(baseDir):
		raise IOError('Directory "%s" does not exist' % baseDir)

	if selection is None:
		selectedDirs = None
		selectedFiles = None
	else:
		selectedDirs = set()
		selectedFiles = set()
		for selected in selection:
			normSel = normpath(selected)
			if normSel == '.':
				# Although strictly speaking there is no need to treat this as
				# a fatal problem, it is very likely that the caller made a
				# mistake.
				raise ValueError(
					'Selection element "%s" selects entire source directory'
					% selected
					)
			if normSel == '..' or normSel.startswith('..' + sep):
				raise ValueError(
					'Selection element "%s" reaches outside source directory'
					% selected
					)
			path = joinpath(baseDir, normSel)
			if isdir(path):
				selectedDirs.add(normSel)
			elif exists(path):
				selectedFiles.add(normSel)
			else:
				raise IOError('Selected path "%s" does not exist' % path)

	def escalate(ex):
		raise IOError(
			'Error scanning directory entry "%s": %s' % (ex.filename, ex)
			)
	for dirPath, dirNames, fileNames in walk(baseDir, onerror = escalate):
		if dirPath == baseDir and selection is not None:
			# For the top-level directory, use selection.
			dirNames[ : ] = sorted(selectedDirs)
			fileNames = sorted(selectedFiles)

		# Skip hidden directories.
		# We are deleting items from the list we are iterating over; that
		# requires some extra care.
		index = 0
		while index < len(dirNames):
			if dirNames[index].startswith('.'):
				del dirNames[index]
			else:
				index += 1

		# Skip hidden files.
		fileNames = [ name for name in fileNames if not name.startswith('.') ]

		if dirPath == baseDir:
			relDir = ''
		else:
			assert dirPath.startswith(baseDir + sep)
			relDir = dirPath[len(baseDir) + 1 : ]
			yield relDir
		for fileName in fileNames:
			yield joinpath(relDir, fileName)

def installDir(path):
	'''Creates the given path, excluding parent directories.
	The directory is created with permissions such that all users can read what
	is installed and only the owner can modify what is installed.
	If the given path already exists, nothing happens.
	'''
	if not isdir(path):
		# We have to do chmod() separately because the "mode" argument of
		# mkdir() is modified by umask.
		mkdir(path)
		chmod(path, 0755)

def _installDirsRec(path):
	'''Like installDirs(), except that "altsep" is not supported as directory
	separator in "path'.
	'''
	if not isdir(path):
		parent = path[ : path.rindex(sep)]
		_installDirsRec(parent)
		mkdir(path)
		chmod(path, 0755)

def installDirs(path):
	'''Creates the given path, including any parent directories if necessary.
	Any newly created directories are created with permissions such that all
	users can read what is installed and only the owner can modify what is
	installed.
	If the given path already exists, nothing happens.
	'''
	if altsep is not None:
		path = path.replace(altsep, sep)
	_installDirsRec(path)

def installFile(srcPath, destPath):
	'''Copies a file from the given source path to the given destination path.
	The destination file is created with permissions such that all users can
	read (and execute, if appropriate) what is installed and only the owner
	can modify what is installed.
	Raises IOError if there is a problem reading or writing files.
	'''
	copyfile(srcPath, destPath)
	chmod(destPath, 0755 if (stat(srcPath).st_mode & 0100) else 0644)

def installSymlink(target, link):
	'''Creates a symbolic link with the given name to the given target.
	If a symbolic link with the given name already exists, it is replaced.
	If a different file type with the given name already exists, OSError is
	raised.
	Also raises OSError if this platform lacks os.symlink().
	'''
	if islink(link):
		remove(link)
	symlink(target, link)

def installTree(srcDir, destDir, paths):
	'''Copies files and directories from the given source directory to the
	given destination directory. The given paths argument is a sequence of
	paths relative to the source directory; only those files and directories
	are copied. The scanTree() function is suitable to provide paths.
	Files and directories are created with permissions such that all users can
	read (and execute, if appropriate) what is installed and only the owner
	can modify what is installed.
	Raises IOError if there is a problem reading or writing files or
	directories.
	'''
	if not isdir(destDir):
		raise IOError('Destination directory "%s" does not exist' % destDir)

	for relPath in paths:
		if altsep is not None:
			relPath = relPath.replace(altsep, sep)
		srcPath = joinpath(srcDir, relPath)
		destPath = joinpath(destDir, relPath)
		if islink(srcPath):
			print 'Skipping symbolic link:', srcPath
		elif isdir(srcPath):
			installDir(destPath)
		elif isfile(srcPath):
			_installDirsRec(dirname(destPath))
			installFile(srcPath, destPath)
		else:
			print 'Skipping unknown file type:', srcPath
