#!/bin/sh
if [ -z "$1" ]; then
   ARCH=host
else
   ARCH=$1
fi

if [ -z "$BUILDTYPE" ]; then
	bt=Release
  S=
else
  bt=Debug
	S=_g
fi

tgt=libnode${S}.so

HOSTDIR=out-backup/host
OUTDIR=out-backup/$ARCH

mkdir -p $OUTDIR

build_host() {
   if ./configure && make BUILDTYPE=$bt; then 
 			mkdir -p $HOSTDIR
      cp out/$bt/obj.target/$tgt  $HOSTDIR
			cp out/$bt/mksnapshot${S}	$HOSTDIR
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
SNAPSHOT=out/$bt/mksnapshot$S
if ! make -j4 BUILDTYPE=$bt; then
   if ! [ -e  $SNAPSHOT ]; then
      echo compile failed!
      exit 1
   fi
   cp $SNAPSHOT $OUTDIR
   #always copy release version
   cp $HOSTDIR/mksnapshot out/$bt/
   if ! make BUILDTYPE=$bt; then
      echo failed in second make
      exit 1
   fi
   adb root || echo "failed to adb root " && exit 1
	 OK=$(adb shell "test -e /data/libc++_shared.so && echo ok")
   if [ "$OK" != "ok" ]; then
      adb push $NDK/std-toolchains/android-14-arm-libc++/arm-linux-androideabi/lib/armv7-a/libc++_shared.so /data
   fi
	 adb push $OUTDIR/mksnapshot /data
	 adb shell "export LD_LIBRARY_PATH=/data:/system/lib;cd /data;./mksnapshot --startup_src=snapshot.cc;exit"
   adb pull /data/snapshot.cc out/$bt/obj.target/v8_snapshot/geni/
	 touch out/$bt/obj.target/v8_snapshot/geni/snapshot.cc
	 if ! make BUILDTYPE=$bt;then 
      echo make failed
      exit 1
   fi
   cp out/$bt/obj.target/$tgt $OUTDIR
	 if [ -d out.$ARCH ]; then
      rm -rf out.$ARCH
      mv out out.$ARCH
   fi
fi

