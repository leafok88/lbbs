#!/bin/sh
cpfile() {
if [ ! -f $1/$2 ]; then
  /bin/cp  $2 $1/$2
fi
}

md() {
if [ ! -d $1/$2 ]; then
  mkdir $1/$2
fi
}

PRG_HOME="$1"

md ${PRG_HOME} conf
md ${PRG_HOME} data
md ${PRG_HOME} var
md ${PRG_HOME} log
md ${PRG_HOME}/data chicken

for i in `find conf/ -maxdepth 1 -type f`; do
cpfile ${PRG_HOME} $i
done

for i in `find data/ -maxdepth 1 -type f`; do
cpfile ${PRG_HOME} $i
done

chown bbs:bbs -R ${PRG_HOME}
find ${PRG_HOME} -type d -exec chmod 750 {} \;
find ${PRG_HOME} -type f -exec chmod 640 {} \;
find ${PRG_HOME} -name *.php -type f -exec chmod 750 {} \;
chmod 6750 ${PRG_HOME}/bin/bbsd
