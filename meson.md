Experience Migrating Away from the Make Build System
====================================================

This article explores using [meson](https://mesonbuild.com/) to replace
[make](https://en.wikipedia.org/wiki/Make_(software)) for building artifacts
from source code.  The artifacts include: executable programs, link libraries,
documentation, and test cases.

For a simple, but real-world experiment I used my
github repo at [Github: ringbuffer](https://github.com/dturvene/ring-buffer),
converting the `Makefile` to a `meson.build` file.

Abstract
--------
The [make](https://en.wikipedia.org/wiki/Make_(software)) program was
originally developed sometime between Unix V1 (Ken Thompson in 1969) and Unix
V6 (released to the world in 1978.)  It has been enhanced a good deal, most
visibly by [GNU make](https://www.gnu.org/software/make/), but still retains
many of the original design decisions.

Large projects using `make`, such as the 
[Linux kernel](https://www.linuxfoundation.org/), have organically created
makefile layers to the point of almost incomprehension.  I give you an example
from `$K/scripts/Makefile.build`:

```
# .S file exports must have their C prototypes defined in asm/asm-prototypes.h
# or a file that it includes, in order to get versioned symbols. We build a
# dummy C file that includes asm-prototypes and the EXPORT_SYMBOL lines from
# the .S file (with trailing ';'), and run genksyms on that, to extract vers.
#
# This is convoluted. The .S file must first be preprocessed to run guards and
# expand names, then the resulting exports must be constructed into plain
# EXPORT_SYMBOL(symbol); to build our dummy C file, and that gets preprocessed
# to make the genksyms input.
#
# These mirror gensymtypes_c and co above, keep them in synch.
cmd_gensymtypes_S =                                                         \
    (echo "\#include <linux/kernel.h>" ;                                    \
     echo "\#include <asm/asm-prototypes.h>" ;                              \
    $(CPP) $(a_flags) $< |                                                  \
     grep "\<___EXPORT_SYMBOL\>" |                                          \
     sed 's/.*___EXPORT_SYMBOL[[:space:]]*\([a-zA-Z0-9_]*\)[[:space:]]*,.*/EXPORT_SYMBOL(\1);/' ) | \
    $(CPP) -D__GENKSYMS__ $(c_flags) -xc - |                                \
    $(GENKSYMS) $(if $(1), -T $(2))                                         \
     $(patsubst y,-R,$(CONFIG_MODULE_REL_CRCS))                             \
     $(if $(KBUILD_PRESERVE),-p)                                            \
     -r $(firstword $(wildcard $(2:.symtypes=.symref) /dev/null))
```

Furthermore, a significant make implementation relies on the compiler and other
executables to perform a number of functions; see the shell, perl, python and C
programs in `$K/scripts`.

This need has prompted a number of efforts to make a "better" make without
losing its features and flexibility.  [cmake](https://cmake.org/) and 
[automake](https://www.gnu.org/software/automake/)
are termed a `meta-builder`, that
take a simplified syntax and creates a Makefile as output.

I used this example to learn the [meson](https://mesonbuild.com/) build
system.  It, among many others, is gradually replacing
 to build software
projects.  Meson is being incorporated into a number of projects, including my
favorite [QEMU](https://www.qemu.org/).  It was fairly easy to learn the
grammar to replicate the project Makefile; simple, albeit, but useful.

Meson is a python package, so follow the instructions to install it. I used a
docker ubuntu 18.04 image but will change soon.

```
linux> meson setup builddir & cd builddir
linux> meson compile -v
linux> meson test
```

The `test` command should successfully run through four increasingly complex
tests of the ringbuffer.

*More to come....*
