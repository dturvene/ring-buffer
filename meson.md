Experience Migrating Away from the Make Build System
====================================================

This article explores using [meson](https://mesonbuild.com/) to replace
[make](https://en.wikipedia.org/wiki/Make_(software)) for building artifacts
from source code.  The artifacts include: executable programs, link libraries,
documentation, and test cases.

For a simple, but real-world experiment I used my
github repo at [Github: ringbuffer](https://github.com/dturvene/ring-buffer),
converting the `Makefile` to a `meson.build` file.

The make requirements I attempted to achieve using meson include:

* flexible preprocessor and compiler arguments,
* compile and link rules,
* dependency file list creation so if one of the dependency files changes
  all files using it will be rebuilt,
* program documentation rule,
* program regression test rule,
* explicit clean rule.

Why Meson?
----------
See [History]() for a deep-dive into build systems; it is a rabbit hole.  I had 
to start somewhere so I chose [meson]() for the following reasons:

* It is feature-rich and flexible but seems to focus on C/C++ development.

* It is being incorporated into a number of projects, including my favorite
  [QEMU](https://www.qemu.org/).

* It is fairly easy to learn the grammar to replicate a simple project
  Makefile.  There is a good amount of documentation and user support.
  
* It is developed in Python3, which I find to be a great tool language.

* It is actively managed and developed. Since starting my study the meson stable
  tip has updated from 0.45 to 0.61.5 to 1.0.0. I am using a docker debian image
  (see `rb.Dockerfile`) to more easily manage the cohesive packages supporting
  [meson]().

History
-------
The [make](https://en.wikipedia.org/wiki/Make_(software)) program was
originally developed sometime between Unix V1 (Ken Thompson in 1969) and Unix
V6 (released to the world in 1978.)  It has been enhanced a good deal, most
visibly by [GNU make](https://www.gnu.org/software/make/) and 
[ccache](https://ccache.dev/). However, it retains many of the original design
requirements and decisions. Large projects using [make](), such as the 
[Linux kernel](https://www.linuxfoundation.org/), have organically created
makefile layers to the point of almost incomprehension.

A significant make implementation *must* use external executables to
perform necessary functions outside the scope of [make]().  See the shell,
perl, python and C support programs in `$K/scripts` called by various
makefiles. See the `cmd_gensymtypes_S` make function in
`$K/scripts/Makefile.build` for example.

The state of [make]() has prompted a number of efforts to produce a "better"
software build tool without losing its features and flexibility.  I
have used a large number of these build tools as a developer and end user
including [cmake](cmake.org), gradle/groovy, maven, bazel, meson, ninja,
autotools. I am sure there are many more I have overlooked.

Layered on top of the these build tool is a comparable package manager that:

1) collects the executable and supporting configuration files into a package,
2) installs the package files into a target filesystem, checking for compatible
dependencies in the target filesystem.

Package managers include dpkg/apt, yum, pacman, opkg.  I am sure there are many 
more I have overlooked.

And *then* on top of a package manager there is a framework to manage a set
of packages. There does not seem to be a common term for these guys or even a
common set of functions, but the most accurate term I have found is "Distro
Builder", which is what I will use here, with "Image Builder" as a close
second.  Possible functions for a distro builder are:

* gather a list of versioned packages, either locally or downloaded,
* gather the necessary build toolsuite *for the target architecture*, either
  locally or downloaded,
* explode each package into a work area,
* possibly install patch updates to the package,
* using the toolsuite, build the package into a local temp area,
* create a filesystem and, using a comparable package manager, install each
  package into the filesystem.

Tools providing some/all of these functions include buildroot, google git-repo,
linux yocto, debian/canonical pbuilder/debootstrap/multistrap, openwrt image
builder. I am sure there are image builder functions I have overlooked.
I am sure there are many such tools I have overlooked.

So now that I have illustrated my view of the layers involved, let us look at
my experience with [meson]().

Make versus Meson
-----------------
The reason I have stayed with [make]() for so long is once I figured out
a recipe for the different rules, it was a simple cut-and-paste for new
projects, and the makefile hierarchy scaled up well. Essentially I was not
clear how much effort would be necessary for an alternative, and how much
efficiency it would create.  Many of the alternatives were too daunting for
me: autotools uses the M4 macro language, the apache projects uses XML
configurations.

[cmake]() is prevalent and I have used it; there is a cmake build configuration
file in here for comparison to meson.  It clearly influenced [meson]() goals
but I never found it to be as powerful as [make](). Easier? Yes.

So why start using [meson]()?
