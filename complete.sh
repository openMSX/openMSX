_openmsx_complete()
{
	local cur tmp
	cur=${COMP_WORDS[COMP_CWORD]}
	tmp=$(openmsx -bash $3)
	COMPREPLY=($(compgen -W '$tmp' -- "$cur"))
}
complete -f -F _openmsx_complete openmsx
