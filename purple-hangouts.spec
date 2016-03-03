%global plugin_name hangouts

%global commit0 a2c9af3a253cac3af87b9298c1d162d167eed070
%global shortcommit0 %(c=%{commit0}; echo ${c:0:7})
%global archcommit0 %(c=%{commit0}; echo ${c:0:12})
%global date 20160227

Name: purple-%{plugin_name}
Version: 1.0
Release: 1.%{date}hg%{shortcommit0}%{?dist}
Summary: Hangouts plugin for libpurple

License: GPLv3
URL: https://bitbucket.org/EionRobb/purple-hangouts/
Source0: https://bitbucket.org/EionRobb/purple-hangouts/get/%{commit0}.tar.gz#/purple-hangouts-%{shortcommit0}.tar.gz

BuildRequires: pkgconfig(libprotobuf-c)
BuildRequires: pkgconfig(json-glib-1.0)
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(purple)
BuildRequires: pkgconfig(zlib)
BuildRequires: pkgconfig(nss)
BuildRequires: gcc

%package -n pidgin-%{plugin_name}
Summary: Adds pixmaps, icons and smileys for Hangouts protocol
BuildArch: noarch
Requires: %{name} = %{version}-%{release}
Requires: pidgin

%description
Adds support for Hangouts to Pidgin, Adium, Finch and other libpurple 
based messengers.

%description -n pidgin-%{plugin_name}
Adds pixmaps, icons and smileys for Hangouts protocol inplemented by hangouts-purple.

%prep
%autosetup -n EionRobb-purple-%{plugin_name}-%{archcommit0}

# fix W: wrong-file-end-of-line-encoding
perl -i -pe 's/\r\n/\n/gs' README.md

# generating empty configure script
echo '#!/bin/bash' > configure
chmod +x configure

%build
%configure
%make_build

%install
# Creating base directories...
mkdir -p %{buildroot}%{_libdir}/purple-2/
mkdir -p %{buildroot}%{_datadir}/pixmaps/pidgin/protocols/{16,22,48}/

# Executing base install from makefile...
%make_install

# Setting correct chmod...
chmod 755 %{buildroot}%{_libdir}/purple-2/lib%{plugin_name}.so

# Installing icons...
install -p hangouts16.png %{buildroot}%{_datadir}/pixmaps/pidgin/protocols/16/%{plugin_name}.png
install -p hangouts22.png %{buildroot}%{_datadir}/pixmaps/pidgin/protocols/22/%{plugin_name}.png
install -p hangouts48.png %{buildroot}%{_datadir}/pixmaps/pidgin/protocols/48/%{plugin_name}.png

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%{_libdir}/purple-2/lib%{plugin_name}.so
%doc README.md

%files -n pidgin-%{plugin_name}
%{_datadir}/pixmaps/pidgin/protocols/*/%{plugin_name}.png

%changelog
* Mon Feb 29 2016 V1TSK <vitaly@easycoding.org> - 1.0-1.20160227hga2c9af3
- First SPEC version.
