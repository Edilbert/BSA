#!/bin/sh
#
# Copyright (c) 2017 Nils Eilers <nils.eilers@gmx.de>
# This work is free. You can redistribute it and/or modify it under the
# terms of the Do What The Fuck You Want To Public License, Version 2,
# as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
#
# test-arch.sh
# Determine supported architectures when building universal binaries
#
# The output is for expample
# -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk -arch i386 -arch x86_64
# and is to be included to CFLAGS and LDFLAGS
#
# If we're not on Mac OS X, OS X, macOS or whatever name is used today,
# just return nothing
if [ `uname` != 'Darwin' ]; then
   exit 0
fi

# Determine latest SDK
# Test if we have a working xcodebuild -showsdks
xcodebuild -showsdks >/dev/null 2>&1
if [ $? -ne 0 ]; then
   # no, fall back
   sdkpath=`find /Developer/SDKs -type d -maxdepth 1 | sort | tail -n1`
else
   sdkparam=`xcodebuild -showsdks | awk '/^$/{p=0};p; /OS X SDKs:/{p=1}' | tail -1 | cut -f3`
   sdkpath=`xcodebuild -version $sdkparam Path`
fi

# Write a dummy C program into a temporary file that is used
# as a testcase for the compiler
tempfoo=`basename $0`
TMPDIR=`mktemp -d -q /tmp/${tempfoo}.XXXXXX`
TMPFILE="$TMPDIR/dummy.c"
if [ $? -ne 0 ]; then
   echo "$0: Can't create temp file, exiting..."
   exit 1
fi
TMPBIN="$TMPDIR/a.out"
cat - > $TMPFILE << 'EOF'

int main(void)
{
   return 0;
}

EOF

# Test which architectures are supported by the given SDK
for arch in ppc ppc64 i386 x86_64; do
   cc -isysroot $sdkpath -arch $arch $TMPFILE -o $TMPBIN >/dev/null 2>&1
   if [ $? -eq 0 ]; then
      supported="$supported -arch $arch"
   fi
done

# Cleanup
rm $TMPFILE
rm -f $TMPBIN
rmdir $TMPDIR

# Output results
echo -isysroot $sdkpath $supported
