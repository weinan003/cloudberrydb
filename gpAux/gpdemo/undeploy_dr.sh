set -x
SOURCEDIR=$(pwd)/../../
DATADIR=$(pwd)

pg_ctl stop -D $DATADIR/drdir/standby
pg_ctl stop -D $DATADIR/drdir/dbfast_mirror1/demoDataDir0

rm -r $DATADIR/drdir
rm -r $DATADIR/archive_dir
