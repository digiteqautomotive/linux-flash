Name:           fw-flash
Version:        1.1
Release:        1
Summary:        Digiteq Automotive FG4 flash tool
License:        GPL-3.0-only
Group:          Applications/System
Url:            http://www.digiteqautomotive.com
Source0:        fw-flash.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  make


%description
fw-flash is a tool for flashing Digiteq Automotive FG4 cards firmware.

%prep
%setup -q -n fw-flash

%build
export CFLAGS="${RPM_OPT_FLAGS}"
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot} PREFIX=/usr

%files
%defattr(-,root,root)
%{_bindir}/*

%changelog
* Tue Aug 29 20:02:10 CEST 2023 - martin.tuma@digiteqautomotive.com 1.1-1
- Fixed broken flashing when no card SN given

* Fri Aug 18 13:31:35 CEST 2023 - martin.tuma@digiteqautomotive.com 1.0-1
- Added support for card-specific FWs (T100/T200)

* Thu May  5 11:36:03 CEST 2022 - martin.tuma@digiteqautomotive.com 0.2-1
- Added "make install" Makefile target
- Added RPM package file

* Fri Nov 19 00:00:00 CET 2021 - martin.tuma@digiteqautomotive.com 0.1-1
- Initial release
