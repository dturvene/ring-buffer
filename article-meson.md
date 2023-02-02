---
title: "Meson Article"
---

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
* clean rule.

[meson]() has `rules` but, more accurately, uses the term `target`.  So there
is a set of meson documentation targets and regression targets...

Why Meson?
----------
See [History]() for a deep-dive into build systems; it is a rabbit hole.

The reason I have stayed with [make]() for so long is once I figured out
a recipe for the different rules, it was a simple cut-and-paste for new
projects, and the makefile hierarchy scaled up well. Essentially I was not
clear how much effort would be necessary to switch to an alternative, 
and how much efficiency it would create.

Or possibly, I was just too lazy to look at the cmake, m4, java or groovy
manuals/tutorials and learn something new.

However, [QEMU]() is moving to [meson]() and I need to use QEMU so I
decided to see what's what with meson. Additionally, there are a large number
of [meson projects](https://mesonbuild.com/Users.html) using it, notably
[libvirt](https://libvirt.org/).  This means I have a large base of complicated
`meson.build` files to study. Yes, I am aware this is *also* true for cmake,
gradle, etc.

Additionally:

* It is feature-rich and flexible but seems to focus on C/C++ development.
* It is fairly easy to learn the grammar to replicate a simple project
  Makefile.  There is a good amount of documentation and user support.
* It is developed in [python 3](https://www.python.org/), which I find to be a
  great tool language.
* It is actively managed and developed. Since starting my study the meson stable
  tip has updated from 0.61.5 to 1.0.0.  I currently develop on Ubuntu 18.04,
  distributed with the very old meson 0.45.1 release.

History
-------
The [make](https://en.wikipedia.org/wiki/Make_(software)) program was
originally developed sometime between Unix V1 (Ken Thompson circa 1969) and Unix
V6 (general released circa 1978.)  It has been enhanced a good deal, most
visibly by [GNU make](https://www.gnu.org/software/make/) and 
[ccache](https://ccache.dev/). However, it retains many of the original design
requirements and decisions. Large projects using [make](), such as the 
[Linux kernel](https://www.linuxfoundation.org/), have organically created
makefile layers to the point of almost incomprehension.

A significant make implementation *must* use external executables to
perform necessary functions outside the scope of [make]().  See the shell,
perl, python and C support programs in `$K/scripts` called by various
makefiles. To inspect a real-world rule "gone bad" rule function see the
`cmd_gensymtypes_S` function in `$K/scripts/Makefile.build`.

The state of [make]() has prompted a number of efforts to produce a "better"
software build tool without losing its features and flexibility.  I
have used a large number of these build tools as a developer and end user
including [cmake](cmake.org), gradle/groovy, maven, bazel, meson, ninja,
autotools. I am sure there are many more I have overlooked.

Of these [cmake]() is arguably the most prevalent for C/C++ development, and
the one I have used the most behind make. There is a cmake `CMakeLists.txt` in
the repo here for comparison to meson. It clearly influenced [meson]() goals
and some of its framework.  However, I did not use [cmake]() if [make]() was an
option.  I never found it to be as comprehensive as [make](). Easier? Yes.
More powerful? Sometimes. More flexible? No.  More intuitive? No.

Now layer on top of the these build tools a comparable package manager that:

1) collects the executable and supporting configuration files into a package,
2) installs the package files into a target filesystem, checking for compatible
dependencies in the target filesystem.

Package managers include dpkg/apt, yum, pacman, opkg.  I am sure there are many 
more I have overlooked.

*Then* on top of a package manager there is a framework to manage a set
of packages. There does not seem to be a common term for these guys or even a
common set of functions, but the most accurate term I have found is "Distro
Builder", which is what I will use here, with "Image Builder" as a close
second.  Possible functions for a distro builder are:

* gather a list of versioned packages, either locally or downloaded,
* gather the necessary build toolsuite *for the target architecture*, either
  locally or downloaded,
* explode each package into a scratch/work area,
* possibly install patch updates to a package,
* using the toolsuite, build the package into a build area,
* create a filesystem (possibly using `man:chroot` or ) and, using a comparable
  package manager, install each package into the filesystem.

Tools providing some/all of these functions include buildroot, google git-repo,
linux yocto, debian/canonical pbuilder/debootstrap/multistrap, openwrt image
builder. I am sure there are image builder functions I have overlooked.
I am sure there are many such tools I have overlooked.

Now that I have illustrated my view of the layers involved, let us look at
my experience with [meson]().

Comparison Framework
--------------------
I decided to start my experience with a small project I created last 
year at [github project](https://github.com/dturvene/ring-buffer)
using `C` and [GNU Make]() and documented in medium article
[Ringbuffer in C](https://medium.com/@dturvene/a-portable-ringbuffer-implementation-in-c-5349c03a9c25).

I added adding [cmake]() and [meson]() configuration files. I also added an
`rb.Dockerfile` [docker](https://www.docker.com/) configuration to more easily
manage the various python and meson dependencies and an `rb-functions.sh` file
containing shell functions to manage docker, make, cmake and meson build and
testing.

Meson Investigation
-------------------
It is immediately clear that [meson]() has benefitted from the significant
software development evolution since [make]() was created. This is apparent in
the  [meson design rationale](https://mesonbuild.com/Design-rationale.html).
Furthermore, it is solely based on [python 3]() with no other dependecies. 
The meson build files appear to be malleable and easily extendable.

One small example is automatic dependency checking, which must be hacked (in a 
number of ways) to make rules OR explicitly defined as a target prerequisite. 

[meson]() also has a comprehensive logging mechanism to review build steps AND
test steps. I have added this capabilty to my `makefiles` with
some work but it is not obvious.

The [meson]() documentation is extensive and growing.  I like the
[meson FAQ](https://mesonbuild.com/FAQ.html), 
[meson HOWTO](https://mesonbuild.com/howtox.html),
[meson in-depth tutorial](https://mesonbuild.com/IndepthTutorial.html)
pages.

Summary
-------
[meson]() is a great step forward from my casual experience but I need to
continue to research and expand my knowledge. The website has A LOT of
documentation that I still need to read.  I also need to explore the 
[ninja](https://ninja-build.org/) backend in a little more depth.

I am also starting to use [Rust](https://www.rust-lang.org/) which 
uses [cargo](https://doc.rust-lang.org/book/ch01-03-hello-cargo.html) for build 
and package manager support.
There is a [meson crate](https://docs.rs/meson/latest/meson/) but I have not
researched it yet.

Bottom line: [make]() is still used by many projects and I suppose I will
continue to have to know it, but I will use [meson]() as my preferred build
system.
