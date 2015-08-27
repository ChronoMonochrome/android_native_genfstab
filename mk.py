#!/usr/bin/python 

#
# Copyright (C) 2015 Shilin Victor chrono.monochrome@gmail.com
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os, sys

ROOT="/media/chrono/Other/android-ndk-r10d"
APPNAME=(os.path.abspath(".")).split("/")[-1].replace(".", "_")
NDK_PLATFORM_VER=8
CC="%s/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86/bin/arm-linux-androideabi-gcc" % ROOT
CPP="%s/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86/bin/arm-linux-androideabi-g++" % ROOT
INCLUDE="%s/platforms/android-%d/arch-arm/usr/include/"%(ROOT,NDK_PLATFORM_VER)
LIB="%s/platforms/android-%d/arch-arm/usr/lib"%(ROOT,NDK_PLATFORM_VER)
CFLAGS="""-O2 -marm -std=c++11 -fomit-frame-pointer -I %s"""%INCLUDE
LDFLAGS="%s/crtbegin_dynamic.o %s/crtend_android.o -L %s -nostdlib -lc -lm -lgcc -llog" % (LIB, LIB, LIB)
files=os.listdir("./")
cpp_files=[i for i in files if os.path.splitext(i)[-1] == ".cpp"]
c_files=[i for i in files if os.path.splitext(i)[-1] == ".c"]

if cpp_files:
  for i in cpp_files:
    os.system("%s -o %s -c %s %s"%(CPP, os.path.splitext(i)[0]+".o", i, CFLAGS))

if c_files:
  for i in c_files:
    os.system("%s -o %s -c %s %s"%(CC, os.path.splitext(i)[0]+".o", i, CFLAGS))

o_files = ""

for i in c_files + cpp_files:
    o_files+=" %s" % (os.path.splitext(i)[0] + ".o")

if o_files:
  os.system("%s -o %s %s %s"%(CPP, APPNAME, o_files, LDFLAGS))
else: print("mk.py: no O files found")

if len(sys.argv) > 1:
  if (sys.argv[1] == "push"):
    os.system("adb push %s /data/usr/%s" % (APPNAME, APPNAME))
#os.system("%s -o main.o -c main.cpp %s"%(CPP, CFLAGS))
#os.system("%s -o main main.o %s"%(CPP, LDFLAGS))
