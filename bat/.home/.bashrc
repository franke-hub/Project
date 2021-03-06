##############################################################################
##
##       Copyright (C) 2020 Frank Eskesen.
##
##       This file is free content, distributed under the "un-license,"
##       explicitly released into the Public Domain.
##       (See accompanying file LICENSE.UNLICENSE or the original
##       contained within http://unlicense.org)
##
##############################################################################
##
## Title-
##       .bashrc
##
## Function-
##       Shell customization script, sourced for each shell invocation
##
## Last change date-
##        2020/01/19
##
##############################################################################

##############################################################################
# Debugging hook
[ -n "$debugging" ] && date "+%s.%N HOST($HOST) USER($USER) begin .bashrc $$ $0" >>$debugging

##############################################################################
# If not running bash interactively, don't do anything
# CYGWIN: Ignore interactive check (??TEMPORARY??)
isCYGWIN=`uname | grep CYGWIN`
if [ -z "$isCYGWIN" ] ; then
  [ -z "$BASH_VERSION" ] && return
  [[ $- != *i* ]] && return
fi

##############################################################################
# Common initialization
# [ -r /etc/bashrc ] && source /etc/bashrc || ([ -r /etc/bash.bashrc ] && source /etc/bash.bashrc)

[ -r $HOME/bat/bash_common ] && source $HOME/bat/bash_common

export EXECIGNORE="*.dll"           ## Exclude *.dlls from TAB expansion

##############################################################################
# XTERM decoration
[ "${TERM:0:5}" == "xterm" -a -r $HOME/bat/setPROMPT ] && source $HOME/bat/setPROMPT

##############################################################################
# Debugging hook
[ -n "$debugging" ] && date "+%s.%N HOST($HOST) USER($USER) *end* .bashrc $$ $0" >>$debugging
