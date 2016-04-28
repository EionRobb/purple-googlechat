![Logo](https://bitbucket.org/EionRobb/purple-hangouts/avatar)
# Hangouts Plugin for libpurple #

A replacement prpl for Hangouts in Pidgin/libpurple to support the proprietary protocol that Google uses for its Hangouts service.  So far it supports all the fun things that aren't part of the XMPP interface, such as Group Chats, synchronised history between devices and SMS support via Google Voice.

This plugin is written by [Eion Robb](http://eion.robbmob.com/blog/) and [Mike 'Maiku' Ruprecht](https://bitbucket.org/CMaiku/).
Heavily inspired by the [hangups library](https://github.com/tdryer/hangups) by Tom Dryer (et. al.) using code from [Nakul Gulati](https://hg.pidgin.im/soc/2015/nakulgulati/main/) and protobufs from [Darryl Pogue](http://dpogue.ca/)

## Compiling ##
To compile, just do the standard `make && sudo make install` dance.  You'll need development packages for libpurple, libjson-glib, glib and libprotobuf-c to be able to compile.

## Debian/Ubuntu ##
Run the following commands from a terminal

```
#!sh
sudo apt-get install libpurple-dev libjson-glib-dev libglib2.0-dev libprotobuf-c-dev protobuf-c-compiler mercurial make;
hg clone https://bitbucket.org/EionRobb/purple-hangouts/ && cd purple-hangouts;
make && sudo make install
```

## Fedora ##
On Fedora you can use [purple-hangouts](https://copr.fedorainfracloud.org/coprs/xvitaly/purple-hangouts/) COPR repository.

At first time you should add COPR repository and enable it:
```
#!sh
sudo dnf copr enable xvitaly/purple-hangouts
```
Now you can install packages:
```
#!sh
sudo dnf install purple-hangouts pidgin-hangouts
```

## Building RPM package for Fedora/openSUSE/CentOS/RHEL ##
```
#!sh
mkdir -p ~/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
wget https://bitbucket.org/EionRobb/purple-hangouts/raw/440c6734e1540525b2e25797c3856121e12719ee/purple-hangouts.spec -O ~/rpmbuild/SPECS/purple-hangouts.spec
sudo dnf builddep ~/rpmbuild/SPECS/purple-hangouts.spec
spectool --all --get-files ~/rpmbuild/SPECS/purple-hangouts.spec --directory ~/rpmbuild/SOURCES/
rpmbuild -ba  ~/rpmbuild/SPECS/purple-hangouts.spec
```
The result can be found in ``~/rpmbuild/RPMS/`uname -m`/`` directory.

## Windows ##
Use the [Windows installer](http://eion.robbmob.com/purple-hangouts.exe) to make life easier, otherwise development builds of Windows dll's live at http://eion.robbmob.com/libhangouts.dll (you'll also need libprotobuf-c-1.dll and libjson-glib-1.0.dll in your Pidgin folder)

## Like this plugin? ##
Say "Thanks" by [sending us $1](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=PZMBF2QVF69GA)