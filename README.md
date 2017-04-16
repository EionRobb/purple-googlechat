![Logo](https://bitbucket.org/EionRobb/purple-hangouts/avatar)
# Hangouts Plugin for libpurple #

A replacement prpl for Hangouts in Pidgin/libpurple to support the proprietary protocol that Google uses for its Hangouts service.  So far it supports all the fun things that aren't part of the XMPP interface, such as Group Chats, synchronised history between devices and SMS support via Google Voice.

This plugin is written by [Eion Robb](http://eion.robbmob.com/blog/) and [Mike 'Maiku' Ruprecht](https://bitbucket.org/CMaiku/).
Heavily inspired by the [hangups library](https://github.com/tdryer/hangups) by Tom Dryer (et. al.) using code from [Nakul Gulati](https://hg.pidgin.im/soc/2015/nakulgulati/main/) and protobufs from [Darryl Pogue](http://dpogue.ca/)

Please read [the FAQ](https://bitbucket.org/EionRobb/purple-hangouts/wiki/Home) before posting any issues

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
On Fedora you can install [package](https://apps.fedoraproject.org/packages/purple-hangouts) from Fedora's main repository:
```
#!sh
sudo dnf install purple-hangouts pidgin-hangouts
```

## CentOS/RHEL ##
On CentOS/RHEL you can install [package](https://apps.fedoraproject.org/packages/purple-hangouts) from Fedora's [EPEL7](http://fedoraproject.org/wiki/EPEL) repository:
```
#!sh
sudo yum install purple-hangouts pidgin-hangouts
```

## Arch Linux ##
On Arch Linux you can install a [package](https://aur.archlinux.org/packages/purple-hangouts-hg) from the [AUR](https://wiki.archlinux.org/index.php/Arch_User_Repository):
```
#!sh
wget https://aur.archlinux.org/cgit/aur.git/snapshot/purple-hangouts-hg.tar.gz
tar -xvf purple-hangouts-hg.tar.gz
cd purple-hangouts-hg
makepkg -sri
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