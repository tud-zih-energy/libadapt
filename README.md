[![Build Status](https://travis-ci.org/rschoene/libadapt.png)](https://travis-ci.org/rschoene/libadapt)

# libadapt
libadapt can be used to change the hardware and software environment when certain executables are started or funtions are entered/exited. libadapt supports different types of knobs, e.g., cpu frequency scaling, concurrency throttling, hardware settings via x86_adapt and file writes.

## Note
This library will not intercept any function calls! It can be used to trigger actions, nothing more.

## Knobs
Knobs define groups of action that can be triggered by libadapt. 
### Frequency Scaling

Changes frequency and voltage via the cpufreq kernel interface and writes to /sys/devices/system/cpu/cpu<nr>/cpufreq/scaling_[governor|setspeed] These files should be accessible and writable for the user!

### Concurrency Throttling

Changes the number of OpenMP threads. This should only be used outside of parallel regions (e.g., before the region is started) The OpenMP parallel program should also be linked dynamically.

Bug:
changing the number of threads leads to a situation where some CPUs are not used anymore. However their frequency settings can still be taken into account by the OS. If the number of threads is first reduced and afterwards the frequency of the active CPUs is reduced, some CPUs still have a high frequency. The processor or OS is then free to use this high frequency also for the still active CPUs.
### Hardware Changes

Allows to change hardware prefetcher settings or C-state specifications. see https://github.com/tud-zih-energy/x86_adapt

### C state limit

Change the C-state limit by disabling certain C-states via writes to /sys/devices/system/cpu/cpu<nr>/cpuidle/state<id>/disable Deeper C-states have a higher wake-up latency but use less power. Check out these fancy papers:

Wake-up latencies for processor idle states on current x86 processors (Schoene et al.)
An Energy Efficiency Feature Survey of the Intel Haswell Processor (Hackenberg et al.)
### File Handling

Allows to write settings to a specific file, which can also be a device. E.g., write a 1 whenever a function is entered and a 0 whenever it is exited. Or write sth to /dev/cpu/0/msr at a specific offset
### Adding new Knobs
Please have a look at the adapt_internal.h documentation if you want to extend the functionality.

## The Configuration File
The configuration file defines which actions to take when a specific function is entered/exited.
It is read via libconfig. Thus the syntax is special. Here is an example:
```
default:
{
   # here can be settings that are enabled when the library is loaded
   # the encoding of the setting is stated below
}
# the first binary
# the next binary definition starts with "binary_1:"
binary_0:
{
    # the binary name can be plain
    name = "/home/user/bin/my_executable";
    # but one can also use fancy regular expressions, i.e.,
    # name = "/home/user/bin/.*"
    # this is a function called from this binary
    function_0:
    {
        # mandatory
        name="foo";
        # optional
        # change frequency when entering/exiting this function
        dvfs_freq_before= 1866000; 
        dvfs_freq_after= 1600000;
        # optional
        # change number of threads when entering/exiting this function
        dct_threads_before = 2;
        dct_threads_after = 3;
        # optional
        # change settings from x86a
        # you should check which settings are available on your system!
        x86_adapt_AMD_Stride_Prefetch_before = 1;
        x86_adapt_AMD_Stride_Prefetch_after = 0;
        # optional
        # write something to this file whenever a region is entered
        file_0:
        {
            name="/tmp/foo.log";
            before="1";
            after="0";
            # offset (int) would be an additional parameter that is not used here
        };
        # another file definition could be placed here
        # optional
        # change the deepest allowed c-state
        # 1 represents sys/devices/system/cpu/cpu<nr>/cpuidle/state1
        csl_before = 1; 
        # 1 represents sys/devices/system/cpu/cpu<nr>/cpuidle/state4
        csl_after= 4;
             
    };
    # now function_1 from binary_0 could be defined
};
# now binary_1 could be defined
```
### init, default, and function settings

- init settings are applied ONCE, when the library is loaded, before other settings are applied
- default
  * global default settings are applied at EVERY enter/exit/sample event for executables that are NOT defined in the configuration files
  * local default settings (within a binary* definition) are applied at EVERY enter/exit/sample event for the respective executable
- function settings are applied when THIS function is entered/exited/sampled

## Building
libadapt uses CMake for building. You can provide the following options to cmake:
* `-DCFG_DIR=...`, `-DCFG_INC=...`, `-DCFG_LIB=...` can be used to give cmake a hint where libconfig and its headers are installed
* `-DCPU_DIR=...`, `-DCPU_INC=...`, `-DCPU_LIB=...` can be used to give cmake a hint where libcpufreq and its headers are installed
* `-DXA_DIR=...`, `-DXA_INC=...`, `-DXA_LIB=...` can be used to give cmake a hint where libx86_adapt and its headers are installed
* `-DNO_CPUFREQ=On` if you want to build without libcpufreq support
* `-DNO_X86_ADAPT=On` if you want to build without libx86_adapt support
* 
```
mkdir build
cd build
cmake -DNO_CPUFREQ=On ..
```

## Citing
Its usage was initially presented at Ena-HPC 2013:

Integrating performance analysis and energy efficiency optimizations in a unified environment, R Schöne, D Molka - Computer Science-Research and Development, 2014, DOI:10.1007/s00450-013-0243-7
