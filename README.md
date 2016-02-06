![Logo](https://bitbucket.org/EionRobb/purple-hangouts/avatar)
# Hangouts Plugin for libpurple #

A replacement prpl for Hangouts in Pidgin/libpurple to support the proprietary protocol that Google uses for its Hangouts service.

This plugin is written by [Eion Robb](http://eion.robbmob.com/blog/) and [Mike 'Maiku' Ruprecht](https://bitbucket.org/CMaiku/).
Heavily inspired by the [hangups library](https://github.com/tdryer/hangups) by Tom Dryer (et. al.) using code from [Nakul Gulati](https://hg.pidgin.im/soc/2015/nakulgulati/main/) and protobufs from [Darryl Pogue](http://dpogue.ca/)

## Compiling ##
To compile, just do the standard `make && sudo make install` dance.  You'll need development packages for libpurple, libjson-glib, glib and libprotobuf-c to be able to compile.

## Windows ##
Development builds of Windows dll's live at http://eion.robbmob.com/libhangouts.dll
