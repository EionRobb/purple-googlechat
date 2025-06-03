# Google Chat Plugin for libpurple #

A replacement prpl for Google Chat in Pidgin/libpurple to support the proprietary protocol that Google uses for its "Google Chat" service.

This plugin is written by [Eion Robb](https://github.com/EionRobb/) based on the old Hangouts plugin written by [Eion Robb](https://github.com/EionRobb/) and [Mike 'Maiku' Ruprecht](https://github.com/cmaiku).

## Compiling ##
To compile, just do the standard `make && sudo make install` dance.  You'll need development packages for libpurple, libjson-glib, glib and libprotobuf-c to be able to compile.

## Debian/Ubuntu ##
Run the following commands from a terminal

```bash
#!sh
sudo apt-get install -y libpurple-dev libjson-glib-dev libglib2.0-dev libprotobuf-c-dev protobuf-c-compiler git make;
git clone https://github.com/EionRobb/purple-googlechat/ && cd purple-googlechat;
make && sudo make install
```

## Fedora ##
```bash
json-glib-devel libpurple-devel glib2-devel libpurple-devel protobuf-c-devel protobuf-c-compiler
```

## Windows ##
Builds of Windows dll's are [here](https://github.com/EionRobb/purple-googlechat/releases/latest) - copy this into your C:\Program Files (x86)\Pidgin\plugins folder.  You'll also need [libprotobuf-c-1.dll](https://github.com/EionRobb/purple-googlechat/raw/master/libprotobuf-c-1.dll) and [libjson-glib-1.0.dll](https://github.com/EionRobb/purple-googlechat/raw/master/libjson-glib-1.0.dll) in your C:\Program Files (x86)\Pidgin folder (*not* the plugins subfolder)

Until there's some kind of helper/installer, you might want to copy in the protocol icons into the pixmaps folder, eg copy [github.com/EionRobb/purple-googlechat/googlechat16.png](https://github.com/EionRobb/purple-googlechat/raw/master/googlechat16.png) to C:\Program Files (x86)\Pidgin\pixmaps\pidgin\protocols\16\googlechat.png (and the same for the 22 and 48 png files)

## Authentication ##
The plugin uses cookies from a logged in browser to authenticate
1. Open https://chat.google.com/ in a private/incognito browser window and log in normally
2. Press F12 to open developer tools.
3. Select the "Application" (Chrome) or "Storage" (Firefox) tab.
4. In the sidebar, expand "Cookies" and select https://chat.google.com.
5. In the cookie list, find the COMPASS, SSID, SID, OSID and HSID rows.
6. When using Firefox, you may have multiple COMPASS cookies with different paths. Pick the one where path is /.
7. Paste the value of each cookie into the 'Advanced' tab of the account:
![image](https://github.com/user-attachments/assets/7e534172-1557-490b-9cb1-f8022ad1461c)
8. Close the browser window to prevent the cookies being invalidated

There's also a handy extension https://github.com/mautrix/googlechat/issues/93#issuecomment-1689792422
* Firefox: [addons.mozilla.org/en-US/firefox/addon/gchat-login-cookie-generator](https://addons.mozilla.org/en-US/firefox/addon/gchat-login-cookie-generator/) and
* Chromium based browsers: [chrome.google.com/webstore/detail/matrix-gchat-bridge-login/mofmfbkepponmdchhamalbcldoajbmho](https://chrome.google.com/webstore/detail/matrix-gchat-bridge-login/mofmfbkepponmdchhamalbcldoajbmho)

You may also need to use a fresh "Guest Profile" in Chrome if the above doesn't work

## Bitlbee ##
Grab the cookie values as above, then set them with
```irc
account <account id> set COMPASS_token <value>
account <account id> set SSID_token <value>
account <account id> set SID_token <value>
account <account id> set OSID_token <value>
account <account id> set HSID_token <value>
```

## Like this plugin? ##
Say "Thanks" by [buying me a coffee](https://buymeacoffee.com/eionrobb)
