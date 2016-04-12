/***********************************************************************
 * Copyright (c) 2010-2016 Technische Universitaet Dresden             *
 *                                                                     *
 * This file is part of libadapt.                                      *
 *                                                                     *
 * libadapt is free software: you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by*
 * the Free Software Foundation, either version 3 of the License, or   *
 * (at your option) any later version.                                 *
 *                                                                     *
 * This program is distributed in the hope that it will be useful,     *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of      *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the       *
 * GNU General Public License for more details.                        *
 *                                                                     *
 * You should have received a copy of the GNU General Public License   *
 * along with this program. If not, see <http://www.gnu.org/licenses/>.*
 ***********************************************************************/

/*************************************************************/
/**
* @file adapt.h
* @brief Header File for libadapt
* @author Robert Schoene robert.schoene@tu-dresden.de
*
* libadapt
*
* @version 0.4
* 
*************************************************************/

/**
 * @mainpage 
 * @section main libadapt
 * libadapt can be used to change the hardware and software environment when
 * certain executables are started or funtions are entered/exited.
 * libadapt supports different types of knobs, e.g., cpu frequency scaling,
 * concurrency throttling, hardware settings via x86_adapt and file writes.
 * @note This library will not intercept any function calls! It can be used to
 *  trigger actions, nothing more. You can check out libomp_interpos which can
 * make use of libadapt. It interpositions several OpenMP runtimes (e.g., GNU,
 * Intel, Open64) and calls this library whenever a parallel region is entered
 * and exited and whenever the threads within the parallel region start and
 * finish). You can also use VampirTrace to make a fancy tuning-tool.
 * Just check out the VTRACE_BRANCH_API_HOOKS branch
 * @subsection building Build instructions
 * The library is built using CMake. Please see the instructions in the package README on how built it. 
 * @subsection knobs Knobs
 * @subsubsection dvfs Frequency Scaling
 * Changes frequency and voltage via the cpufreq kernel interface and writes 
 * to /sys/devices/system/cpu/cpu<nr>/cpufreq/scaling_[governor|setspeed]
 * These files should be accessible and writable for the user!
 * @subsubsection dct Concurrency Throttling
 * Changes the number of OpenMP threads. This should only be used outside of
 * parallel regions (e.g., before the region is started)
 * The OpenMP parallel program should also be linked dynamically.
 * @bug changing the number of threads leads to a situation where some CPUs are
 * not used anymore. However their frequency settings can still be taken into
 * account by the OS. If the number of threads is first reduced and afterwards
 * the frequency of the active CPUs is reduced, some CPUs still have a high
 * frequency. The processor or OS is then free to use this high frequency also
 * for the still active CPUs.
 * @subsubsection x86_adapt Hardware Changes
 * Allows to change hardware prefetcher settings or C-state specifications.
 * see https://github.com/tud-zih-energy/x86_adapt
 * @subsubsection csl C state limit
 * Change the C-state limit by disabling certain C-states via writes to 
 * /sys/devices/system/cpu/cpu<nr>/cpuidle/state<id>/disable
 * Deeper C-states have a higher wake-up latency but use less power. Check out
 * these fancy papers:
 *    -  Wake-up latencies for processor idle states on current x86 processors (Schoene et al.)
 *    -  An Energy Efficiency Feature Survey of the Intel Haswell Processor (Hackenberg et al.)
 * @subsubsection file File Handling
 * Allows to write settings to a specific file, which can also be a device.
 * E.g., write a 1 whenever a function is entered and a 0 whenever it is exited.
 * Or write sth to /dev/cpu/0/msr at a specific offset
 * @subsection add Adding new Knobs
 * Have a look at the adapt_internal.h documentation
 * @subsection call Calling libadapt
 * Have a look at the adapt.h documentation
 * @subsection cfg The Configuration File
 * The configuration file is read via libconfig. Thus the syntax is special.
 * Here is an example:
 * @code
 * init:
 * {
 *    # here can be settings that are enabled when the library is loaded
 *    # the encoding of the setting is stated below
 * }
 * # the first binary
 * # the next binary definition starts with "binary_1:"
 * binary_0:
 * {
 *     # the binary name can be plain
 *     name = "/home/user/bin/my_executable";
 *     # but one can also use fancy regular expressions, i.e.,
 *     # name = "/home/user/bin/.*"
 * 
 *     # this is a function called from this binary
 *     function_0:
 *     {
 *         # mandatory
 *         name="foo";
 *
 *         # optional
 *         # change frequency when entering/exiting this function
 *         dvfs_freq_before= 1866000; 
 *         dvfs_freq_after= 1600000;
 *
 *         # optional
 *         # change number of threads when entering/exiting this function
 *         dct_threads_before = 2;
 *         dct_threads_after = 3;
 *
 *         # optional
 *         # change settings from x86a
 *         # you should check which settings are available on your system!
 *         x86_adapt_AMD_Stride_Prefetch_before = 1;
 *         x86_adapt_AMD_Stride_Prefetch_after = 0;
 * 
 *         # optional
 *         # write something to this file whenever a region is entered
 *         file_0:
 *         {
 *             name="/tmp/foo.log";
 *             before="1";
 *             after="0";
 *             # offset (int) would be an additional parameter that is not used here
 *
 *         };
 *         # another file definition could be placed here
 *
 *         # optional
 *         # change the deepest allowed c-state
 *         # 1 represents sys/devices/system/cpu/cpu<nr>/cpuidle/state1
 *         csl_before = 1; 
 *         # 1 represents sys/devices/system/cpu/cpu<nr>/cpuidle/state4
 *         csl_after= 4;
 *              
 *     };
 *     # now function_1 from binary_0 could be defined
 * };
 * # now binary_1 could be defined
 * @endcode
 * @subsection cfg-default init, default, and function settings
 *   - init settings are applied ONCE, when the library is loaded. Only _before settings are applied!
 *   - default
 *        -# global default settings are applied at EVERY enter/exit/sample event for executables that are NOT defined in the configuration files
 *        -# local default settings (within a binary_* definition) are applied at EVERY enter/exit/sample event for the respective executable
 *   - function settings are applied when THIS function is entered/exited/sampled
 *
 */

#include <stdint.h>


#define ADAPT_OK                  0
#define ADAPT_NO_ACTUAL_ADAPT     1
#define ADAPT_ERROR_WHILE_ADAPT   2
#define ADAPT_NOT_INITITALIZED    3

/**
 * @brief Initializes library
 *
 * This will read the libadapt configuration file that is defined with the
 * environment variable ADAPT_CONFIG_FILE. It will also transfer the definition
 * to internal library structures.
 * @return 0 or ErrorCode
 */
int adapt_open(void);

/**
 * @brief Closes library
 */
void adapt_close(void);

/**
 * @brief Get an ID for a specific executable
 *
 * Use this function to get an ID for an executable. This function will check
 * whether there has been a definition in the configuration file that matches
 * the binary_name. If so, the definitions are handled and an ID is returned. 
 * @param binary_name name of the executable
 * @returns an ID for that binary that has to be passed to other library calls
 * or 0 if the executable name did not match
 */

uint64_t adapt_add_binary(char * binary_name);

/**
 * @brief Define a (region, temporal_region_id) pair
 *
 * After an executable is defined, one may define several regions within
 * the executable. These regions can later be entered and exited.
 * The programmer passes a pair of region_name and region_id. The region_id is
 * used for the enter/exit events.
 * @param binary_id the binary id for the region which is generated with
 * adapt_add_binary
 * @param rname the name of the region
 * @param rname the id of the region
 * @returns 0 if the region is registered<br>
 * 1 if the library is not initialized (see adapt_open()) or the binary is not registered (see adapt_add_binary()) or there is no configuration registered for this region
 */
int adapt_def_region(uint64_t binary_id, const char* rname, uint32_t rid);

/**
 * @brief enter a certain region and do stack handling
 * 
 * You have 2 ways to use this libraries enter and exit calls<br>
 * (1) with stacks<br>
 *   This is thread save
 *   You can use adapt_enter_stacks() and adapt_exit() so rules for enter and
 *     exit can be defined<br>
 * (2) without stacks<br>
 *   There is no stack handling, which means confusion in multithreaded applications
 *   Therefor you can only use enter without a defined exit to functions.<br>
 * 
 * @param binary_id the ID generated with adapt_add_binary()
 * @param tid ID of the current thread, you should start counting tids from 0, the maximal nr of allowed tids is defined in adapt.c
 * @param rid ID of the region that is entered. If can be defined with
 *        adapt_def_region() but does not have to be defined.
 * @param cpu the current CPU of the thread or negative (then libadapt will
 *        gather the current CPU).
 * @returns 0 if something has been set
 *          1 if nothing had been set
 *          2 if some error occured
 */
int adapt_enter_stacks(uint64_t binary_id, uint32_t tid, uint32_t rid, int32_t cpu);

/**
 * @brief enter or sample a certain region, but don't do stack handling
 * @see adapt_enter_stacks
 * 
 * @param binary_id the ID generated with adapt_add_binary()
 * @param rid ID of the region that is entered. If can be defined with
 *        adapt_def_region() but does not have to be defined.
 * @param cpu the current CPU of the thread or negative (then libadapt will
 *        gather the current CPU).
 * @returns 0 if something has been set
 *          1 if nothing had been set
 *          2 if some error occured
 */
/**
 * Use this if you have only enter handling, but no exit-handling
 */
int adapt_enter_no_stacks(uint64_t binary_id, uint32_t rid, int32_t cpu);

/**
 * @brief enter or sample a certain region with optional stack handling
 *        or exit the last region
 * @see adapt_enter_stacks
 * @see adapt_enter_no_stacks
 * @see adapt_exit
 *
 * @param binary_id the ID generated with adapt_add_binary()
 * @param tid ID of the current thread, you should start counting tids from 0, the maximal nr of allowed tids is defined in adapt.c
 * @param rid ID of the region that is entered. If can be defined with
 *        adapt_def_region() but does not have to be defined.
 * @param cpu the current CPU of the thread or negative (then libadapt will
 *        gather the current CPU).
 * @param stack_on switch for stack handling (zero means no stack handling and no thread save handling)
 * @param exit handle if we exit the region or enter (zero means we want
 *        to got in the region)
 * @returns 0 if something has been set
 *          1 if nothing had been set
 *          2 if some error occured
 */
/**
 * Use this for everything enter with optional stack and exit
 */
int adapt_enter_or_exit(uint64_t binary_id, uint32_t tid, uint32_t rid,int32_t cpu, int stack_on, int exit);

/**
 * @brief exit a specific region
 *
 * @see adapt_enter_stacks()
 * Exit a region that has been enetered before using adapt_enter_stacks()
 * @param binary_id the ID generated with adapt_add_binary()
 * @param tid ID of the current thread, you should start counting tids from 0, the maximal nr of allowed tids is defined in adapt.c
 * @param rid ID of the region that is entered. If can be defined with
 *        adapt_def_region() but does not have to be defined.
 * @param cpu the current CPU of the thread or negative (then libadapt will
 *        gather the current CPU).
 * @returns 0 if something has been set
 *          1 if nothing had been set
 *          2 if some error occured
 */
int adapt_exit(uint64_t binary_id,uint32_t tid, int32_t cpu);
