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

static int * cpu_fds;
static int * die_fds;

static int set=0;

static void init_info(struct pref_setting_ids * settings, x86_adapt_device_type type, int ci_nr, int64_t setting){
  settings->id=ci_nr;
  settings->setting=setting;
  settings->type=type;
}


int x86_adapt_read_from_config(void * vp,struct config_t * cfg, char * buffer,
                               char * prefix)
{
  config_setting_t *setting;
  struct x86_adapt_configuration_item  ci;
  x86_adapt_device_type type;
  int i;
  int ci_nr;
  struct x86_adapt_pref_information * info = vp;
  memset(info,0,sizeof(struct x86_adapt_pref_information));

  /* doesnt mater whether die or cpu item */
  for (type = 0; type < X86_ADAPT_MAX; type ++)
  {
  /* get the all item names from x86_adapt lib*/
    for (ci_nr=0;ci_nr<x86_adapt_get_number_cis(type);ci_nr++){
      if (x86_adapt_get_ci_definition(type, ci_nr,&ci)){
        return 0;
      }
      sprintf(buffer, "%s.%s_%s_before",
          prefix ,X86_ADAPT_PREF_CONFIG_STRING, ci.name);
      setting = config_lookup(cfg, buffer);
      /* if there is a setting with this name */
      if (setting){
        info->settings_before=realloc(info->settings_before,(info->nr_settings_before+1)*sizeof(struct pref_setting_ids));
        /* init_info will immediately fail at settings->setting=setting
         * */
        init_info(
            &info->settings_before[info->nr_settings_before], type,
            ci_nr,
            config_setting_get_int64(setting));
        /* add it to info */
        info->nr_settings_before++;
        set=1;
      }
      sprintf(buffer, "%s.%s_%s_after",
          prefix ,X86_ADAPT_PREF_CONFIG_STRING, ci.name);
      setting = config_lookup(cfg, buffer);
      if (setting){
        /* add it to info */
        info->settings_after=realloc(info->settings_after,(info->nr_settings_after+1)*sizeof(struct pref_setting_ids));
        init_info(
            &info->settings_after[info->nr_settings_after], type,
            ci_nr,
            config_setting_get_int64(setting));
        info->nr_settings_after++;
        set=1;
      }
      sprintf(buffer, "%s.%s_%s_all_before",
          prefix ,X86_ADAPT_PREF_CONFIG_STRING, ci.name);
      setting = config_lookup(cfg, buffer);
      if (setting){
        /* add it to info */
        info->settings_before_all=realloc(info->settings_before_all,(info->nr_settings_before_all+1)*sizeof(struct pref_setting_ids));
        init_info(
            &info->settings_before_all[info->nr_settings_before_all], type,
            ci_nr,
            config_setting_get_int64(setting));
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
            &info->settings_after_all[info->nr_settings_after_all], type,
            ci_nr,
            config_setting_get_int64(setting));
        info->nr_settings_after_all++;
        set=1;
      }
    }
  }
  if (set)
  {
    cpu_fds=(int*)calloc(x86_adapt_get_nr_avaible_devices(X86_ADAPT_CPU),sizeof(int));
    for (i=0;i<x86_adapt_get_nr_avaible_devices(X86_ADAPT_CPU);i++)
    {
      cpu_fds[i]=x86_adapt_get_device(X86_ADAPT_CPU,i);
    }
    die_fds=(int*)calloc(x86_adapt_get_nr_avaible_devices(X86_ADAPT_DIE),sizeof(int));
		for (i=0;i<x86_adapt_get_nr_avaible_devices(X86_ADAPT_DIE);i++)
    {
      die_fds[i]=x86_adapt_get_device(X86_ADAPT_DIE,i);
    }
  }
  return set;
}

int x86_adapt_process_before(void * vp, int32_t cpu)
{
  int i;
  struct x86_adapt_pref_information * info = vp;

  if (cpu<0)
    cpu=sched_getcpu();

  for (i=0;i<info->nr_settings_before;i++)
  {
    switch (info->settings_before[i].type)
    {
      case X86_ADAPT_CPU:
        if ( cpu_fds[cpu] > 0 )
        {
          x86_adapt_set_setting(cpu_fds[cpu],info->settings_before[i].id,info->settings_before[i].setting);
          return 0;
        }
        return 1;
      case X86_ADAPT_DIE: /* fall-through */
      default:
        return 1;
    }
  }

  for (i=0;i<info->nr_settings_before_all;i++)
  {
    int nr_devices=x86_adapt_get_nr_avaible_devices(info->settings_before_all[i].type);
    int * fds=NULL;
    switch (info->settings_before_all[i].type)
    {
      case X86_ADAPT_CPU: fds=cpu_fds; break;
      case X86_ADAPT_DIE: fds=die_fds; break;
      default:
        return 1;
    }
    for (cpu=0;cpu<nr_devices;cpu++)
    {
      if ( fds[cpu] > 0 )
        x86_adapt_set_setting(fds[cpu],info->settings_before_all[i].id,info->settings_before_all[i].setting);
    }
  }
  return 0;
}

int x86_adapt_process_after(void * vp, int32_t cpu)
{
  int i;
  struct x86_adapt_pref_information * info = vp;

  if (cpu<0)
    cpu=sched_getcpu();

  for (i=0;i<info->nr_settings_after;i++)
  {
    switch (info->settings_after[i].type)
    {
      case X86_ADAPT_CPU:
        if (cpu_fds[cpu] > 0)
        {
          x86_adapt_set_setting(cpu_fds[cpu],info->settings_after[i].id,info->settings_after[i].setting);
          return 0;
        }
        return 1;
      case X86_ADAPT_DIE: /* fall-through, no match from cpu to die */
      default:
        return 1;
    }
  }

  for (i=0;i<info->nr_settings_after_all;i++)
  {
    int nr_devices=x86_adapt_get_nr_avaible_devices(info->settings_after_all[i].type);
    int * fds=NULL;
    switch (info->settings_after_all[i].type)
    {
      case X86_ADAPT_CPU: fds=cpu_fds; break;
      case X86_ADAPT_DIE: fds=die_fds; break;
      default:
        return 1;
    }
    for (cpu=0;cpu<nr_devices;cpu++)
    {
      if (fds[cpu] > 0)
        x86_adapt_set_setting(fds[cpu],info->settings_after_all[i].id,info->settings_after_all[i].setting);
    }
  }
  return 0;
}

int x86_adapt_reset(){
   return 0;
}
