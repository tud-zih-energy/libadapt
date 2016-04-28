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
* @file adapt_internal.h
* @brief Header File for libadapts internal handling
*
* If you add a knob type here, then<br>
* (1) add the knob header in this file<br>
* (2) add the knob functions to the knobs list<br>
* (3) add the knob to the enum knobs<br>
* (4) add the knob information size to the adapt_information_size<br>
* (5) write documentation in adapt.h
* @author Robert Schoene robert.schoene@tu-dresden.de
*
* libadapt
*
* @version 0.4
* 
*************************************************************/
#ifndef ADAPT_INTERNAL_H_
#define ADAPT_INTERNAL_H_

/* if you add a knob category, include its header here */

/* Dynamic Concurrency Throttling */
#ifndef NO_DCT
#include "../knobs/dct.h"
#endif

/* x86 adapt */
#ifndef NO_X86_ADAPT
#include "../knobs/x86_adapt_items.h"
#endif

/* DVFS via sysfs */
#ifndef NO_CPUFREQ
#include "../knobs/dvfs.h"
#endif

/* c-state limit via sysfs */
#ifndef NO_CSL
#include "../knobs/c_state_limit.h"
#endif

/* write sth to a file */
#include "../knobs/file.h"

/**
 * @struct adapt_definition
 * @brief represents a knob type that can be changed via libadapt 
 */
struct adapt_definition {
  /**
   * how large is the memory buffer that should be allocated for the knob type
   */
  size_t information_size;

  /**
   * a describing name for the knob type
   */
  char * name;

  /**
   * initialize the knob type and check whether the prerequisites for the
   * knob type are fulfilled.
   * @return 0 or ErrorCode
   */
  int (*init)(void);

  /**
   * parse the configuration file and check whether there has been a definition
   * for this knob type
   * @param info a memory buffer of size information_size.
   * @param cfg the configuration file that should be parsed
   * @param buffer a string buffer of 1024 byte you can work with to avoid allocs
   * @param prefix a prefix that should be used when parsing the config file 
   * @return 1 if there had been a setting, otherwise 0
   */
  int (*read_from_config)(void * info, struct config_t * cfg, char * buffer,
                          char * prefix);

  /**
   * If a knob type provides an adaption rule for a specific region, this
   * function will be called when<br>
   * (1) the function is entered
   * (2) a sample hits the region
   * @param info a memory buffer of size information_size.
   * @param cpu the current CPU of the thread that calls this function
   * @return 0 if the changing could be processed, otherwise ErrorCode
   */
  int (*process_before)(void * info, int32_t cpu);

  /**
   * If a knob type provides an adaption rule for a specific region, this
   * function will be called when the function is exited
   * @param info a memory buffer of size information_size.
   * @param cpu the current CPU of the thread that calls this function
   * @return 0 if the changing could be processed, otherwise ErrorCode
   */
  int (*process_after)(void * info, int32_t cpu);

  /**
   * This will be called when libadapt is closed.
   * @return 0 or ErrorCode
   */
  int (*fini)(void);
};

/* definitions for knob categories */
struct adapt_definition knobs[] =
{
#ifndef NO_DCT
  {
    .information_size=sizeof(struct dct_information),
    .name="Dynamic Concurrency Throttling",
    .init=init_dct_information,
    .read_from_config=dct_read_from_config,
    .process_before=dct_process_before,
    .process_after=dct_process_after,
    .fini=NULL
  },
#endif
#ifndef NO_X86_ADAPT
  {
    .information_size=sizeof(struct x86_adapt_pref_information),
    .name="x86_adapt",
    .init=x86_adapt_init, // directly from x86_adapt.h
    .read_from_config=x86_adapt_read_from_config,
    .process_before=x86_adapt_process_before,
    .process_after=x86_adapt_process_after,
    .fini=x86_adapt_reset
  },
#endif

#ifndef NO_CPUFREQ
  {
    .information_size=sizeof(struct dvfs_information),
    .name="DVFS via cpufreq entries in sysfs",
    .init=init_dvfs,
    .read_from_config=dvfs_read_from_config,
    .process_before=dvfs_process_before,
    .process_after=dvfs_process_after,
    .fini=fini_dvfs
  },
#endif

#ifndef NO_CSL
  {
    .information_size=sizeof(struct csl_information),
    .name="C-State limit via cpuidle entries in sysfs",
    .init=csl_init,
    .read_from_config=csl_read_from_config,
    .process_before=csl_process_before,
    .process_after=csl_process_after,
    .fini=csl_fini
  },
#endif
  {
    .information_size=sizeof(struct file_information),
    .name="File access",
    .init=NULL,
    .read_from_config=file_read_from_config,
    .process_before=file_process_before,
    .process_after=file_process_after,
    .fini=NULL
  }
};

/* enum for processing */
enum knobs
{
#ifndef NO_DCT
  ADAPT_DCT,
#endif
#ifndef NO_X86_ADAPT
  X86A,
#endif

#ifndef NO_CPUFREQ
  ADAPT_DVFS,
#endif

#ifndef NO_CSL
  ADAPT_CSL,
#endif
  ADAPT_FILE,
  /* used for loops */
  ADAPT_MAX
};

/* used for allocating informations */
static size_t adapt_information_size =
#ifndef NO_DCT
    sizeof(struct dct_information)+
#endif
#ifndef NO_X86_ADAPT
    sizeof(struct x86_adapt_pref_information)+
#endif
#ifndef NO_CPUFREQ
    sizeof(struct dvfs_information)+
#endif

#ifndef NO_CSL
    sizeof(struct csl_information)+
#endif
    sizeof(struct file_information)
;

#endif /* ADAPT_INTERNAL_H_ */
