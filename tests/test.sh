#!/bin/bash
# execute tests


build() {
    # create necessary files
    prev="$(pwd)"
    if [ "$1" = "self" -a -d ../build  ]; then
	build_dir=1
    elif [ -d ../build ]; then
	cd ../build
	cmake --build .
	build_dir=1
    else
	echo "No build directory ws found so I will try to compile on my own with decent flags"
	cd ..
	mkdir build
	cd build
	if [ "$1" = "testsys" -o "$2" = "testsys"  ]; then
	    cmake -DCMAKE_BUILD_TYPE=Debug -DNO_X86_ADAPT=yes ../
	else
	    cmake -DCMAKE_BUILD_TYPE=Debug -DNO_X86_ADAPT=yes -DNO_CPUFREQ=yes ../
	fi
	cmake --build .
	build_dir=0
    fi
    cd $prev
}

prepare() {
    # copy and create necessary files
    cp ../include/adapt.h .
    cp ../build/libadapt_static.a .
    cp ../build/libadapt.so .
    touch test_file
}

compile() {
    if [ -f test.c ]; then
	# compile source
	echo; echo "Compile the test progam"; echo "======================="
	# for use the static library we need to compile with -Wl,--no-as-needed
	# otherwise we get undefined reference errors from __dlerror and __dlsym
	#gcc -Wl,--no-as-needed -ldl test.c libadapt_static.a -fopenmp -o test
	# so we use the dynamic linked one
	gcc test.c -L. -ladapt -fopenmp -o test
    else
	echo "I didn't find the source code for my test program. So, I will do nothing"
	status=1
    fi
}

run() {
    # and execute program
    echo; echo "Running the test progam"; echo "======================="
    # only if sudo if it's possible
    # LD_LIBRARY_PATH is necessary if we used dynamic linked library
    if $(which sudo > /dev/null 2>&1); then
	sudo ADAPT_CONFIG_FILE="$(pwd)/example_config" LD_LIBRARY_PATH="$(pwd)/../build/" ./test "$@"
	status="$?"
    else
	echo "Sudo is not avaible. So need the right permissions to set the frequency"
	ADAPT_CONFIG_FILE="$(pwd)/example_config" LD_LIBRARY_PATH="$(pwd)/../build/" ./test "$@"
	status="$?"
    fi
    echo "======================="; echo "Done"; echo
}

clean() {
    # delete all unecessary files
    rm test
    rm test_file
    rm adapt.h
    rm libadapt*
    if [ "$1" != "no_log" ]; then
	rm *.log || status=1
    fi
    # remove build dir, if we have create it
    if [ $build_dir -eq 0 ]; then
	rm -r ../build
    fi
}

if [ "$1" = "travis" ]; then
    # if we use travis we have no abillity to change the frequency
    build self && prepare && compile && run quiet file dct && clean no_log
elif [ "$1" = "testsys" ]; then
    build $1 && prepare && compile && run verbose > run.log 2> run_error.log && clean no_log
elif [ "$1" = "no_log" ]; then
    build && prepare && compile && run verbose all && clean no_log
else
    build > build.log && prepare > prepare.log && compile > compile.log && run quiet file dct > run.log 2> run_error.log && clean no_log
fi

exit $status
