# meson build file for the linux C ringbuffer project
# See rb.Dockerfile and rb-functions.sh

# https://mesonbuild.com/Builtin-options.html#core-options
project('simple ringbuffer', 'c',
  default_options: 'buildtype=debugoptimized',
  version : '1.0',
  license : 'MIT',
  )

''' comment out
# https://mesonbuild.com/Builtin-options.html
# default buildtype=debug so this is unnecessary
add_global_arguments('-g', language : 'c')
# default buildtype=debugoptimized so this is unnecessary
add_global_arguments('-O2', language : 'c')
'''

# https://mesonbuild.com/IndepthTutorial.html
# Add preprocessor arg to enable event logging
# this adds a significant amount of time to capture and dump logs
add_global_arguments('-DDEBUG_LOGGING', language : 'c')

# https://mesonbuild.com/FAQ.html#what-is-the-correct-way-to-use-threads-such-as-pthreads
pthread_dep = dependency('threads')

src = ['ringbuffer.c', 'logevt.c']

# build the target executable
rb = executable('ringbuffer', sources : src, dependencies : pthread_dep)

# target github markdown doc build to HTML for review
# https://mesonbuild.com/Reference-manual_functions.html#custom_target
doctarget = custom_target('README',
	  output : 'README.html',
	  input : 'README.md',
	  command : ['/usr/bin/pandoc', '-f', 'markdown', '-s', '@INPUT@', '-o', '@OUTPUT@'],
	  install : true,
	  install_dir : 'subdir')

# ringbuffer regression tests
test('validation test', rb, args : ['-t 1'])
test('simple stress', rb, args : ['-t 2'])
test('large stress using spinlock for critical section', rb, args : ['-t 3', '-c 10000000'])
test('large stress using pthread mutex for critical section', rb, args : ['-t 3', '-c 10000000', '-m'])
