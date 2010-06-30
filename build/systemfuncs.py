# $Id$

class SystemFunction(object):
	name = None

	@classmethod
	def getFunctionName(cls):
		return cls.name

	@classmethod
	def getMakeName(cls):
		return cls.name.upper()

	@classmethod
	def iterHeaders(cls, targetPlatform):
		raise NotImplementedError

class FTruncateFunction(SystemFunction):
	name = 'ftruncate'

	@classmethod
	def iterHeaders(cls, targetPlatform):
		yield '<unistd.h>'

class ClockGetTimeFunction(SystemFunction):
	name = 'clock_gettime'

	@classmethod
	def iterHeaders(cls, targetPlatform):
		yield '<time.h>'

class MMapFunction(SystemFunction):
	name = 'mmap'

	@classmethod
	def iterHeaders(cls, targetPlatform):
		if targetPlatform in ('darwin', 'openbsd'):
			yield '<sys/types.h>'
		yield '<sys/mman.h>'

class PosixMemAlignFunction(SystemFunction):
	name = 'posix_memalign'

	@classmethod
	def iterHeaders(cls, targetPlatform):
		yield '<stdlib.h>'

class USleepFunction(SystemFunction):
	name = 'usleep'

	@classmethod
	def iterHeaders(cls, targetPlatform):
		yield '<unistd.h>'

# Build a list of system functions using introspection.
def _discoverSystemFunctions(localObjects):
	for obj in localObjects:
		if isinstance(obj, type) and issubclass(obj, SystemFunction):
			if obj is not SystemFunction:
				yield obj
systemFunctions = list(_discoverSystemFunctions(locals().itervalues()))
