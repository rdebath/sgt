This directory contains `newspasswd', an NNTP proxy.

OVERVIEW
--------

`newspasswd' has two functions:

 - To accept USER and PASS authentication locally, and translate
   them into some alternative form of authentication at the news
   server it is proxying.

 - To perform full-article rewriting on every article posted (so
   that, for example, you could implement per-newsgroup From lines
   and randomised sigs using a newsreader that didn't actually
   support those features).

INSTALLATION
------------

A prerequisite for installation is `userv'. If you don't already
have this, you should; it's useful and good and allows all sorts of
interesting Unix administration and user configurability.

To install:

 - Put the service files `newspasswd-rewrite' and
   `newspasswd-validate' in /etc/userv/services.d. If you don't have
   that directory or some other directory which is scanned for
   services by the primary userv configuration, create one, or munge
   these service descriptions into your systemwide userv config
   somehow.

 - Edit those service files. If you intend to run the news proxy
   itself under a gid other than `news', you need to modify the
   service files so that the services they provide will be accepted
   coming from that gid; also, if you intend to put the
   newspasswd-validator script anywhere other than
   /usr/local/lib/news then you will need to alter the pathname in
   the newspasswd-validate service file.

 - Install newspasswd-validator somewhere where all users can read
   and execute it, such as /usr/local/lib/news. (It's referenced by
   absolute pathname from the newspasswd-validate service file, so
   it doesn't need to be on the typical user's PATH.)

 - Install newspasswd-proxy somewhere where inetd can run it (so
   it'll need to be readable and executable); /usr/local/lib/news
   might be a good place, again. Edit it so that it mentions the
   right news server instead of the one I've hardcoded into it.

 - Install `newspasswd' itself somewhere on the typical user's PATH.

 - Add a line to inetd.conf that invokes newspasswd-proxy. Typically
   this would be on the default NNTP port:

      nntp stream tcp nowait news /usr/local/lib/news/newspasswd-proxy

   (Note the word `news' to ensure it starts up as a non-root user.)

 - Alternatively, you can run `newspasswd-proxy -daemon' which will
   do the listening loop itself rather than trusting inetd, but it
   won't detach like a proper daemon because I never got round to
   writing that bit.

OPERATION
---------

When you connect to the proxy, it immediately connects through to
the upstream server, and the greeting you see is the one from the
server. For the most part, the proxy will pass data back and forth
without changing anything.

If the upstream server requests authentication, the proxy will pass
that request on to you; you must authenticate with USER and PASS to
the proxy, and the proxy will authenticate on your behalf to the
upstream server. If all goes well, your session then continues
without a hitch. If you fluff your local authentication, the proxy
will give you an error message; and if the proxy fluffs the remote
authentication, the upstream server's error will be passed through
to you.

If you attempt to post before you have authenticated to the proxy,
the proxy will request authentication at the point of the POST
command. This is so that it will know your Unix user name when it
sees the actual article; it will feed the article through a
rewriting service on your behalf (by default this does no rewriting
at all) and post the rewritten version.

CONFIGURATION
-------------

The username you provide to the proxy should be your Unix username
on the proxy machine. The password you supply will be passed on to a
userv service called `newspasswd-validate'. A default version of
this is provided on behalf of all users; each user can override it
if they wish. This allows extreme flexibility in the matter of, for
example, one-time passwords, or multiple valid passwords enabling
more than one level of remote authentication.

The protocol expected of a `newspasswd-validate' reimplementation
is:

 - it should read one \n-terminated line which will contain the
   password.
 - it should print one \n-terminated line, which begins with either
   a `+' sign (password accepted) or a `-' sign (password invalid).
   In the latter case the rest of the line will be passed back to
   the NNTP client as an error message.
 - it should then conduct an authentication exchange on
   stdin/stdout; it will gain temporary access to the NNTP
   connection for this purpose.

Example runs:

  <<< b4dg3r                     [service client sends password]
  >>> +ok                        [we say password was fine]
  >>> AUTHINFO USER chairman     [now we start talking to NNTP server]
  <<< 381 now send password      [and we see the NNTP server's replies]
  >>> AUTHINFO PASS n1neofdiam0nds
  <<< 281 authentication accepted
  [satisfied, we now close the connection]

Or if (more likely) the whole point of running the proxy is to
gateway to a more exotic form of authentication:

  <<< b4dg3r                     [service client sends password]
  >>> +ok                        [we say password was fine]
  >>> AUTHINFO GENERIC md5cookie1way chairman [start talking to NNTP server]
  <<< 100 b3:b4:72:97:77:34:63:2a:90:18:47:18:b2:99:4a:24
  >>> MD5 1a:9e:45:ac:77:52:58:b5:4f:76:08:08:af:73:00:ed
  <<< 281 authentication accepted
  [satisfied, we now close the connection]

Or if the password comes out wrong:

  <<< b4dg3e                     [service client sends password]
  >>> -invalid password          [we reject it]
  [we close the connection in disgust]

The default implementation of this uses a file in the user's home
directory called `.newspasswd', which is created using the
`newspasswd' program. It assumes your upstream server supports
AUTHINFO GENERIC in a form that uses the NNTPAUTH environment
variable, or something compatible.

The article rewriting service is called `newspasswd-rewrite'. The
semantics are easy: a service implementation should read an entire
news article on stdin, and output one (typically a modified form of
the input) on stdout. The default service implementation just
executes `cat'.
