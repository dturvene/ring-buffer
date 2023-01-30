#!/bin/bash
# Ringbuffer bash functions to automated management tasks.
#
# Run with no arguments shows available functions:
#  docker_c: create the $IMG_TAG container image
#  docker_r: launch the $IMG_TAG with desired arguments
#  docker_t: confirm program versions in image

#  The remain assume inside docker container:
#  run_makefile: clean and build program, doc and run test suite using GNU make
#  run_makefile_alt: make program using an alternative makefile
#  run_meson_init: remove existing meson build area and create new
#  run_meson_bld: clean and build program, doc and run test suite using meson

# NOTE: A lot of this is boilerplate

dstamp=$(date +%y%m%d)
tstamp='date +%H%M%S'
default_func=usage

# Defaults for options
M_DEF="placeholder"

usage() {

    printf "\nUsage: $0 [options] func [func]+"
    printf "\n Valid opts:\n"
    printf "\t-e M_DEF: override env var (default ${M_DEF})\n"
    printf "\n Valid funcs:\n"

    # display available function calls
    typeset -F | sed -e 's/declare -f \(.*\)/\t\1/' | grep -v -E "usage|parseargs"
}

parseargs() {

    while getopts "e:h" Option
    do
	case $Option in
	    e ) M_DEF=$OPTARG;;
	    h | * ) usage
		exit 1;;
	esac
    done

    shift $(($OPTIND - 1))
    
    # Remaining arguments are the functions to eval
    # If none, then call the default function
    EXECFUNCS=${@:-$default_func}
}

t_prompt() {

    printf "Continue [YN]?> " >> /dev/tty; read resp
    resp=${resp:-n}

    if ([ $resp = 'N' ] || [ $resp = 'n' ]); then 
	echo "Aborting" 
        exit 1
    fi
}

trapexit() {

    echo "Catch Ctrl-C"
    t_prompt
}

# check if inside a container
in_container()
{
    echo "Confirm in a docker container"
    
    grep -q docker /proc/1/cgroup
    if [ $? != 0 ]; then
	echo "NOT IN CONTAINER"
	exit -1
    fi

    if [ ! -f /.dockerenv ]; then
	echo "NOT IN CONTAINER"
	exit -1
    fi

}

###########################################
# Setup
###########################################
# . ~/bin/template.sh set_env
set_env() {

    # Call ". $0 $FUNCNAME"

    echo "Loading additional $0 environment variables"

    export M_DYN="dynamic place holder"
    export M_TOP="$PWD"
    
    printenv | egrep "^M_"
}

###########################################
# Operative functions
###########################################
init() {

    # generic initialization function, change as necessary
    echo "placeholder"

}

# Create docker image
docker_c()
{
    DFILE=rb.Dockerfile
    if [ -z $IMG_TAG ]; then
	echo "No Docker Image Tag set"
	exit
    fi

    echo "$PWD: build Docker=$DFILE with IMG_TAG=$IMG_TAG"
    t_prompt
    docker build -f $DFILE -t $IMG_TAG .

    # check images
    docker images
    
    # if desired, prune intermediate images to clean up
    # docker image prune -f

    # check running images
    docker ps -aq
}

# Launch docker image
docker_r()
{
    echo "$PWD: run IMG_TAG=$IMG_TAG using $PWD as /home/work"
    if [ -z $IMG_TAG ]; then
	echo "No Docker Image Tag set"
	exit
    fi

    # blow away and rebuild
    # docker rmi $IMG_TAG

    # mount the local drive as /home/work
    #  --rm: remove container on exit
    #  -i: interactive, keep STDIN open
    #  -t: allocate a psuedo-tty
    docker run \
	   --volume="$PWD:/home/work" \
	   --workdir=/home/work \
	   --rm -it $IMG_TAG

}

# confirm versions of necessary programs, esp. meson
docker_t()
{
    python --version
    # 3.7.3

    pip --version
    # 22.3.1

    # locally installed meson package
    export PATH=/home/user1/.local/bin:$PATH
    meson --version
    # 1.0.0
}

# clean and build program, doc and run test suite using GNU make
run_makefile()
{
    in_container
    
    cd /home/work
    make clean
    make
    make README.html
    make test
}

# make program using an alternative makefile
run_makefile_alt()
{
    in_container
    
    cd /home/work
    make -f Makefile.depsubdir clean
    make -f Makefile.depsubdir
    ls -l .deps
}

# remove existing  meson build area and create new
run_meson_init()
{
    in_container
    
    cd /home/work
    rm -rf ./builddir
    meson setup builddir
}

# clean and build program, doc and run test suite using meson
run_meson_bld()
{
    in_container

    if [ ! -d /home/work/builddir ]; then
	echo "call run_meson_init"
	exit -1
    fi

    cd /home/work/builddir

    meson compile --clean
    meson compile -v
    meson test
}

###########################################
#  Main processing logic
###########################################
trap trapexit INT

parseargs $*

for func in $EXECFUNCS
do
    eval $func
done

