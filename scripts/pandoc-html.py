#!/usr/bin/env python3
# create an html file from source markdown using pandoc
#
# unit test:
#   MESON_SOURCE_ROOT=/home/work MESON_BUILD_ROOT=/home/work/meson_bld ../scripts/pandoc-html.py README

import os
import sys
import subprocess
from pdb import set_trace as bp

# get the filename, should be no suffix 
fname = os.path.basename(sys.argv[1])

# set from meson.build
meson_source_root = os.environ['MESON_SOURCE_ROOT']
meson_build_root = os.environ['MESON_BUILD_ROOT']

# create file paths using fname arg
infile = os.path.join(meson_source_root, fname + '.md')
outfile = os.path.join(meson_build_root, fname + '.html')

# build command string
cmd = ['/usr/bin/pandoc', '-f' , 'markdown' , '-s', infile, '-o', outfile]
# print(f'cmd={cmd}')

# launch subprocess and collect output
# https://docs.python.org/3/library/subprocess.html#subprocess.Popen
# WARN: shell=True sometimes hangs, debug THAT later..
proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

# wait for cmd to complete with status
ret=proc.wait()

# gather stdout, stderr
output, errors = proc.communicate()

# if failed display output, otherwise copesetic
if ret:
    print(f'FAILED ret={ret}')
    print(f'output={output} errors={errors}')
