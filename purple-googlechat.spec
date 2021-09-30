%global plugin_name googlechat

%global commit0 4302f901fd4fb5c1d5ad3b083fa5b29380332b9c
%global shortcommit0 %(c=%{commit0}; echo ${c:0:7})
%global archcommit0 %(c=%{commit0}; echo ${c:0:12})
%global date 20160714

Name: purple-%{plugin_name}
Version: 0
Release: 1.%{date}hg%{shortcommit0}%{?dist}
Summary: Google Chat plugin for libpurple

License: GPLv3+
URL: https://github.com/EionRobb/%name
Source0: https://github.com/EionRobb/%name/archive/%commit0/%name-%archcommit0.tar.gz

BuildRequires: pkgconfig(libprotobuf-c)
BuildRequires: pkgconfig(json-glib-1.0)
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(purple)
BuildRequires: pkgconfig(zlib)
BuildRequires: gcc

%package -n pidgin-%{plugin_name}
Summary: Adds pixmaps, icons and smileys for Google Chat protocol
BuildArch: noarch
Requires: %{name} = %{?epoch:%{epoch}:}%{version}-%{release}
Requires: pidgin

%description
Adds support for Google Chat to Pidgin, Adium, Finch and other libpurple
based messengers.

%description -n pidgin-%{plugin_name}
Adds pixmaps, icons and smileys for Google Chat protocol implemented by
purple-googlechat.

%prep
%autosetup -n %name-%commit0

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
* Thu Sep 30 2021 Pavel Raiskup <praiskup@redhat.com> - 0-1.20160714hg4302f90
- inherit purple-hangouts into purple-googlechat package
