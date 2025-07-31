set -x
SOURCEDIR=/home/wwn/cloudberrydb
DATADIR=$SOURCEDIR/gpAux/gpdemo

cd $DATADIR

# as gpbasebackup
gpstop -a
mkdir $DATADIR/drdir
cp -rf $DATADIR/datadirs/standby $DATADIR/drdir/
cp -rf $DATADIR/datadirs/dbfast_mirror1/ $DATADIR/drdir/
gpstart -a


# configure archive
#FIXME: we need a sync method give the fresh StandbySnapshot to DR
gpconfig -c max_standby_streaming_delay -v 1000;

mkdir -p $DATADIR/archive_dir/qd
mkdir -p $DATADIR/archive_dir/dbfast1

pushd datadirs/qddir/demoDataDir-1/
echo "archive_mode=on" >> postgresql.conf
echo "wal_level=replica" >> postgresql.conf
echo "archive_command = 'cp %p $DATADIR/archive_dir/qd/%f'" >> postgresql.conf
popd

pushd datadirs/dbfast1/demoDataDir0/
echo "archive_mode=on" >> postgresql.conf
echo "wal_level=replica" >> postgresql.conf
echo "archive_command = 'cp %p $DATADIR/archive_dir/dbfast1/%f'" >> postgresql.conf
popd
gpstop -ar

# configure restore
pushd $DATADIR/drdir/dbfast_mirror1/demoDataDir0
echo "hot_standby=on" >> postgresql.conf
echo "restore_command='cp $DATADIR/archive_dir/dbfast1/%f %p'" >> postgresql.conf
echo "/home/wwn/cbdb/bin/postgres \"-D\" \"/home/wwn/cloudberrydb/gpAux/gpdemo/drdir/dbfast_mirror1/demoDataDir0\" \"-p\" \"8003\" \"-c\" \"gp_role=execute\"" > postmaster.opts
echo "" > postgresql.auto.conf
popd

pushd $DATADIR/drdir/standby
echo "hot_standby=on" >> postgresql.conf
echo "hot_dr=on" >> postgresql.conf
echo "restore_command='cp $DATADIR/archive_dir/qd/%f %p'" >> postgresql.conf
echo "/home/wwn/cbdb/bin/postgres \"-D\" \"/home/wwn/cloudberrydb/gpAux/gpdemo/drdir/standby\" \"-p\" \"8001\" \"-c\" \"gp_role=dispatch\"" > postmaster.opts
echo "" > postgresql.auto.conf

#reset dbid
echo "gp_dbid=1"> internal.auto.conf
popd

#manual populate segmentconfigfile
pushd $DATADIR/drdir/standby
echo "1 -1 p p s u 8001 ubuntu ubuntu" > gpsegconfig_dump
echo "2 0 p p s u 8003 ubuntu ubuntu" >> gpsegconfig_dump
popd

#start dr
pg_ctl restart -D $DATADIR/drdir/standby
pg_ctl restart -D $DATADIR/drdir/dbfast_mirror1/demoDataDir0
