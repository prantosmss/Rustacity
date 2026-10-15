#!/bin/sh

#
# Here we create empty xcconfig files if they do not exist.  Otherwise,
# the build will fail.
#
configs='Debug Release'
for config in $configs
do
   if [ ! -f $TOPLEVEL/mac/$config.xcconfig ]
   then 
      touch $TOPLEVEL/mac/$config.xcconfig
   fi
done

#
# Generate the header file for preprocessing the Info.plist
#
# It's also used for create the distribution files
#
cd ${TOPLEVEL}
mkdir -p mac/build
eval $(g++ -Wno-deprecated -stdlib=libc++ -E -dM src/Rustacity.h | awk '/#define *RUSTACITY_(VERSION|RELEASE|REVISION|MODLEVEL) /{print $2 "=" $3}')
case $CONFIGURATION in
Debug|Debug64)
    RUSTACITY_EXECUTABLE=Wrapper;;
*)
    RUSTACITY_EXECUTABLE=Wrapper;;
esac
cat >mac/build/Info.plist.h <<EOF
#define RUSTACITY_EXECUTABLE ${RUSTACITY_EXECUTABLE}
#define RUSTACITY_VERSION ${RUSTACITY_VERSION}
#define RUSTACITY_RELEASE ${RUSTACITY_RELEASE}
#define RUSTACITY_REVISION ${RUSTACITY_REVISION}
#define RUSTACITY_DIST_VERSION ${RUSTACITY_VERSION}.${RUSTACITY_RELEASE}.${RUSTACITY_REVISION}
#define RUSTACITY_INFO_VERSION ${RUSTACITY_VERSION}.${RUSTACITY_RELEASE}.${RUSTACITY_REVISION}.${RUSTACITY_MODLEVEL}
EOF

exit 0
