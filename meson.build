# meson build file for the linux C ringbuffer project
# See rb.Dockerfile and rb.sh

# To build and test
#  meson compile --clean: clean all artifacts
#  meson compile -v : build showing compiler calls
#  meson test: run all `test` calls

# delete/create ./builddir

# https://mesonbuild.com/Builtin-options.html#core-options
project('simple ringbuffer', 'c',
  default_options: 'buildtype=debugoptimized',
  version : '1.0',
  license : 'MIT',
  )

# https://mesonbuild.com/Builtin-options.html
# default buildtype=debug so this is unnecessary
# add_global_arguments('-g', language : 'c')

# https://mesonbuild.com/IndepthTutorial.html
add_global_arguments('-DLOG_EVENT', language : 'c')

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

'''
# commented out but left for info purposes
# another way to run an external command
# worse than custom_target because will run even for 'clean'
indoc = join_paths(meson.current_source_dir(), 'README.md')
outdoc = join_paths(meson.current_build_dir(), 'README.html')
message('pandoc @0@ to @1@'.format(indoc, outdoc))
r = run_command('/usr/bin/pandoc', '--metadata', 'pagetitle="A Simple Ringbuffer in C"', '-f', 'markdown', '-s', indoc, '-o', outdoc, check: true)
output = r.stdout().strip()
errtxt = r.stderr().strip()
message('README pandoc output=:@0@: err=:@1@:'.format(output, errtxt))
'''

# ringbuffer tests
test('validation test', rb, args : ['-t 1'])
test('simple stress', rb, args : ['-t 2'])
test('large stress using spinlock for critical section', rb, args : ['-t 3', '-c 10000000'])
test('large stress using pthread mutex for critical section', rb, args : ['-t 3', '-c 10000000', '-m'])
