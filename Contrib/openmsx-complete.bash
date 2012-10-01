# This file enables openmsx specific tab-completion in the bash shell.
#
# To enable it system-wide, copy and rename this file to
#    /etc/bash_completion.d/openmsx
# To enable it only for the current user, source this file from e.g. your
#    ~/.bashrc
# file.
#
# This needs an openMSX version newer than openmsx-0.9.1 (TODO replace with
# actual version number once that number is known).

_openmsx_complete()
{
	local cur tmp
	cur=${COMP_WORDS[COMP_CWORD]}
	tmp=$(openmsx -bash $3)
	COMPREPLY=($(compgen -W '$tmp' -- "$cur"))
}
complete -f -F _openmsx_complete openmsx
