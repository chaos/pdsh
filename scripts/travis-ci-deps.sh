#!/bin/bash

prefix=$HOME/local
cachedir=$HOME/local/.cache

#
# Ordered list of software to download and install into $prefix.
#  NOTE: Code currently assumes .tar.gz suffix...
#
downloads="\
https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz \
https://ftp.gnu.org/gnu/automake/automake-1.15.tar.gz \
https://ftp.gnu.org/gnu/libtool/libtool-2.4.6.tar.gz"

declare -r prog=${0##*/}
declare -r long_opts="prefix:,cachedir:,verbose,printenv"
declare -r short_opts="vp:c:P"
declare -r usage="\
\n
Usage: $prog [OPTIONS]\n\
Download and install to a local prefix (default=$prefix)
the travis-ci dependencies for building pdsh\n\
\n\
Options:\n\
 -v, --verbose           Be verbose.\n\
 -P, --printenv          Print environment variables to stdout\n\
 -c, --cachedir=DIR      Check for precompiled dependency cache in DIR\n\
 -e, --max-cache-age=N   Expire cache in N days from creation\n\
 -p, --prefix=DIR        Install software into prefix\n
"

die() { echo -e "$prog: $@"; exit 1; }
say() { echo -e "$prog: $@"; }
debug() { test "$verbose" = "t" && say "$@"; }

GETOPTS=`/usr/bin/getopt -u -o $short_opts -l $long_opts -n $prog -- $@`
if test $? != 0; then
    die "$usage"
fi

eval set -- "$GETOPTS"
while true; do
    case "$1" in
      -v|--verbose)          verbose=t;     shift   ;;
      -c|--cachedir)         cachedir="$2"; shift 2 ;;
      -e|--max-cache-age)    cacheage="$2"; shift 2 ;;
      -p|--prefix)           prefix="$2";   shift 2 ;;
      -P|--printenv)         print_env=1;   shift   ;;
      --)                    shift ; break;         ;;
      *)                     die "Invalid option '$1'\n$usage" ;;
    esac
done

print_env () {
    echo "LD_LIBRARY_PATH=${prefix}/lib:$LD_LIBRARY_PATH"
    echo "CPPFLAGS=-I${prefix}/include"
    echo "LDFLAGS=-L${prefix}/lib"
    echo "PKG_CONFIG_PATH=${prefix}/lib/pkgconfig:${prefix}/share/pkgconfig"
    echo "PATH=${HOME}/.local/bin:${HOME}/local/usr/bin:${prefix}/bin:${PATH}"
    echo "export PATH PKG_CONFIG_PATH LDFLAGS CPPFLAGS LD_LIBRARY_PATH"
}

if test -n "$print_env"; then 
   print_env
   exit 0
fi

eval "${print_env}"

sanitize ()
{
    # Sanitize cache name
    echo $1 | sed 's/[\t /\]/_/g'
}

check_cache ()
{
    test -n "$cachedir" || return 1
    local url=$(sanitize $1)
    local cachefile="${cachedir}/${url}"
    test -f "${cachefile}" || return 1
    test -n "$cacheage"    || return 0

    local ctime=$(stat -c%Y ${cachefile})
    local maxage=$((${cacheage}*86400))
    test $ctime -gt $maxage && return 1
}

add_cache ()
{
    test -n "$cachedir" || return 0
    mkdir -p "${cachedir}" &&
    touch "${cachedir}/$(sanitize ${1})"
}

for pkg in $downloads; do
    name=$(basename ${pkg} .tar.gz)
    if check_cache "$name"; then
       say "Using cached version of ${name}"
       continue
    fi
    mkdir -p ${name}  || die "Failed to mkdir ${name}"
    (
      cd ${name} &&
      curl -L -O --insecure ${pkg} || die "Failed to download ${pkg}"
      tar --strip-components=1 -xf *.tar.gz || die "Failed to un-tar ${name}"
      CC=gcc ./configure --prefix=${prefix} \
                  --sysconfdir=${prefix}/etc || : &&
      make PREFIX=${prefix} &&
      make PREFIX=${prefix} install
    ) || die "Failed to build and install $name"
    add_cache "$name"
done
