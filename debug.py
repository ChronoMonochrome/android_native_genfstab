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
CC="gcc"
CPP="g++"
CFLAGS="-O2 -std=c++11 -fomit-frame-pointer"
LDFLAGS="-lc -lm -lgcc -nostdlib  stubstart.S"
files=os.listdir("./")
cpp_files=[i for i in files if os.path.splitext(i)[-1] == ".cpp"]
c_files=[i for i in files if os.path.splitext(i)[-1] == ".c"]

if cpp_files:
  for i in cpp_files:
    os.system("%s -o %s -c %s %s"%(CPP, os.path.splitext(i)[0]+".o", i, CFLAGS))

if c_files:
  for i in c_files:
    os.system("%s -o %s -c %s %s"%(CC, os.path.splitext(i)[0]+".o", i, CFLAGS))

o_files=""

for i in files:
  if os.path.splitext(i)[-1] == ".o":
    o_files+=" %s" % i

if o_files:
  os.system("%s -o %s %s %s"%(CPP, APPNAME, o_files, LDFLAGS))

#os.system("%s -o main.o -c main.cpp %s"%(CPP, CFLAGS))
#os.system("%s -o main main.o %s"%(CPP, LDFLAGS))
