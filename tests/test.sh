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
    
    freq_path="/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies"

    # make frequencies depend on the host
    if [ -r $freq_path  ]; then
	freqs="$(cat $freq_path)"
    else
	freqs="1000"
    fi

    # count how many frequencies do we have
    i=1
    while [ "$(echo $freqs | cut -d ' ' -f $i)" != ""  ]; do
	prev=$(echo $freqs | cut -d ' ' -f $i)
	i=$(($i+1))
	next=$(echo $freqs | cut -d ' ' -f $i)

	if [ "$prev" = "$next" ]; then
	    i=$(($i-1))
	    break
	fi
    done

    # use $(print_freq) to create host specfic frequencies
    # this wouldn't work in the zsh in a subshell
    print_freq() {
	if [ $i -gt 1 ]; then
	    echo $freqs | cut -d ' ' -f $(($RANDOM % ($i - 1) + 1))
	else
	    echo $freqs
	fi
    }

    ## init
    export init_dct_threads_before=3
    export init_dct_threads_after=4
    export init_dvfs_freq_before=1600000
    # after adapt_close() the previous cpu gouvernor will be loaded and so this
    # option doesn't make any sense and we are not test for this
    init_dvfs_freq_after=2000000

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
    export bin_x86_adapt_option="x86_adapt_Intel_Clock_Modulation"
    export bin_x86_adapt_before=19
    export bin_x86_adapt_after=21

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

    # and last but not least x86_adapt
    function_3:
    {
	name="test_x86_adapt";
	# we are very variable in the settings
	${bin_x86_adapt_option}_before=$bin_x86_adapt_before;
	${bin_x86_adapt_option}_after=$bin_x86_adapt_after;

    };
};
EOF

fi
}

build_x86_adapt() {
    ## Function to build x86_adapt if necessary

    status=0
    count_modules=0

    # save original directory and go ahead
    PREV="$(pwd)"
    cd ../

    # fetch sources if we need them
    [ ! -d x86_adapt ] && git clone https://github.com/tud-zih-energy/x86_adapt.git

    # start to build
    cd x86_adapt
    if [ -d build ]; then
	cd build
	cmake --build .
	status=$(($status + $?))
    else
	mkdir build && cd build
	cmake ..
	status=$(($status + $?))
	cmake --build .
	status=$(($status + $?))
    fi

    # first module unloading because we want to use our fresh
    for module in x86_adapt_driver x86_adapt_defs; do
	if $(lsmod | grep $module -q); then
	    if $(which sudo > /dev/null 2>&1); then
		sudo rmmod $module || count_modules=$(($count_modules + 1))
		echo "unload $module"
	    else
		echo "Sudo is not available, maybe we can't unload the modules"
		rmmod $module || count_modules=$(($count_modules + 1))
		echo "unload $module"
	    fi
	fi
    done

    # and now load the modules
    # but if the modules are loaded and we are unable to unload them that's also okay
    if [ ! "$count_modules" = "2"  ]; then
	if $(which sudo > /dev/null 2>&1); then
	    sudo insmod kernel_module/definition_driver/x86_adapt_defs.ko
	    status=$(($status + $?))
	    sudo insmod kernel_module/driver/x86_adapt_driver.ko
	    status=$(($status + $?))
	else
	    insmod kernel_module/definition_driver/x86_adapt_defs.ko
	    status=$(($status + $?))
	    insmod kernel_module/driver/x86_adapt_driver.ko
	    status=$(($status + $?))
	fi
    fi

    # save path to x86_adapt and go back
    cd ../..
    export PWDX="$(pwd)"
    cd $PREV

    return $status
}

build() {
    ## Function to build the source of libadapt if necessary

    # save previous location
    prev="$(pwd)"

    status=0

    # on our test systems we can use x86_adapt
    if [[ $@ =~ "testsys" || $@ =~ "x86_adapt" ]]; then
	build_x86_adapt
	status=$(($status + $?))
    fi

    if [[ $@ =~ "self" && -d ../build  ]]; then
	# if the sources were already builded and the build dir exists we do nothing
	build_dir=1
    elif [ -d ../build ]; then
	# if the build dir exists we compile it again
	cd ../build
	cmake --build .
	build_dir=1
    else
	# if we haven't such a nice environment we make the decision for you
	echo "No build directory was found so I will try to compile on my own with decent flags"
	mkdir ../build && cd ../build
	if [[ $@ =~ "testsys" || ( $@ =~ "x86_adapt" && $@ =~ "dvfs" ) ]]; then
	    # on our testsystem we have sudo and can use cpufreq
	    cmake -DCMAKE_BUILD_TYPE=Debug -DXA_INC=$PWDX/x86_adapt/library/include -DXA_LIB=$PWDX/x86_adapt/build ../
	    status=$(($status + $?))
	elif [[ $@ =~ "x86_adapt" ]]; then
	    cmake -DCMAKE_BUILD_TYPE=Debug -DNO_CPUFREQ=yes -DXA_INC=$PWDX/x86_adapt/library/include -DXA_LIB=$PWDX/x86_adapt/build ../
	    status=$(($status + $?))
	elif [[ $@ =~ "dvfs" ]]; then
	    cmake -DCMAKE_BUILD_TYPE=Debug -DNO_X86_ADAPT=yes ../
	    status=$(($status + $?))
	else
	    cmake -DCMAKE_BUILD_TYPE=Debug -DNO_X86_ADAPT=yes -DNO_CPUFREQ=yes ../
	    status=$(($status + $?))
	fi
	cmake --build .
	# clean build dir
	build_dir=0
    fi

    # change to the directory where we was before the build starts
    cd $prev

    return $status
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
    # file for x86_adapt if necessary
    if [ -d ../x86_adapt ]; then
	cp ../x86_adapt/library/include/x86_adapt.h .
	cp ../x86_adapt/build/libx86_adapt.so .
    fi
}

compile() {
    ## Function to compile the test program

    status=0

    if [ -f test.c ]; then
	# compile source
	echo; echo "Compile the test progam"; echo "======================="
	# for use the static library we need to compile with -Wl,--no-as-needed
	# otherwise we get undefined reference errors from __dlerror and __dlsym
	#gcc -Wl,--no-as-needed -ldl test.c libadapt_static.a -fopenmp -o test
	# so we use the dynamic linked one
	if [[ $@ =~ "testsys" || $@ =~ "x86_adapt" ]]; then
	    echo "Compile with x86_adapt testing!"
	    gcc test.c -L. -ladapt -lx86_adapt -fopenmp -DX86_ADAPT -o test
	else
	    gcc test.c -L. -ladapt -fopenmp -o test
	fi
	status=$(($status + $?))
    else
	# error if we have not source for our test
	echo "I didn't find the source code for my test program. So, I will do nothing"
	status=1
    fi

    return $status
}

run() {
    ## Function to run the test program

    # print a nice info
    echo; echo "Running the test progam"; echo "======================="
    # only use sudo if it's possible
    if [ $(which sudo > /dev/null 2>&1) -a $(which taskset > /dev/null 2>&1) ]; then
	# LD_LIBRARY_PATH is necessary if we used dynamic linked library
	# give all parameters passed to the function to the test program
	sudo ADAPT_CONFIG_FILE="$(pwd)/example_config" LD_LIBRARY_PATH="$(pwd)/" taskset -c 0 ./test $bin_file_name $bin_x86_adapt_option "$@"
	status=$(($status + $?))
    elif $(which sudo > /dev/null 2>&1); then
	echo "Taskset is needed for a successfully dvfs testing"
	sudo ADAPT_CONFIG_FILE="$(pwd)/example_config" LD_LIBRARY_PATH="$(pwd)/" ./test $bin_file_name $bin_x86_adapt_option "$@"
	status=$(($status + $?))
    else
	# info if we have no sudo
	echo "Sudo is not available. So need the right permissions to set the frequency"
	ADAPT_CONFIG_FILE="$(pwd)/example_config" LD_LIBRARY_PATH="$(pwd)/" ./test $bin_file_name $bin_x86_adapt_option "$@"
	status=$(($status + $?))
    fi
    echo "======================="; echo "Done"; echo

    return $status
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
	rm *.log
    fi
    # and the same for the config
    if [ "$1" = "conf" -o "$2" = "conf"  ]; then
	rm example_config
    fi
    # remove build dir, if we have create it
    if [ $build_dir -eq 0 ]; then
	rm -r ../build
    fi
    # and everythin from x86_adapt
    [ -f libx86_adapt.so ] && rm libx86_adapt.so
    [ -f x86_adapt.h ] && rm x86_adapt.h
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

    return $status
}


## Main
# get some commands together to reduce the complexity
until_run_no_output() {
    config >/dev/null 2>&1 && \
    build $@ >/dev/null 2>&1 && \
    prepare >/dev/null 2>&1 && \
    compile $@ >/dev/null 2>&1
}

until_run_logs() {
    config >config.log && \
    build $@ >build.log && \
    prepare >prepare.log && \
    compile $@ >compile.log
}

until_run_verbose() {
    config && \
    build $@ && \
    prepare && \
    compile $@
}

# start with the command line option parsing
if [ "$1" = "" ]; then
    # without a parameter we use only tests that should work on every computer and
    # pipe the output to /dev/null
    # so only the fail or sucess of the evaluate() is printed
    until_run_no_output $@ && \
    run machine dct file >run.log 2>/dev/null && \
    evaluate run.log  && \
    clean log conf
elif [ "$1" = "travis" ]; then
    # if we use travis we have no abillity to change the frequency
    # we want a full output and didn't need to clean up
    # evaluate and full output need a little trick to work properly
    until_run_verbose $@ && \
    run verbose dct file && \
    echo -n "" > $bin_file_name && \
    run verbose dct file > run.log 2>/dev/null && \
    evaluate run.log 
elif [ "$1" = "testsys" ]; then
    # on a test system we create logs to proof them afterwards
    until_run_logs $@ && \
    run verbose $@ >run.log 2>run_error.log && \
    evaluate run.log && \
    clean
elif [ "$1" = "log" ]; then
    # if the user want some logs we create them
    until_run_logs $@ && \
    run verbose $@ >run.log 2>run_error.log && \
    evaluate run.log && \
    clean
elif [ "$1" = "verbose" ]; then
    # with verbose you get a full output with a little trick we can also use evalute()
    until_run_verbose $@ && \
    run $@ && \
    echo -n "" > $bin_file_name && \
    run $@ >run.log 2>/dev/null && \
    evaluate run.log && \
    clean log
elif [ "$1" = "self" ]; then
    # don'r recompile the sources in ../build
    until_run_no_output $@ && \
    run machine $@ > run.log 2>/dev/null && \
    evaluate run.log  && \
    clean log conf
else
    # with other parameters the user get a minimal output and can chose the tests
    until_run_no_output $@ && \
    run machine $@ > run.log 2>/dev/null && \
    evaluate run.log  && \
    clean log conf
fi

# return sucess or fail
exit $status

