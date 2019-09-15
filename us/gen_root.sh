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
		if [[ -f $SYSROOT/$ent.data ]]; then
			target_data=$OBJROOT/${ent//\//_}.data.obj
			projects/$PROJECT/build/utils/file2obj -i $SYSROOT/$ent.data -o $target_data -p rh
			id_data=$(projects/$PROJECT/build/utils/objstat -i $target_data)
			mv $target_data $OBJROOT/$id_data
			echo $ent.data'*'$id_data

			projects/$PROJECT/build/utils/file2obj -i $SYSROOT/$ent -o $target -p rxh -f 1:RWD:$id_data
			id=$(projects/$PROJECT/build/utils/objstat -i $target)
			mv $target $OBJROOT/$id
			echo $ent'*'$id
		else
			projects/$PROJECT/build/utils/file2obj -i $SYSROOT/$ent -o $target -p rxh
			id=$(projects/$PROJECT/build/utils/objstat -i $target)
			mv $target $OBJROOT/$id
			echo $ent'*'$id
		fi
	fi
done

