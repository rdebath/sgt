#!/bin/sh

# Set up a Unix account the way I like it.

SCRIPTS="addcr findgrep lns mff mfinger mono mtribs multi newscript"
SCRIPTS="$SCRIPTS remcr rot13 timber"

cd $HOME/src/local

test -r $HOME/bin || mkdir $HOME/bin
for i in $SCRIPTS; do
  test -r $HOME/bin/$i || ./lns $i $HOME/bin/$i
done

test -r $HOME/.jedrc    || ./lns jed.rc $HOME/.jedrc
test -r $HOME/.screenrc || ./lns screen.rc $HOME/.screenrc
test -r $HOME/.sigs     || ./lns signature $HOME/.sigs

test -r $HOME/.bashrc || {
  echo 'source ~/src/local/bash.rc'
} > $HOME/.bashrc

test -r $HOME/.bash_profile || {
  echo '# even login shells want .bashrc to be read'
  echo 'source ~/.bashrc'
} > $HOME/.bash_profile
