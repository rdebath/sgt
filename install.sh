#!/bin/sh

# Set up a Unix account the way I like it.

SCRIPTS="addcr findgrep lns mff mfinger mono mtribs multi newscript"
SCRIPTS="$SCRIPTS remcr rot13 timber"

cd $HOME/src/local

test -e $HOME/bin || mkdir $HOME/bin
./lns $SCRIPTS $HOME/bin

test -e $HOME/.jedrc    || ./lns jed.rc $HOME/.jedrc
test -e $HOME/.screenrc || ./lns screen.rc $HOME/.screenrc
test -e $HOME/.sigs     || ./lns signature $HOME/.sigs

test -e $HOME/.bashrc || {
  echo 'source ~/src/local/bash.rc'
} > $HOME/.bashrc

test -e $HOME/.bash_profile || {
  echo '# even login shells want .bashrc to be read'
  echo 'source ~/.bashrc'
} > $HOME/.bash_profile
