#!/bin/sh
if [ -z "$1" ]; then
   ARCH=host
else
   ARCH=$1
fi

HOSTDIR=out-backup/host
OUTDIR=out-backup/$ARCH

mkdir -p $OUTDIR

build_host() {
   if ./configure && make; then 
 			mkdir -p $HOSTDIR
      cp out/Release/obj.target/libnode.so  $HOSTDIR
			cp out/Release/mksnapshot	$HOSTDIR
	 fi
}


if [ "$ARCH" = "host" ] ; then
	build_host
  exit 0
fi

if ! [ -e $HOSTDIR/mksnapshot ]; then
		build_host
fi

if ./android-configure $NDK $ARCH ; then
   sed -ibak -n 's/-rdynamic\s*$/-rdynamic -pie/;p' out/deps/v8/src/mksnapshot.target.mk
else
   echo failed to configure
   exit 1
fi

if [ "$ARCH" = "x86" ]; then
   UNAME=i686
else
	UNAME=$ARCH
fi

if ! adb shell "uname -a"|grep -qe $UNAME;  then 
   echo emulator for $ARCH is not running!
   exit 1
fi
SNAPSHOT=out/Release/mksnapshot
if ! make -j4; then
   if ! [ -e  $SNAPSHOT ]; then
      echo compile failed!
      exit 1
   fi
   cp $SNAPSHOT $OUTDIR
   cp $HOSTDIR/mksnapshot out/Release/
   if ! make; then
      echo failed in second make
      exit 1
   fi
   adb root
	 adb push $OUTDIR/mksnapshot /data
	 adb shell "cd /data;./mksnapshot --startup_src=snapshot.cc;exit"
   adb pull /data/snapshot.cc out/Release/obj.target/v8_snapshot/geni/
	 touch out/Release/obj.target/v8_snapshot/geni/snapshot.cc
	 if ! make;then 
      echo make failed
      exit 1
   fi
   cp out/Release/obj.target/libnode.so $OUTDIR
fi

