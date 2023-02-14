# meson build file for the linux C ringbuffer project
# See rb.Dockerfile and rb-functions.sh

# https://mesonbuild.com/Builtin-options.html#core-options
# buildtype=[plain, debug, debugoptimized, release,...]
#   To see current values> meson configure
#   To change> meson setup meson_bld --wipe
project('simple ringbuffer', 'c',
  default_options: [ 'buildtype=plain' ],
  version : '1.4',
  license : 'MIT',
  )

cc = meson.get_compiler('c')

# https://mesonbuild.com/Configuration.html
# one mechanism to define build options
conf_data = configuration_data()
conf_data.set('version', '2.1')
configure_file(
    input: 'config.h.in',
    output: 'config.h',
    configuration : conf_data)

status = ['Building Ringbuffer']
status += [
       'Build Version .... : @0@'.format(meson.project_version()),
       'test_level ....... : @0@'.format(get_option('test_level')),
       '',
]

# https://mesonbuild.com/Build-options.html
# 
test_level = get_option('test_level')

# see meson_options.txt
if test_level > 2
   message('NOT USED!! Set test_level > 2')
   # https://mesonbuild.com/IndepthTutorial.html
   add_global_arguments('-DTEST_LEVEL=3', language : 'c')
else
    message('NOT USED!! Disable test_level')
endif

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

message('\n '.join(status))

# build the github README markdown and meson article for draft review
# meson compile doc-readme
# https://mesonbuild.com/Run-targets.html
run_target('doc-readme', command : ['scripts/pandoc-html.py', 'README'] )
# change article name due to Medium import screwiness
run_target('doc-meson', command : ['scripts/pandoc-html.py', 'article-meson-v2'] )

# https://mesonbuild.com/Unit-tests.html
# ringbuffer regression tests
# linux> meson test [--repeat=N]
test('validation test', rb, args : ['-t 1'])
test('simple stress', rb, args : ['-t 2', '-l'])
test('large stress spinlock for critical section', rb,
	    args : ['-t 3', '-c 10000000'])
test('large stress pthread mutex for critical section', rb,
	    args : ['-t 3', '-c 10000000', '-m'], timeout: 60)
test('large stress spinlock with event logging', rb,
	    args : ['-t 3', '-c 10000000', '-l'])
# occasionally this test timeouts at 30sec so bump it up	    
test('large stress pthread mutex with event logging', rb,
	    args : ['-t 3', '-c 10000000', '-m', '-l'], timeout: 60)
