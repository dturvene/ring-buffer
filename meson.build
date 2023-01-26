# meson compile -help: 
# meson compile -v : build showing compiler calls
# meson compile --clean: clean all artifacts

# https://mesonbuild.com/Builtin-options.html#core-options
project('simple ringbuffer', 'c',
  default_options: 'buildtype=debug',
  version : '0.1',
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

# github markdown doc build to HTML for review
pandoc = find_program('gendoc.sh')
indoc = 'README.md'
outdoc = 'README.html'

doctarget = custom_target('README',
	  output : outdoc,
	  input : indoc,
	  command : [pandoc, '@INPUT@', '@OUTPUT@'],
	  install : true,
	  install_dir : 'subdir')

# ringbuffer tests

test('validation test', rb, args : ['-t 1'])
test('simple stress', rb, args : ['-t 2'])
test('large stress using spinlock for critical section', rb, args : ['-t 3', '-c 10000000'])
test('large stress using pthread mutex for critical section', rb, args : ['-t 3', '-c 10000000', '-m'])
