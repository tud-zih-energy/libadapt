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

/*
 * dvfs.c
 *
 *  Created on: 09.01.2012
 *      Author: rschoene
 */

#include "fastcpufreq.h"
#include <cpufreq.h>
#include "dvfs.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

/*#define VERBOSE 1 */

static unsigned int num_cpus = 0;

static struct cpufreq_policy ** saved_policies = NULL;

static int save_before_settings(void) {
    unsigned int i;
    assert(saved_policies == NULL);
    saved_policies = calloc(num_cpus, sizeof(*saved_policies));
    if (saved_policies == NULL)
      return ENOMEM;
    for (i = 0; i < num_cpus; i++) {
        saved_policies[i] = cpufreq_get_policy(i);
        if (saved_policies[i] == NULL)
        {
            free(saved_policies);
            saved_policies=NULL;
            return EACCES;
        }
    }
    return 0;
}

static int restore_before_settings() {
    for (int cpu = 0; cpu < num_cpus; cpu++) {
        int ret;
        if ((ret = cpufreq_set_policy(cpu, saved_policies[cpu]))) {
            return ret;
        }
        cpufreq_put_policy(saved_policies[cpu]);
    }
    free(saved_policies);
    return 0;
}


static long dvfs_set_freq(int64_t frequency, int32_t cpu) {
#ifdef VERBOSE
    fprintf(stderr,"adapting 1 frequency to %d %d\n",frequency, cpu);
#endif
    assert(frequency);
    if (cpu < 0) {
        cpu = sched_getcpu();
    }
    return fcf_set_frequency(cpu , frequency);
}

int init_dvfs() {
  int ret;
  num_cpus = sysconf(_SC_NPROCESSORS_CONF);
  ret = save_before_settings();
  if (ret)
      return ret;
  ret = fcf_init_once();
  return ret;

}

int fini_dvfs() {
    fcf_finalize();
    if (saved_policies) {
        restore_before_settings(-1);
    }
    return 0;
}

int dvfs_read_from_config(void * vp, struct config_t * cfg, char * buffer, 
                          char * prefix) {
  config_setting_t *setting;
  struct dvfs_information * info = vp;
  int was_set = 0;
  info->freq_before = 0;
  info->freq_after = 0;
  sprintf(buffer, "%s.%s_freq_before", prefix, DVFS_CONFIG_STRING);
  setting = config_lookup(cfg, buffer);
  if (setting) {
    info->freq_before = config_setting_get_int(setting);
#ifdef VERBOSE
    fprintf(stderr,"%s = %d \n",buffer,info->freq_before);
#endif
    was_set = 1;
  }
  sprintf(buffer, "%s.%s_freq_after", prefix, DVFS_CONFIG_STRING);
  setting = config_lookup(cfg, buffer);
  if (setting) {
    info->freq_after = config_setting_get_int(setting);
#ifdef VERBOSE
    fprintf(stderr,"%s = %d \n",buffer,info->freq_after);
#endif
    was_set = 1;
  }
  return was_set;
}

int dvfs_process_before(void * vp, int32_t cpu) {
  struct dvfs_information * info = vp;
  long ok = 0;
  if (info->freq_before == 0) {
    return 0;
  }

#ifdef VERBOSE
  fprintf(stderr,"adapting frequency to %d\n",info->freq_before);
#endif

  ok = dvfs_set_freq(info->freq_before, cpu);

  if (ok != info->freq_before) {
    fprintf(stderr,"Setting frequency failed before %li!\n",ok);
    return ok;
  }
  return 0;
}

int dvfs_process_after(void * vp, int32_t cpu) {
  struct dvfs_information * info = vp;
  long ok = 0;
  if (info->freq_after == 0) {
    return 0;
  }
#ifdef VERBOSE
  fprintf(stderr,"adapting frequency to %d\n",info->freq_after);
#endif
  ok = dvfs_set_freq(info->freq_after, cpu);

  if (ok != info->freq_after) {
    fprintf(stderr, "Setting frequency failed after %li!\n", ok);
    return ok;
  }
  return 0;
}
