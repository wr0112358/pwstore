# source this file with bash

function __pwstore_complete_global()
{
    local cur=${COMP_WORDS[COMP_CWORD]}
#    COMPREPLY[0]="called func global with arg: $1\n"
#    COMPREPLY[1]="222\n"
    if [[ "$cur" != -?* ]] && [[ "$cur" == -* ]];then
	COMPREPLY=( $( compgen -W "-f -o" $cur ))
    else
	COMPREPLY=( $( compgen -W "add dump lookup get remove change_passwd gen_passwd" $cur ))
    fi
}

function __pwstore_complete_add()
{
    local cur=${COMP_WORDS[COMP_CWORD]}
    if [[ "$cur" != -?* ]] && [[ "$cur" == -* ]];then
	COMPREPLY=( $( compgen -W "-i" $cur ))
#    else
#	COMPREPLY=( $( compgen -W "-i\ \ \ \ interactive"))
    fi
}

function __pwstore_complete_dump()
{
    local cur=${COMP_WORDS[COMP_CWORD]}
}

function __pwstore_complete_lookup()
{
    local cur=${COMP_WORDS[COMP_CWORD]}
    if [[ "$cur" != -?* ]] && [[ "$cur" == -* ]];then
	COMPREPLY=( $( compgen -W "-i -o -n" $cur ))
    fi
}

function __pwstore_complete_get()
{
    local cur=${COMP_WORDS[COMP_CWORD]}
    if [[ "$cur" != -?* ]] && [[ "$cur" == -* ]];then
	COMPREPLY=( $( compgen -W "-o -n" $cur ))
#    else if [[ "$cur" != "get" ]]
#	# -n must be specified at least once
#	COMPREPLY=( $( compgen -W "-n"))
    fi
}

function __pwstore_complete_remove()
{
    local cur=${COMP_WORDS[COMP_CWORD]}
    if [[ "$cur" != -?* ]] && [[ "$cur" == -* ]];then
	COMPREPLY=( $( compgen -W "-n --force" $cur ))
#    else
#	# -n must be specified at least once
#	COMPREPLY=( $( compgen -W "-n"))
    fi
}

function __pwstore_complete_change_passwd()
{
    local cur=${COMP_WORDS[COMP_CWORD]}
}

function __pwstore_complete_gen_passwd()
{
    local cur=${COMP_WORDS[COMP_CWORD]}
}

function __pwstore_complete()
{
    local handle_cmd="__pwstore_complete_global"

    for((i=0; i < ${#COMP_WORDS[@]}; i++)); do
	case ${COMP_WORDS[i]} in
	    "add")
		handle_cmd="__pwstore_complete_add"
		;;
	    "dump")
		handle_cmd="__pwstore_complete_dump"
		;;
	    "lookup")
		handle_cmd="__pwstore_complete_lookup"
		;;
	    "get")
		handle_cmd="__pwstore_complete_get"
		;;
	    "remove")
		handle_cmd="__pwstore_complete_remove"
		;;
	    "change_passwd")
		handle_cmd="__pwstore_complete_change_passwd"
		;;
	    "gen_passwd")
		handle_cmd="__pwstore_complete_gen_passwd"
		;;
	    esac
    done

    $handle_cmd
}

complete -F __pwstore_complete pwstore
