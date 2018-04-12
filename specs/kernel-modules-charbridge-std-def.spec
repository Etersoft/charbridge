%define module_name	charbridge
%define module_release	alt1
%define module_version	0.1

%define kversion	4.9.93
%define krelease	alt0.1.M80P.1
%define flavour		std-def

%define base_arch %(echo %_target_cpu | sed 's/i.86/i386/;s/athlon/i386/')

%define module_dir /lib/modules/%kversion-%flavour-%krelease/%module_name

Name: kernel-modules-%module_name-%flavour
Version: %module_version
Release: %module_release

Summary: charbridge kernel module

License: Distributable
Group: System/Kernel and hardware
URL: https://git.etersoft.ru/projects/?p=asu/charbridge.git;a=summary

Packager: Kernel Maintainer Team <kernel@packages.altlinux.org>

ExclusiveOS: Linux

BuildPreReq: kernel-build-tools >= 0.7
BuildRequires: modutils
BuildRequires: rpm >= 4.0.2-75
BuildRequires: kernel-headers-modules-%flavour = %kversion-%krelease
BuildRequires: kernel-source-%module_name = %module_version

Provides: kernel-modules-%module_name-%kversion-%flavour-%krelease = %version-%release
Conflicts: kernel-modules-%module_name-%kversion-%flavour-%krelease < %version-%release
Conflicts: kernel-modules-%module_name-%kversion-%flavour-%krelease > %version-%release

PreReq: coreutils
PreReq: modutils
PreReq: kernel-image-%flavour = %kversion-%krelease
Requires(postun): kernel-image-%flavour = %kversion-%krelease

%description
charbridge module

%prep
rm -rf kernel-source-%module_name-%module_version

tar -jxvf %kernel_src/kernel-source-%module_name-%module_version.tar.bz2

%setup -D -T -n kernel-source-%module_name-%module_version

%build
. %_usrsrc/linux-%kversion-%flavour/gcc_version.inc
./configure --with-linuxdir=%_usrsrc/linux-%kversion-%flavour --with-modulesdir=%buildroot/%module_dir --with-kernel-release=%kversion-%flavour-%krelease
%__make CC=gcc-$GCC_VERSION

%install
make prefix="%buildroot" install

%post
%post_kernel_modules %kversion-%flavour-%krelease

%postun
%postun_kernel_modules %kversion-%flavour-%krelease

%files
%module_dir/*

%changelog
* Thu Apr 12 2018 Pavel Vainerman <pv@altlinux.ru> 0.1-alt1
- initial build for ALT Linux
