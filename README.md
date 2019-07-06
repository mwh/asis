asis is a tiny web server that serves out mod_asis-style asis files
over Unix sockets. It's intended for use with nginx or other servers
without that functionality built in.

asis always serves files out of the current working directory. On
receiving a request, it looks for either a file by that name with a
".asis" extension, or an "index.asis" file inside the directory at
that location. 

.asis files
-----------

A .asis file must start with a Status: line, then the HTTP headers
to use, a blank line, and the body of the response. For example:

    Status: 200
    Content-type: text/plain
    
    Hello, world!

Compiling and running
---------------------

Build asis with `make`. The asis server is launched with just

    asis SOCKET_PATH

where SOCKET_PATH is the path to the Unix socket to create. By
default, asis will create a socket called "socket" in the current
directory if no path is given.

Configuring a server
--------------------

The asis server is intended for use behind a proxying server that
supports Unix sockets, like nginx. For nginx, add a configuration
line

    proxy_pass http://unix:SOCKET_PATH:;

where you want requests to be forwarded from.
