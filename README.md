# Google Chat Plugin for libpurple #

A replacement prpl for Google Chat in Pidgin/libpurple to support the proprietary protocol that Google uses for its "Google Chat" service.

This plugin is written by [Eion Robb](https://github.com/EionRobb/) based on the old Hangouts plugin written by [Eion Robb](https://github.com/EionRobb/) and [Mike 'Maiku' Ruprecht](https://github.com/cmaiku).

## Compiling ##
To compile, just do the standard `make && sudo make install` dance.  You'll need development packages for libpurple, libjson-glib, glib and libprotobuf-c to be able to compile.

## Debian/Ubuntu ##
Run the following commands from a terminal

```
#!sh
sudo apt-get install -y libpurple-dev libjson-glib-dev libglib2.0-dev libprotobuf-c-dev protobuf-c-compiler git make;
git clone https://github.com/EionRobb/purple-googlechat/ && cd purple-googlechat;
make && sudo make install
```

## Fedora ##
```
json-glib-devel libpurple-devel glib2-devel libpurple-devel protobuf-c-devel protobuf-c-compiler
```

## Windows ##
Development builds of Windows dll's will live at https://eion.robbmob.com/libgooglechat.dll - copy this into your C:\Program Files (x86)\Pidgin\plugins folder.  You'll also need [libprotobuf-c-1.dll](https://github.com/EionRobb/purple-googlechat/raw/master/libprotobuf-c-1.dll) and [libjson-glib-1.0.dll](https://github.com/EionRobb/purple-googlechat/raw/master/libjson-glib-1.0.dll) in your C:\Program Files (x86)\Pidgin folder (not the plugins subfolder)

## Bitlbee ##
If you're using this plugin with bitlbee, authentication might be a little confusing.  After you follow the prompts, you should get a DM from a `purple_request_0` user asking for the token.  You can also set the token as the password on the account, if you don't see the DM.

## Like this plugin? ##
Say "Thanks" by [sending us $1](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=PZMBF2QVF69GA)
