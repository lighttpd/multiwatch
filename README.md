# multiwatch

**homepage:** <https://redmine.lighttpd.net/projects/multiwatch/wiki>

Multiwatch forks multiple instance of one application and keeps them running;
it is made to be used with spawn-fcgi, so all forks share the same fastcgi
socket (no webserver restart needed if you increase/decrease the number of
forks), and it is easier than seting up multiple daemontool supervised instances.

## Usage

Example for spawning two rails instances::

```
#!/bin/sh
# run script

exec spawn-fcgi -n -s /tmp/fastcgi-rails.sock -u www-rails -U www-data -- /usr/bin/multiwatch -f 2 -- /home/rails/public/dispatch.fcgi
```

More details in the man page.

## Build dependencies

* [meson](https://mesonbuild.com/) as build system
* [glib >= 2.16.0](https://gitlab.gnome.org/GNOME/glib)
* [libev](https://software.schmorp.de/pkg/libev.html)

## Build

Setup a build directory `build`:

    meson setup build --prefix /usr/local

Compile it:

    meson compile -C build

Install:

    meson install -C build

## Usage

See man page, e.g. [rendered](https://manpages.debian.org/unstable/multiwatch/multiwatch.1.en.html)
for debian unstable.
