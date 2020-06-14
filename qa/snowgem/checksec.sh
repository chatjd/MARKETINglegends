#!/bin/bash
#
# The BSD License (http://www.opensource.org/licenses/bsd-license.php) 
# specifies the terms and conditions of use for checksec.sh:
#
# Copyright (c) 2009-2011, Tobias Klein.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions 
# are met:
# 
# * Redistributions of source code must retain the above copyright 
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright 
#   notice, this list of conditions and the following disclaimer in 
#   the documentation and/or other materials provided with the 
#   distribution.
# * Neither the name of Tobias Klein nor the name of trapkit.de may be 
#   used to endorse or promote products derived from this software 
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH 
# DAMAGE.
#
# Name    : checksec.sh
# Version : 1.5
# Author  : Tobias Klein
# Date    : November 2011
# Download: http://www.trapkit.de/tools/checksec.html
# Changes : http://www.trapkit.de/tools/checksec_changes.txt
#
# Description:
#
# Modern Linux distributions offer some mitigation techniques to make it 
# harder to exploit software vulnerabilities reliably. Mitigations such 
# as RELRO, NoExecute (NX), Stack Canaries, Address Space Layout 
# Randomization (ASLR) and Position Independent Executables (PIE) have 
# made reliably exploiting any vulnerabilities that do exist far more 
# challenging. The checksec.sh script is designed to test what *standard* 
# Linux OS and PaX (http://pax.grsecurity.net/) security features are being 
# used.
#
# As of version 1.3 the script also lists the status of various Linux kernel 
# protection mechanisms.
#
# Credits:
#
# Thanks to Brad Spengler (grsecurity.net) for the PaX support.
# Thanks to Jon Oberheide (jon.oberheide.org) for the kernel support.
# Thanks to Ollie Whitehouse (Research In Motion) for rpath/runpath support.
# 
# Others that contributed to checksec.sh (in no particular order):
#
# Simon Ruderich, Denis Scherbakov, Stefan Kuttler, Radoslaw Madej,
# Anthony G. Basile, Martin Vaeth and Brian Davis. 
#

# global vars
have_readelf=1
verbose=false

# FORTIFY_SOURCE vars
FS_end=_chk
FS_cnt_total=0
FS_cnt_checked=0
FS_cnt_unchecked=0
FS_chk_func_libc=0
FS_functions=0
FS_libc=0
 
# version information
version() {
  echo "checksec v1.5, Tobias Klein, www.trapkit.de, November 2011"
  echo 
}

# help
help() {
  echo "Usage: checksec [OPTION]"
  echo
  echo "Options:"
  echo
  echo "  --file <executable-file>"
  echo "  --dir <directory> [-v]"
  echo "  --proc <process name>"
  echo "  --proc-all"
  echo "  --proc-libs <process ID>"
  echo "  --kernel"
  echo "  --fortify-file <executable-file>"
  echo "  --fortify-proc <process ID>"
  echo "  --version"
  echo "  --help"
  echo
  echo "For more information, see:"
  echo "  http://www.trapkit.de/tools/checksec.html"
  echo
}

# check if command exists
command_exists () {
  type $1  > /dev/null 2>&1;
}

# check if directory exists
dir_exists () {
  if [ -d $1 ] ; then
    return 0
  else
    return 1
  fi
}

# check user privileges
root_privs () {
  if [ $(/usr/bin/id -u) -eq 0 ] ; then
    return 0
  else
    return 1
  fi
}

# check if input is numeric
isNumeric () {
  echo "$@" | grep -q -v "[^0-9]"
}

# check if input is a string
isString () {
  echo "$@" | grep -q -v "[^A-Za-z]"
}

# check file(s)
filecheck() {
  # check for RELRO support
  if readelf -l $1 2>/dev/null | grep -q 'GNU_RELRO'; then
    if readelf -d $1 2>/dev/null | grep -q 'BIND_NOW'; then
      echo -n -e '\033[32mFull RELRO   \033[m   '
    else
      echo -n -e '\033[33mPartial RELRO\033[m   '
    fi
  else
    echo -n -e '\033[31mNo RELRO     \033[m   '
  fi

  # check for stack canary support
  if readelf -s $1 2>/dev/null | grep -q '__stack_chk_fail'; then
    echo -n -e '\033[32mCanary found   \033[m   '
  else
    echo -n -e '\033[31mNo canary found\033[m   '
  fi

  # check for NX support
  if readelf -W -l $1 2>/dev/null | grep 'GNU_STACK' | grep -q 'RWE'; then
    echo -n -e '\033[31mNX disabled\033[m   '
  else
    echo -n -e '\033[32mNX enabled \033[m   '
  fi 

  # check for PIE support
  if readelf -h $1 2>/dev/null | grep -q 'Type:[[:space:]]*EXEC'; then
    echo -n -e '\033[31mNo PIE       \033[m   '
  elif readelf -h $1 2>/dev/null | grep -q 'Type:[[:space:]]*DYN'; then
    if readelf -d $1 2>/dev/null | grep -q '(DEBUG)'; then
      echo -n -e '\033[32mPIE enabled  \033[m   '
    else   
      echo -n -e '\033[33mDSO          \033[m   '
    fi
  else
    echo -n -e '\033[33mNot an ELF file\033[m   '
  fi 

  # check for rpath / run path
  if readelf -d $1 2>/dev/null | grep -q 'rpath'; then
   echo -n -e '\033[31mRPATH    \033[m  '
  else
   echo -n -e '\033[32mNo RPATH \033[m  '
  fi

  if readelf -d $1 2>/dev/null | grep -q 'runpath'; then
   echo -n -e '\033[31mRUNPATH    \033[m  '
  else
   echo -n -e '\033[32mNo RUNPATH \033[m  '
  fi
}

# check process(es)
proccheck() {
  # check for RELRO support
  if readelf -l $1/exe 2>/dev/null | grep -q 'Program Headers'; then
    if readelf -l $1/exe 2>/dev/null | grep -q 'GNU_RELRO'; then
      if readelf -d $1/exe 2>/dev/null | grep -q 'BIND_NOW'; then
        echo -n -e '\033[32mFull RELRO       \033[m '
      else
        echo -n -e '\033[33mPartial RELRO    \033[m '
      fi
    else
      echo -n -e '\033[31mNo RELRO         \033[m '
    fi
  else
    echo -n -e '\033[31mPermission denied (please run as root)\033[m\n'
    exit 1
  fi

  # check for stack canary support
  if readelf -s $1/exe 2>/dev/null | grep -q 'Symbol table'; then
    if readelf -s $1/exe 2>/dev/null | grep -q '__stack_chk_fail'; then
      echo -n -e '\033[32mCanary found         \033[m  '
    else
      echo -n -e '\033[31mNo canary found      \033[m  '
    fi
  else
    if [ "$1" != "1" ] ; then
      echo -n -e '\033[33mPermission denied    \033[m  '
    else
      echo -n -e '\033[33mNo symbol table found\033[m  '
    fi
  fi

  # first check for PaX support
  if cat $1/status 2> /dev/null | grep -q 'PaX:'; then
    pageexec=( $(cat $1/status 2> /dev/null | grep 'PaX:' | cut -b6) )
    segmexec=( $(cat $1/status 2> /dev/null | grep 'PaX:' | cut -b10) )
    mprotect=( $(cat $1/status 2> /dev/null | grep 'PaX:' | cut -b8) )
    randmmap=( $(cat $1/status 2> /dev/null | grep 'PaX:' | cut -b9) )
    if [[ "$pageexec" = "P" || "$segmexec" = "S" ]] && [[ "$mprotect" = "M" && "$randmmap" = "R" ]] ; then
      echo -n -e '\033[32mPaX enabled\033[m   '
    elif [[ "$pageexec" = "p" && "$segmexec" = "s" && "$randmmap" = "R" ]] ; then
      echo -n -e '\033[33mPaX ASLR only\033[m '
    elif [[ "$pageexec" = "P" || "$segmexec" = "S" ]] && [[ "$mprotect" = "m" && "$randmmap" = "R" ]] ; then
      echo -n -e '\033[33mPaX mprot off \033[m'
    elif [[ "$pageexec" = "P" || "$segmexec" = "S" ]] && [[ "$mprotect" = "M" && "$randmmap" = "r" ]] ; then
      echo -n -e '\033[33mPaX ASLR off\033[m  '
    elif [[ "$pageexec" = "P" || "$segmexec" = "S" ]] && [[ "$mprotect" = "m" && "$randmmap" = "r" ]] ; then
      echo -n -e '\033[33mPaX NX only\033[m   '
    else
      echo -n -e '\033[31mPaX disabled\033[m  '
    fi
  # fallback check for NX support
  elif readelf -W -l $1/exe 2>/dev/null | grep 'GNU_STACK' | grep -q 'RWE'; then
    echo -n -e '\033[31mNX disabled\033[m   '
  else
    echo -n -e '\033[32mNX enabled \033[m   '
  fi 

  # check for PIE support
  if readelf -h $1/exe 2>/dev/null | grep -q 'Type:[[:space:]]*EXEC'; then
    echo -n -e '\033[31mNo PIE               \033[m   '
  elif readelf -h $1/exe 2>/dev/null | grep -q 'Type:[[:space:]]*DYN'; then
    if readelf -d $1/exe 2>/dev/null | grep -q '(DEBUG)'; then
      echo -n -e '\033[32mPIE enabled          \033[m   '
    else   
      echo -n -e '\033[33mDynamic Shared Object\033[m   '
    fi
  else
    echo -n -e '\033[33mNot an ELF file      \033[m   '
  fi
}

# check mapped libraries
libcheck() {
  libs=( $(awk '{ print $6 }' /proc/$1/maps | grep '/' | sort -u | xargs file | grep ELF | awk '{ print $1 }' | sed 's/:/ /') )
 
  printf "\n* Loaded libraries (file information, # of mapped files: ${#libs[@]}):\n\n"
  
  for element in $(seq 0 $((${#libs[@]} - 1)))
  do
    echo "  ${libs[$element]}:"
    echo -n "    "
    filecheck ${libs[$element]}
    printf "\n\n"
  done
}

# check for system-wide ASLR support
aslrcheck() {
  # PaX ASLR support
  if !(cat /proc/1/status 2> /dev/null | grep -q 'Name:') ; then
    echo -n -e ':\033[33m insufficient privileges for PaX ASLR checks\033[m\n'
    echo -n -e '  Fallback to standard Linux ASLR check'
  fi
  
  if cat /proc/1/status 2> /dev/null | grep -q 'PaX:'; then
    printf ": "
    if cat /proc/1/status 2> /dev/null | grep 'PaX:' | grep -q 'R'; then
      echo -n -e '\033[32mPaX ASLR enabled\033[m\n\n'
    else
      echo -n -e '\033[31mPaX ASLR disabled\033[m\n\n'
    fi
  else
    # standard Linux 'kernel.randomize_va_space' ASLR support
    # (see the kernel file 'Documentation/sysctl/kernel.txt' for a detailed description)
    printf " (kernel.randomize_va_space): "
    if /sbin/sysctl -a 2>/dev/null | grep -q 'kernel.randomize_va_space = 1'; then
      echo -n -e '\033[33mOn (Setting: 1)\033[m\n\n'
      printf "  Description - Make the addresses of mmap base, stack and VDSO page randomized.\n"
      printf "  This, among other things, implies that shared libraries will be loaded to \n"
      printf "  random addresses. Also for PIE-linked binaries, the location of code start\n"
      printf "  is randomized. Heap addresses are *not* randomized.\n\n"
    elif /sbin/sysctl -a 2>/dev/null | grep -q 'kernel.randomize_va_space = 2'; then
      echo -n -e '\033[32mOn (Setting: 2)\033[m\n\n'
      printf "  Description - Make the addresses of mmap base, heap, stack and VDSO page randomized.\n"
      printf "  This, among other things, implies that shared libraries will be loaded to random \n"
      printf "  addresses. Also for PIE-linked binaries, the location of code start is randomized.\n\n"
    elif /sbin/sysctl -a 2>/dev/null | grep -q 'kernel.randomize_va_space = 0'; then
      echo -n -e '\033[31mOff (Setting: 0)\033[m\n'
    else
      echo -n -e '\033[31mNot supported\033[m\n'
    fi
    printf "  See the kernel file 'Documentation/sysctl/kernel.txt' for more details.\n\n"
  fi 
}

# check cpu nx flag
nxcheck() {
  if grep -q nx /proc/cpuinfo; then
    echo -n -e '\033[32mYes\033[m\n\n'
  else
    echo -n -e '\033[31mNo\033[m\n\n'
  fi
}

# check for kernel protection mechanisms
kernelcheck() {
  printf "  Description - List the status of kernel protection mechanisms. Rather than\n"
  printf "  inspect kernel mechanisms that may aid in the prevention of exploitation of\n"
  printf "  userspace processes, this option lists the status of kernel configuration\n"
  printf "  options that harden the kernel itself against attack.\n\n"
  printf "  Kernel config: "
 
  if [ -f /proc/config.gz ] ; then
    kconfig="zcat /proc/config.gz"
    printf "\033[32m/proc/config.gz\033[m\n\n"
  elif [ -f /boot/config-`uname -r` ] ; then
    kconfig="cat /boot/config-`uname -r`"
    printf "\033[33m/boot/config-`uname -r`\033[m\n\n"
    printf "  Warning: The config on disk may not represent running kernel config!\n\n";
  elif [ -f "${KBUILD_OUTPUT:-/usr/src/linux}"/.config ] ; then
    kconfig="cat ${KBUILD_OUTPUT:-/usr/src/linux}/.config"
    printf "\033[33m%s\033[m\n\n" "${KBUILD_OUTPUT:-/usr/src/linux}/.config"
    printf "  Warning: The config on disk may not represent running kernel config!\n\n";
  else
    printf "\033[31mNOT FOUND\033[m\n\n"
    exit 0
  fi

  printf "  GCC stack protector support:            "
  if $kconfig | grep -qi 'CONFIG_CC_STACKPROTECTOR=y'; then
    printf "\033[32mEnabled\033[m\n"
  else
    printf "\033[31mDisabled\033[m\n"
  fi

  printf "  Strict user copy checks:                "
  if $kconfig | grep -qi 'CONFIG_DEBUG_STRICT_USER_COPY_CHECKS=y'; then
    printf "\033[32mEnabled\033[m\n"
  else
    printf "\033[31mDisabled\033[m\n"
  fi

  printf "  Enforce read-only kernel data:          "
  if $kconfig | grep -qi 'CONFIG_DEBUG_RODATA=y'; then
    printf "\033[32mEnabled\033[m\n"
  else
    printf "\033[31mDisabled\033[m\n"
  fi
  printf "  Restrict /dev/mem access:               "
  if $kconfig | grep -qi 'CONFIG_STRICT_DEVMEM=y'; then
    printf "\033[32mEnabled\033[m\n"
  else
    printf "\033[31mDisabled\033[m\n"
  fi

  printf "  Restrict /dev/km