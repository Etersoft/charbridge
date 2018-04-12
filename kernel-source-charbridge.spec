%define module_name charbridge
%define module_version 0.1
%define module_release alt1

#### MODULE SOURCES ####
Name: kernel-source-%module_name
Version: %module_version
Release: %module_release

Summary: Sources for 'charbridge' kernel module

License: GPL
Group: Development/Kernel
URL: http://git.etersoft.ru

BuildArch: noarch

BuildPreReq: kernel-build-tools

Source: %module_name-%module_version.tar

%description
'charbridge' kernel module.

%prep
%setup -q -n %module_name-%version

%install
%__mkdir_p %buildroot%kernel_src/kernel-source-%module_name-%module_version
%__mv *.c *.h Makefile %buildroot%kernel_src/kernel-source-%module_name-%module_version
cd %buildroot%kernel_src
%__tar -c kernel-source-%module_name-%module_version | bzip2 -c > \
    %buildroot%kernel_src/kernel-source-%module_name-%module_version.tar.bz2

# hide Unpackages warnings
rm -rf %buildroot%kernel_src/kernel-source-%module_name-%module_version

%files
%kernel_src/kernel-source-%module_name-%module_version.tar.bz2

%changelog
* Thu Apr 12 2018 Pavel Vainerman <pv@altlinux.ru> 0.1-alt1
- initial build for ALT Linux
