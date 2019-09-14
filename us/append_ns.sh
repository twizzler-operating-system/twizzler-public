#!/bin/sh

set -e
set -u

while read dest rest; do
	if [[ ! -f $dest ]]; then
		echo $rest | projects/$PROJECT/build/utils/hier > $dest
	else
		echo $rest | projects/$PROJECT/build/utils/hier -a $dest
	fi
done

for nd in $(find projects/$PROJECT/build/us/objroot -name '__ns*.dat'); do
	ns=${nd/.dat/}
	echo processing $ns '<-' $nd
	projects/$PROJECT/build/utils/appendobj $ns < $nd
	rm $nd
	id=$(projects/$PROJECT/build/utils/objstat -i $ns)
	ln $ns projects/$PROJECT/build/us/objroot/$id
done
