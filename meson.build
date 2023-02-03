# meson build file for the linux C ringbuffer project
# See rb.Dockerfile and rb-functions.sh

# https://mesonbuild.com/Builtin-options.html#core-options
project('simple ringbuffer', 'c',
  default_options: 'buildtype=debugoptimized',
  version : '1.0',
  license : 'MIT',
  )

''' python comment, remove because set in buildtype
# https://mesonbuild.com/Builtin-options.html
# default buildtype=debug so this is unnecessary
add_global_arguments('-g', language : 'c')
# default buildtype=debugoptimized so this is unnecessary
add_global_arguments('-O2', language : 'c')
'''

# debug_log = get_option('debug_logging')

# https://mesonbuild.com/IndepthTutorial.html
# Add preprocessor arg to enable event logging
# this adds a significant amount of time to capture and dump logs
add_global_arguments('-DDEBUG_LOGGING', language : 'c')

# https://mesonbuild.com/FAQ.html#what-is-the-correct-way-to-use-threads-such-as-pthreads
pthread_dep = dependency('threads')

src = ['ringbuffer.c', 'logevt.c']

# build the target executable
rb = executable('ringbuffer', sources : src, dependencies : pthread_dep)

'''
comment out, use pandoc-html.py for doc generation
# target github markdown doc build to HTML for review
# https://mesonbuild.com/Reference-manual_functions.html#custom_target
indoc = 'README.md'
outdoc = 'README.html'
doctarget = custom_target('README',
	  output : outdoc,
	  input : indoc,
	  command : ['/usr/bin/pandoc', '-f', 'markdown', '-s', '@INPUT@', '-o', '@OUTPUT@'],
	  install : true,
	  install_dir : 'subdir')
'''

# build the github README markdown and meson article for draft review
# meson compile doc-readme
# https://mesonbuild.com/Run-targets.html
run_target('doc-readme', command : ['scripts/pandoc-html.py', 'README'] )
# change article name due to Medium import screwiness
run_target('doc-meson', command : ['scripts/pandoc-html.py', 'article-meson-v2'] )

# ringbuffer regression tests
# linux> meson test
test('validation test', rb, args : ['-t 1'])
test('simple stress', rb, args : ['-t 2'])
test('large stress using spinlock for critical section', rb, args : ['-t 3', '-c 10000000'])
test('large stress using pthread mutex for critical section', rb, args : ['-t 3', '-c 10000000', '-m'])
