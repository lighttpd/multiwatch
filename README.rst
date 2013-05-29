Description
-----------

:Homepage:
    http://redmine.lighttpd.net/projects/multiwatch/wiki

Multiwatch forks multiple instance of one application and keeps them running;
it is made to be used with spawn-fcgi, so all forks share the same fastcgi
socket (no webserver restart needed if you increase/decrease the number of
forks), and it is easier than to setup multiple daemontool supervised instances.

Usage
-----

Example for spawning two rails instances::

  #!/bin/sh
  # run script

  exec spawn-fcgi -n -s /tmp/fastcgi-rails.sock -u www-rails -U www-data -- /usr/bin/multiwatch -f 2 -- /home/rails/public/dispatch.fcgi

More details in the man page.


Build dependencies
------------------

* glib >= 2.16.0 (http://www.gtk.org/)
* libev (http://software.schmorp.de/pkg/libev.html)
* cmake or autotools (for snapshots/releases the autotool generated files are included)


Build
-----

* snapshot/release with autotools::

   ./configure
   make

* build from git: git://git.lighttpd.net/multiwatch.git

 * with autotools::

    ./autogen.sh
    ./configure
    make

 * with cmake (should work with snapshots/releases too)::

    cmake .
    make
