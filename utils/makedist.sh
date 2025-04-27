#!/bin/sh
cpfile() {
if [ ! -f $1/$2 ]; then
	/bin/cp  $2 $1/$2
fi
}

cpfileforce() {
/bin/cp -f $2 $1/$2
}

md() {
if [ ! -d $1/$2 ]; then
	mkdir $1/$2
fi
}

md $1
md $1 bin
md $1 conf
md $1 lib

for i in `find bin/ -maxdepth 1 -type f`; do
	cpfileforce $1 $i
done

for i in `find conf/ -maxdepth 1 -type f`; do
	cpfile $1 $i
done

for i in `find lib/ -maxdepth 1 -type f`; do
	cpfileforce $1 $i
done
