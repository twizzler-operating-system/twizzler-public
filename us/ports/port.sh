#!/bin/sh

set -e #exit on errors
set -u #exit on undef vars

# pretty printing!
function error_exit () {
	echo $(tput setaf 1)$@$(tput setaf 7) 2>&1
	exit 1
}

function error_print () {
	echo $(tput setaf 1)$@$(tput setaf 7) 2>&1
}

function status_print () {
	echo $(tput setaf 6)$@$(tput setaf 7)
}


if [[ "$1" == "" ]]; then
	error_exit "please specify the package to port"
fi

if [[ "$PROJECT" == "" ]]; then
	error_exit "please specify the PROJECT variable before running this script."
fi

#port files can use these variables to locate patch files and the install destination
PORTDIR=$(pwd)/us/ports
SYSROOT=$(pwd)/projects/$PROJECT/build/us/sysroot

#we'll need the TOOLCHAIN_PATH to get access to the twizzler C compiler, and the other
#two to construct the triplet correctly.
export $(grep TOOLCHAIN_PATH projects/$PROJECT/config.mk)
export $(grep CONFIG_ARCH projects/$PROJECT/config.mk)
export $(grep CONFIG_MACHINE projects/$PROJECT/config.mk)

export TARGET=$CONFIG_ARCH-$CONFIG_MACHINE-twizzler-musl

# default to nothing
: "${MAKEFLAGS:=}"

export MAKEFLAGS

source us/ports/$1.port

WORKDIR=$(pwd)/projects/$PROJECT/build/us/ports/$NAME

#cleanup temporary files on error
function cleanup () {
	error_print "error occurred; cleaning up"
	rm -rf $WORKDIR
}
trap cleanup 0

rm -rf $WORKDIR
mkdir -p $WORKDIR
(
cd $WORKDIR

status_print "retrieving sources: " $NAME

for i in "${!DOWNLOADS[*]}"; do
	thefile=$(basename ${DOWNLOADS[$i]})
	theurl=${DOWNLOADS[$i]}
	thehash=${HASHES[$i]}
	# check if we've already downloaded the file, and if so, reuse it. Also verify the file hash if
	# we have it.
	status_print "  $thefile"
	if [[ ! -f $thefile ]]; then
		curl --progress-bar $theurl > $thefile
	fi
	if [[ "$thehash" != "-" ]]; then
		if ! echo "$thehash  $thefile" | tee | sha1sum -c - 2>&1 > /dev/null; then
			curl $theurl > $thefile
			echo "$thehash  $thefile" | tee | sha1sum -c -
		fi
	fi
done

PATH="$PATH:$TOOLCHAIN_PATH/bin"

status_print "starting prepare(): " $NAME

#do these in subshells in case they change directories
( prepare ) > prepare.stdout

status_print "starting build(): " $NAME

( build ) > build.stdout

status_print "starting install(): " $NAME

( install ) > install.stdout

)
status_print "post-processing executables: " $NAME
for i in "${SPLITS[@]}"; do
	echo " " $i
	./projects/$PROJECT/build/utils/elfsplit $SYSROOT/$i
	rm $SYSROOT/$i
	mv $SYSROOT/$i.text $SYSROOT/$i
done

trap - 0

status_print "finished packaging $NAME"

