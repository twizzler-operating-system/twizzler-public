#!/usr/bin/env bash

set -e
set -u

while read dest rest; do
	echo $dest $rest
	if [[ ! -f $dest ]]; then
		echo $rest | projects/$PROJECT/build/utils/hier > $dest
	else
		echo $rest | projects/$PROJECT/build/utils/hier -a $dest
	fi
done

for nd in $(find $1 -name '__ns*.dat'); do
	ns=${nd/.dat/}
	echo processing $ns '<-' $nd >&2
	projects/$PROJECT/build/utils/appendobj $ns < $nd
	rm $nd
	id=$(projects/$PROJECT/build/utils/objstat -i $ns)
	ln $ns $1/$id
done
