#!/bin/sh

set -e
set -u

export SYSROOT=$(pwd)/projects/$PROJECT/build/us/sysroot
export OBJROOT=$(pwd)/projects/$PROJECT/build/us/objroot

rm -rf $OBJROOT
mkdir -p $OBJROOT

for ent in $(find projects/$PROJECT/build/us/sysroot | cut -d'/' -f6-); do
	if [[ -f $SYSROOT/$ent ]]; then
		target=$OBJROOT/${ent//\//_}.obj
		projects/$PROJECT/build/utils/file2obj -i $SYSROOT/$ent -o $target -p rxh
		id=$(projects/$PROJECT/build/utils/objstat -i $target)
		mv $target $OBJROOT/$id
		echo $ent'*'$id
	fi
done

