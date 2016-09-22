#!/bin/bash
#***********************************************************************
#* Copyright (c) 2010-2016 Technische Universitaet Dresden             *
#*                                                                     *
#* This file is part of libadapt.                                      *
#*                                                                     *
#* libadapt is free software: you can redistribute it and/or modify    *
#* it under the terms of the GNU General Public License as published by*
#* the Free Software Foundation, either version 3 of the License, or   *
#* (at your option) any later version.                                 *
#*                                                                     *
#* This program is distributed in the hope that it will be useful,     *
#* but WITHOUT ANY WARRANTY; without even the implied warranty of      *
#* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the       *
#* GNU General Public License for more details.                        *
#*                                                                     *
#* You should have received a copy of the GNU General Public License   *
#* along with this program. If not, see <http://www.gnu.org/licenses/>.*
#***********************************************************************

# Complete test script
# prepare everything, do everything and clean everything after all

# Values for a clean and nice output 
RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
NORMAL=$(tput sgr0)
COL=$(tput cols)

config() {
    ## Function to create the config file from the given values so that we can proof
    ## if them applied the right way
 
    ## init
    export init_dct_threads_before=3
    export init_dct_threads_after=4
    export init_dvfs_freq_before=1200000
    export init_dvfs_freq_after=2000000

    ## default
    export def_dct_threads_before=7
    export def_dct_threads_after=8
    export def_dvfs_freq_before=2200000
    export def_dvfs_freq_after=2400000

    ## tests
    export bin_dct_threads_before=5
    export bin_dct_threads_after=6
    # filename need to contain a / to get recognized from test.c
    bin_file_name="./test_file"
    bin_file_name_dirty="\"$bin_file_name\""
    export bin_file_before=0
    bin_file_before_dirty="\"$bin_file_before\n\""
    export bin_file_after=1
    bin_file_after_dirty="\"$bin_file_after\n\""
    export bin_dvfs_freq_before=1300000
    export bin_dvfs_freq_after=2500000

    ## create config
    if [ ! -f $(pwd)/example_config ]; then
	# build config with individual settings
	cat << EOF > $(pwd)/example_config
init:
{
    # no initial settings
    dct_threads_before=$init_dct_threads_before;
    dct_threads_after=$init_dct_threads_after;
    dvfs_freq_before=$init_dvfs_freq_before;
    dvfs_freq_after=$init_dvfs_freq_after;

};

default:
{
    # no default settings
    dct_threads_before=$def_dct_threads_before;
    dct_threads_after=$def_dct_threads_after;
    dvfs_freq_before=$def_dvfs_freq_before;
    dvfs_freq_after=$def_dvfs_freq_after;
};

binary_0:
{
    # settings for this binary
    name = "test";
    # first test dct
    function_0:
    {
	name="test_dct";
	# change number of threads before entering the region
	dct_threads_before=$bin_dct_threads_before;
	# change number of threads after exiting the region
	dct_threads_after=$bin_dct_threads_after;
    };

    # now we test files
    function_1:
    {
	name="test_file";
	# write something to this file whenever a region is entered
	file_0:
	{
	    name=$bin_file_name_dirty;
	    before=$bin_file_before_dirty;
	    after=$bin_file_after_dirty;
	    # offset (int) would be an additional parameter that is not used here
	};

    };

    # we shoudn't forget the frequencies
    function_2:
    {
	name="test_dvfs";
	# change frequency before and after
	dvfs_freq_before=$bin_dvfs_freq_before;
	dvfs_freq_after=$bin_dvfs_freq_after;

    };

    # and last but not least we test x86_adapt
    function_3:
    {
	name="test_x86a";
	# change settings from cc device driver
	#x86_adapt_AMD_Stride_Prefetch_before = 1;
	#x86_adapt_AMD_Stride_Prefetch_after = 0;

    };
};
EOF

fi
}

build() {
    ## Function to build the source of libadapt if necessary

    # save previous location
    prev="$(pwd)"

    if [ "$1" = "self" -a -d ../build  ]; then
	# if the sources were already builded and the build dir exists we do nothing
	build_dir=1
    elif [ "$1" = "travis" ]; then
	# on travis we need to clean up because we build previously with x86_adapt,
	# csl and cpufreq
	rm -rf  ../build && mkdir ../build && cd ../build
	# but x86_adapt, csl and cpufreq doesn't work on travis
	# so we need to disable them to get no errors
	cmake -DCMAKE_BUILD_TYPE=Debug -DNO_X86_ADAPT=yes -DNO_CPUFREQ=yes -DNO_CSL=yes ../
	cmake --build .
	# clean build dir atferwards because we create them
	build_dir=0
    elif [ -d ../build ]; then
	# if the build dir exists we compile it again
	cd ../build
	cmake --build .
	build_dir=1
    else
	# if we haven't such a nice environment we make the decision for you
	echo "No build directory was found so I will try to compile on my own with decent flags"
	mkdir ../build && cd ../build
	if [ "$1" = "testsys" -o "$2" = "testsys" ]; then
	    # on our testsystem we have sudo and can use cpufreq
	    cmake -DCMAKE_BUILD_TYPE=Debug -DNO_X86_ADAPT=yes ../
	else
	    cmake -DCMAKE_BUILD_TYPE=Debug -DNO_X86_ADAPT=yes -DNO_CPUFREQ=yes ../
	fi
	cmake --build .
	# clean build dir
	build_dir=0
    fi

    # change to the directory where we was before the build starts
    cd $prev
}

prepare() {
    ## Function to copy the files from the build directory to the test directory

    # header file for libadapt
    cp ../include/adapt.h .
    # static linked library
    #cp ../build/libadapt_static.a .
    # dynamic linked library
    cp ../build/libadapt.so .
    # test file
    touch $bin_file_name
}

compile() {
    ## Function to compile the test program

    if [ -f test.c ]; then
	# compile source
	echo; echo "Compile the test progam"; echo "======================="
	# for use the static library we need to compile with -Wl,--no-as-needed
	# otherwise we get undefined reference errors from __dlerror and __dlsym
	#gcc -Wl,--no-as-needed -ldl test.c libadapt_static.a -fopenmp -o test
	# so we use the dynamic linked one
	gcc test.c -L. -ladapt -fopenmp -o test
    else
	# error if we have not source for our test
	echo "I didn't find the source code for my test program. So, I will do nothing"
	status=1
    fi
}

run() {
    ## Function to run the test program

    # print a nice info
    echo; echo "Running the test progam"; echo "======================="
    # only use sudo if it's possible
    if $(which sudo > /dev/null 2>&1); then
	# LD_LIBRARY_PATH is necessary if we used dynamic linked library
	# give all parameters passed to the function to the test program
	sudo ADAPT_CONFIG_FILE="$(pwd)/example_config" LD_LIBRARY_PATH="$(pwd)/../build/" ./test "$@"
	# save status of the previous executed command
	status="$?"
    else
	# info if we have no sudo
	echo "Sudo is not avaible. So need the right permissions to set the frequency"
	ADAPT_CONFIG_FILE="$(pwd)/example_config" LD_LIBRARY_PATH="$(pwd)/../build/" ./test "$@"
	status="$?"
    fi
    echo "======================="; echo "Done"; echo
}

clean() {
    ## Function to clean up

    # compiled program
    rm test
    # test file
    rm $bin_file_name
    # header file
    rm adapt.h
    # libraries
    rm libadapt*
    # logs if they won't needed anymore
    if [ "$1" = "log" -o "$2" = "log"  ]; then
	rm *.log || status=1
    fi
    # and the same for the config
    if [ "$1" = "conf" -o "$2" = "conf"  ]; then
	rm example_config || status=1
    fi
    # remove build dir, if we have create it
    if [ $build_dir -eq 0 ]; then
	rm -r ../build
    fi
}

evaluate() {
    ## Function to check the stdout log if everything gone right

    # loop over all of our created variables
    for var in $(printenv | grep -e init_ -e def_ -e bin_); do
	# save the setting name
	name=$(echo $var | cut -d '=' -f 1)
	# and the correspondent value
	value=$(echo $var | cut -d '=' -f 2)
	# search in the output for the setting
	output=$(grep $name $1)
	# and save the setting from that
	cur_val=$(echo $output | cut -d '=' -f 2)
	if [ ! "$output" = "" ]; then
	    # then the setting was found in the output we can go on
	    if [ "$value" = "$cur_val" ]; then
		# nice sucesss if the values match
		printf '%s%s%*s%s' "$name" "$GREEN" $(($COL-${#name})) "[SUCCESS]" "$NORMAL"
		echo
	    else
		# and failed if they didn't match
		printf '%s%s%*s%s' "$name" "$RED" $(($COL-${#name})) "[FAILED]" "$NORMAL"
		echo
		# something went wrong if they didn't match
		status=1
	    fi
	fi
    done
}


## Main 
if [ "$1" = "" ]; then
    # without a parameter we use only tests that should work on every computer and
    # pipe the output to /dev/null
    # so only the fail or sucess of the evaluate() is printed
    config >/dev/null 2>&1 && \
    build >/dev/null 2>&1 && \
    prepare >/dev/null 2>&1 && \
    compile >/dev/null 2>&1 && \
    run machine dct file >run.log 2>/dev/null && \
    evaluate run.log  && \
    clean log conf
elif [ "$1" = "travis" ]; then
    # if we use travis we have no abillity to change the frequency
    # we want a full output and didn't need to clean up
    # evaluate and full output need a little trick to work properly
    config && \
    build travis && \
    prepare && \
    compile && \
    run verbose file dct && \
    echo -n "" > $bin_file_name && \
    run verbose file dct > run.log 2>/dev/null && \
    evaluate run.log 
elif [ "$1" = "testsys" ]; then
    # on a test system we create logs to proof them afterwards
    config >config.log && \
    build testsys >build.log && \
    prepare >prepare.log && \
    compile >compile.log && \
    run verbose ${@:2} >run.log 2>run_error.log && \
    evaluate run.log && \
    clean
elif [ "$1" = "log" ]; then
    # if the user want some logs we create them
    config >config.log && \
    build >build.log && \
    prepare >prepare.log && \
    compile >compile.log && \
    run verbose ${@:2} >run.log 2>run_error.log && \
    evaluate run.log && \
    clean
elif [ "$1" = "verbose" ]; then
    # with verbose you get a full output with a little trick we can also use evalute()
    config && \
    build && \
    prepare && \
    compile && \
    run verbose ${@:2} && \
    echo -n "" > $bin_file_name && \
    run verbose ${@:2} >run.log 2>/dev/null && \
    evaluate run.log && \
    clean log
elif [ "$1" = "self" ]; then
    # don'r recompile the sources in ../build
    config >/dev/null 2>&1 && \
    build self >/dev/null 2>&1 && \
    prepare >/dev/null 2>&1 && \
    compile >/dev/null 2>&1 && \
    run machine ${@:2} > run.log 2>/dev/null && \
    evaluate run.log  && \
    clean log conf
else
    # with other parameters the user get a minimal output and can chose the tests
    config >/dev/null 2>&1 && \
    build >/dev/null 2>&1 && \
    prepare >/dev/null 2>&1 && \
    compile >/dev/null 2>&1 && \
    run machine $@ > run.log 2>/dev/null && \
    evaluate run.log  && \
    clean log conf
fi

# return suces or fail
exit $status

