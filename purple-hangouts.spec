%global plugin_name hangouts

%global commit0 d6eb7fe69b1b3fd909b41c6b33566fc498c834b2
%global shortcommit0 %(c=%{commit0}; echo ${c:0:7})
%global archcommit0 %(c=%{commit0}; echo ${c:0:12})
%global date 20160714

Name: purple-%{plugin_name}
Version: 0
Release: 34.%{date}hg%{shortcommit0}%{?dist}
Epoch: 1
Summary: Hangouts plugin for libpurple

License: GPLv3+
URL: https://bitbucket.org/EionRobb/purple-hangouts/
Source0: https://bitbucket.org/EionRobb/purple-hangouts/get/%{commit0}.tar.gz#/%{name}-%{shortcommit0}.tar.gz

BuildRequires: pkgconfig(libprotobuf-c)
BuildRequires: pkgconfig(json-glib-1.0)
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(purple)
BuildRequires: pkgconfig(zlib)
BuildRequires: gcc

%package -n pidgin-%{plugin_name}
Summary: Adds pixmaps, icons and smileys for Hangouts protocol
BuildArch: noarch
Requires: %{name} = %{?epoch:%{epoch}:}%{version}-%{release}
Requires: pidgin

%description
Adds support for Hangouts to Pidgin, Adium, Finch and other libpurple
based messengers.

%description -n pidgin-%{plugin_name}
Adds pixmaps, icons and smileys for Hangouts protocol implemented by
hangouts-purple.

%prep
%autosetup -n EionRobb-purple-%{plugin_name}-%{archcommit0}

# fix W: wrong-file-end-of-line-encoding
sed -i -e "s,\r,," README.md

%build
export CFLAGS="%{optflags}"
export LDFLAGS="%{__global_ldflags}"
%make_build

%install
# Executing base install from makefile...
%make_install

# Setting correct chmod...
chmod 755 %{buildroot}%{_libdir}/purple-2/lib%{plugin_name}.so

%files
%{_libdir}/purple-2/lib%{plugin_name}.so
%license gpl3.txt
%doc README.md

%files -n pidgin-%{plugin_name}
%{_datadir}/pixmaps/pidgin/protocols/*/%{plugin_name}.png

%changelog
* Tue Jul 19 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1:0-34.20160714hgd6eb7fe
- Updated to latest snapshot.

* Tue Jul 12 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1:0-33.20160712hg2c60a5e
- Updated to latest snapshot.

* Sun Jul 10 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1:0-32.20160710hg5378549
- Fixed Requires for pidgin subpackage. Replaced Perl to sed in prep section.

* Sun Jul 10 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 1:0-31.20160710hg5378549
- Fixed version. Updated SPEC file.

* Sun Jul 10 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-30.20160710hg5378549
- Updated to latest snapshot. Updated install section due upstream changes.

* Tue Jun 21 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-29.20160621hg38f0731
- Added missing LDFLAGS to build.

* Tue Jun 21 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-28.20160621hg38f0731
- Updated to latest Git snapshot.

* Fri Jun 17 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-27.20160615hg2059439
- Updated to latest snapshot. Removed patch.

* Sun Jun 12 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-26.20160609hg90c515d
- Updated to latest snapshot. Removed empty configure script.

* Mon Jun 06 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-25.20160601hgf362605
- Updated to latest snapshot.

* Fri May 27 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-24.20160527hge643cc7
- Updated to latest snapshot. Updated patch.

* Thu May 26 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-23.20160526hg8cafb7a
- Updated to latest snapshot. Updated patch.

* Fri May 13 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-22.20160512hg7a23ed5
- Updated to latest snapshot.

* Tue May 10 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-21.20160510hg78a9c80
- Updated to latest snapshot. Updated patch.

* Sat May 07 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-20.20160507hg2095ac0
- Updated to latest snapshot.

* Thu May 05 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-19.20160505hg4cf1d50
- Updated to latest snapshot.

* Wed May 04 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-18.20160504hge8c30b6
- Updated to latest snapshot.

* Tue May 03 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-17.20160502hg2631ad2
- Updated to latest snapshot. Added patch for Fedora.

* Tue Apr 26 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-16.20160426hgac1741f
- Updated to latest snapshot.

* Thu Apr 21 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-15.20160421hg6f76943
- Updated to latest snapshot.

* Sun Apr 17 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-14.20160417hg635f50c
- Updated to latest snapshot.

* Fri Apr 15 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-13.20160413hg92bfbf1
- Updated to latest snapshot.

* Sun Apr 10 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-12.20160410hga5b0e60
- Updated to latest snapshot.

* Sat Apr 09 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-11.20160409hg7442ecd
- Updated to latest snapshot.

* Wed Apr 06 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-10.20160406hgab7c25a
- Added license section. Updated to latest snapshot.

* Tue Apr 05 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-9.20160405hgbd3b2be
- Fixed SPEC. Updated to latest snapshot.

* Sun Apr 03 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-8.20160403hg0f3cbbd
- Updated to latest snapshot.

* Fri Apr 01 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-7.20160401hg8b37dcc
- Updated to latest snapshot.

* Tue Mar 22 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-6.20160322hg735b7f8
- Updated to latest snapshot.

* Thu Mar 17 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-5.20160316hgfde2d8a
- Updated to latest snapshot.

* Tue Mar 15 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-4.20160315hg694bd41
- Updated to latest snapshot.

* Tue Mar 08 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-3.20160306hgb7ba81f
- Updated to latest snapshot.

* Fri Mar 04 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-2.20160303hg4789156
- Updated to latest snapshot.

* Mon Feb 29 2016 Vitaly Zaitsev <vitaly@easycoding.org> - 0-1.20160227hga2c9af3
- First SPEC version.
