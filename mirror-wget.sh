#!/bin/sh 

# When run, this script will generate a mirror of the PuTTY website
# in a subdirectory called `putty'.

# Fetch the main web site.
wget --no-parent -l0 -r --cut-dirs=1 -nH \
	http://www.chiark.greenend.org.uk/~sgtatham/putty/

# Fetch the .htaccess file from the download site, in order to find the
# version number of the latest release.
cd putty
wget http://the.earth.li/~sgtatham/putty/htaccess
version=`sed -n 's!.*http://the.earth.li/~sgtatham/putty/\(.*\)/!\1!p' htaccess`
echo Latest version is $version

# Retrieve the binaries directory for that version from the binaries
# server.
wget --no-parent -l0 -r --cut-dirs=3 -P $version -nH \
	http://the.earth.li/~sgtatham/putty/$version/

# Fix the hyperlinks in the download and docs pages so that they point
# to the local mirror of the binaries instead of the primary binaries
# server. Without this there's barely any point in doing the mirror at
# all.
perl -i~ -pe 's!http://the.earth.li/~sgtatham/putty/!!' download.html docs.html

# Create a .htaccess file that supplies the redirection from
# "latest" to a numbered version directory. This RedirectMatch
# command ought to do that job independently of the 
echo "RedirectMatch temp ^(.*)/latest(/[^/]*/?[^/]*)?$ \$1/$version\$2" > .htaccess

# The .htaccess file should also ensure the Windows Help files are
# not given any strange MIME types by the web server. Also it
# should make sure the GPG signatures have the correct MIME type.
echo "AddType application/octet-stream .hlp" >> .htaccess
echo "AddType application/octet-stream .cnt" >> .htaccess
echo "AddType application/octet-stream .RSA" >> .htaccess
echo "AddType application/octet-stream .DSA" >> .htaccess
