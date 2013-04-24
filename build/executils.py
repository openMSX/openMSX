from msysutils import msysActive, msysShell

from os import environ
from shlex import split as shsplit
from subprocess import PIPE, Popen

def captureStdout(log, commandLine):
	'''Run a command and capture what it writes to stdout.
	If the command fails or writes something to stderr, that is logged.
	Returns the captured string, or None if the command failed.
	'''
	# TODO: This is a modified copy-paste from compilers._Command.
	commandParts = shsplit(commandLine)
	env = dict(environ)
	while commandParts:
		if '=' in commandParts[0]:
			name, value = commandParts[0].split('=', 1)
			del commandParts[0]
			env[name] = value
		else:
			break
	else:
		raise ValueError(
			'No command specified in "%s"' % commandLine
			)

	if msysActive() and commandParts[0] != 'sh':
		commandParts = [
			environ.get('MSYSCON') or environ.get('SHELL') or 'sh.exe',
			'-c', shjoin(commandParts)
			]

	try:
		proc = Popen(
			commandParts, bufsize = -1, env = env,
			stdin = None, stdout = PIPE, stderr = PIPE,
			)
	except OSError, ex:
		print >> log, 'Failed to execute "%s": %s' % (commandLine, ex)
		return None
	stdoutdata, stderrdata = proc.communicate()
	if stderrdata:
		severity = 'warning' if proc.returncode == 0 else 'error'
		log.write('%s executing "%s"\n' % (severity.capitalize(), commandLine))
		# pylint 0.18.0 somehow thinks stderrdata is a list, not a string.
		# pylint: disable-msg=E1103
		stderrdata = stderrdata.replace('\r', '')
		log.write(stderrdata)
		if not stderrdata.endswith('\n'):
			log.write('\n')
	if proc.returncode == 0:
		return stdoutdata
	else:
		print >> log, 'Execution failed with exit code %d' % proc.returncode
		return None

def shjoin(parts):
	'''Joins the given sequence into a single string with space as a separator.
	Characters that have a special meaning for the shell are escaped.
	This is the counterpart of shlex.split().
	'''
	def escape(part):
		return ''.join(
			'\\' + ch if ch in '\\ \'"$()[]' else ch
			for ch in part
			)
	return ' '.join(escape(part) for part in parts)
