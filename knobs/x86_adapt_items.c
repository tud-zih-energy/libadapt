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
 * x86_adapt_items.c
 *
 *  Created on: 24.10.2012
 *      Author: rschoene
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <libconfig.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#include "x86_adapt_items.h"

extern int sched_getcpu(void);

int x86_adapt_can_be_used=0;

struct change{
  /* this is a bitvector telling which cpu's settings have been changed */
  uint64_t * cpus_used;
  int first;
  pthread_mutex_t mutex;
  uint64_t default_setting;
};

/* changes done to specific cpus */
static struct change * changes=NULL;

/* changes done to all cpus (e.g., /dev/x86_adapt/cpu/all) */
static struct change * changes_all=NULL;

static int nr_changes=0;

static void init_info(struct pref_setting_ids * settings, int ci_nr, int64_t setting, int nr_cpus){
  int i;
  settings->id=ci_nr;
  settings->setting=setting;
  if (ci_nr>=nr_changes){
    changes=realloc(changes,(ci_nr+1)*sizeof(struct change));
    for (i=nr_changes;i<=ci_nr;i++){
      changes[i].cpus_used=calloc((nr_cpus/64)+1,sizeof(uint64_t));
      pthread_mutex_init(& changes[i].mutex,NULL);
      changes[i].first=1;
    }
    changes_all=realloc(changes_all,(ci_nr+1)*sizeof(struct change));
    for (i=nr_changes;i<=ci_nr;i++){
      changes_all[i].cpus_used=calloc((nr_cpus/64)+1,sizeof(uint64_t));
      pthread_mutex_init(& changes_all[i].mutex,NULL);
      changes_all[i].first=1;
    }
    nr_changes=ci_nr+1;
  }
}

static inline int do_toggle(struct pref_setting_ids * settings,int fd, int cpu,struct change * cur_change){
  int ret;
  if (cur_change->first){
    pthread_mutex_lock(&cur_change->mutex);
    if (cur_change->first){
      if ( (ret=x86_adapt_get_setting(fd,settings->id,&cur_change->default_setting)) != 8 )
        return ret;
      else
        cur_change->first=0;
#ifdef DEBUG
      fprintf(stderr,"Reading initial x86_adapt is %llx on CPU %d\n",cur_change->default_setting,cpu);
#endif
    pthread_mutex_unlock(&cur_change->mutex);
    }
  }
  cur_change->cpus_used[cpu/64]|=(1ULL<<cpu%64);
  if ( (ret=x86_adapt_set_setting(fd,settings->id,settings->setting)) != 8 ){
    return ret;
  }
  return 0;
}

int x86_adapt_read_from_config(void * vp,struct config_t * cfg, char * buffer,
                               char * prefix)
{
  /* get the prefetcher names from x86_adapt lib*/
  config_setting_t *setting;
  struct x86_adapt_configuration_item  ci;
  int set=0;
  int ci_nr;
  struct x86_adapt_pref_information * info = vp;
  memset(info,0,sizeof(struct x86_adapt_pref_information));
  for (ci_nr=0;ci_nr<x86_adapt_get_number_cis(X86_ADAPT_CPU);ci_nr++){
      if (x86_adapt_get_ci_definition(X86_ADAPT_CPU, ci_nr,&ci)){
        return 0;
      }
      sprintf(buffer, "%s.%s_%s_before",
          prefix ,X86_ADAPT_PREF_CONFIG_STRING, ci.name);
      setting = config_lookup(cfg, buffer);
      if (setting){
        info->settings_before=realloc(info->settings_before,(info->nr_settings_before+1)*sizeof(struct pref_setting_ids));
        init_info(
            &info->settings_before[info->nr_settings_before],
            ci_nr,
            config_setting_get_int64(setting),
            sysconf(_SC_NPROCESSORS_CONF));
        /* add it to info */
        info->nr_settings_before++;
      }
      sprintf(buffer, "%s.%s_%s_after",
          prefix ,X86_ADAPT_PREF_CONFIG_STRING, ci.name);
      setting = config_lookup(cfg, buffer);
      if (setting){
        /* add it to info */
        info->settings_after=realloc(info->settings_after,(info->nr_settings_after+1)*sizeof(struct pref_setting_ids));
        init_info(
            &info->settings_after[info->nr_settings_after],
            ci_nr,
            config_setting_get_int64(setting),
            sysconf(_SC_NPROCESSORS_CONF));
        info->nr_settings_after++;
      }
      sprintf(buffer, "%s.%s_%s_all_before",
          prefix ,X86_ADAPT_PREF_CONFIG_STRING, ci.name);
      setting = config_lookup(cfg, buffer);
      if (setting){
        /* add it to info */
        info->settings_before_all=realloc(info->settings_before_all,(info->nr_settings_before_all+1)*sizeof(struct pref_setting_ids));
        init_info(
            &info->settings_before_all[info->nr_settings_before_all],
            ci_nr,
            config_setting_get_int64(setting),
            1);
        info->nr_settings_before_all++;
        set=1;
      }
      sprintf(buffer, "%s.%s_%s_all_after",
          prefix ,X86_ADAPT_PREF_CONFIG_STRING, ci.name);
      setting = config_lookup(cfg, buffer);
      if (setting){
        /* add it to info */
        info->settings_after_all=realloc(info->settings_after_all,(info->nr_settings_after_all+1)*sizeof(struct pref_setting_ids));
        init_info(
            &info->settings_after_all[info->nr_settings_after_all],
            ci_nr,
            config_setting_get_int64(setting),
            1);
        info->nr_settings_after_all++;
        set=1;
      }
    }
  return set;
}


int x86_adapt_process_before(void * vp, int32_t cpu)
{
  int i,fd,ret;
  struct x86_adapt_pref_information * info = vp;
  if (cpu<0)
    cpu=sched_getcpu();

  fd=x86_adapt_get_device(X86_ADAPT_CPU,cpu);

  for (i=0;i<info->nr_settings_before;i++)
    if ((ret=do_toggle(&info->settings_before[i],fd,cpu,&changes[info->settings_before[i].id]))) return ret;

  //  x86_adapt_put_device(X86_ADAPT_CPU,cpu);

  fd=x86_adapt_get_all_devices(X86_ADAPT_CPU);

  for (i=0;i<info->nr_settings_before_all;i++)
    if ((ret=do_toggle(&info->settings_before_all[i],fd,0,&changes_all[info->settings_before_all[i].id]))) return ret;
//  x86_adapt_put_all_devices(X86_ADAPT_CPU);
  return 0;
}

int x86_adapt_process_after(void * vp, int32_t cpu)
{
  int i,fd,ret;
  struct x86_adapt_pref_information * info = vp;

  if (cpu<0)
    cpu=sched_getcpu();

  fd=x86_adapt_get_device(X86_ADAPT_CPU,cpu);

  for (i=0;i<info->nr_settings_after;i++)
      if ((ret=do_toggle(&info->settings_after[i],fd,cpu,&changes[info->settings_after[i].id]))) return ret;

//  x86_adapt_put_device(X86_ADAPT_CPU,cpu);

  fd=x86_adapt_get_all_devices(X86_ADAPT_CPU);

  for (i=0;i<info->nr_settings_after_all;i++)
    if ((ret=do_toggle(&info->settings_after_all[i],fd,0,&changes_all[info->settings_after_all[i].id]))) return ret;
//  x86_adapt_put_all_devices(X86_ADAPT_CPU);

  return 0;
}

int x86_adapt_reset(){
  int i,j,fd;
  uint64_t bit_pos;
  int array_pos;
  /*after settings have to be reset before before settings. */
  for (i=0;i<nr_changes;i++){
    for (j=0;j<sysconf(_SC_NPROCESSORS_CONF);j++){
      bit_pos=(1ULL)<<j%64;
      array_pos=j/64;
      /* when we changesd this cpu */
      if (changes[i].cpus_used[array_pos]&bit_pos){
        fd=x86_adapt_get_device(X86_ADAPT_CPU,j);
#ifdef DEBUG
        fprintf(stderr,"Resetting x86_adapt %d to %llx on CPU %d\n",i,changes[i].default_setting,j);
#endif
        if ( (x86_adapt_set_setting(fd,i,changes[i].default_setting)) != 8 ){
          fprintf(stderr,"Error resetting x86_adapt item id %d at cpu %d\n",i,j );
        }
        x86_adapt_put_device(X86_ADAPT_CPU,j);
      }
    }
    /* when we changed all cpus */
    if (changes_all[i].cpus_used[0]){
      fd=x86_adapt_get_all_devices(X86_ADAPT_CPU);
#ifdef DEBUG
      fprintf(stderr,"Resetting x86_adapt %d to %llx on CPU all\n",i,changes[i].default_setting);
#endif
      if ( (x86_adapt_set_setting(fd,i,changes_all[i].default_setting)) != 8 ){
        fprintf(stderr,"Error resetting x86_adapt item id %d at cpu %d\n",i,j );
      }
      x86_adapt_put_all_devices(X86_ADAPT_CPU);
    }
  }
  return 0;
}
