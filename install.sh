#!/bin/sh

# Set up a Unix account the way I like it.
# Requires this `local' directory checked out, and also `utils'.

SCRIPTS="addcr cal detab findgrep mono mtribs newscript nsort remcr rot13 svnlog timber"

test -r $HOME/bin || mkdir $HOME/bin

cd $HOME/src/utils
make -s install-progs PREFIX=$HOME

cd $HOME/src/local

LNS=$HOME/bin/lns

for i in $SCRIPTS; do
  test -r $HOME/bin/$i || $LNS $i $HOME/bin/$i
done

test -r $HOME/.jedrc      || $LNS jed.rc $HOME/.jedrc
test -r $HOME/.screenrc   || $LNS screen.rc $HOME/.screenrc
test -r $HOME/.sigs       || $LNS signature $HOME/.sigs
test -d $HOME/.ssh        || (umask 077; mkdir $HOME/.ssh)
if test -r $HOME/.ssh/config; then
  if grep -q '^CheckHostIP' $HOME/.ssh/config; then
    : # do nothing
  else
    (echo 'CheckHostIP no'; cat $HOME/.ssh/config) > $HOME/.ssh/config.new && \
      mv $HOME/.ssh/config.new $HOME/.ssh/config
  fi
else
  echo 'CheckHostIP no' > $HOME/.ssh/config
fi

test -r $HOME/.bashrc || {
  echo 'source ~/src/local/bash.rc'
} > $HOME/.bashrc

test -r $HOME/.bash_profile || {
  echo '# even login shells want .bashrc to be read'
  echo 'source ~/.bashrc'
} > $HOME/.bash_profile
