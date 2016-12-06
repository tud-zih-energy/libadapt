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
 * x86_adapt_items.h
 *
 *  Created on: 24.10.2012
 *      Author: rschoene
 */

#ifndef X86_ADAPT_ITEMS_H_
#define X86_ADAPT_ITEMS_H_

#include <stdint.h>
#include <x86_adapt.h>

struct pref_setting_ids {
  uint64_t setting;
  int id;
  x86_adapt_device_type type;
};

struct x86_adapt_pref_information {
  int8_t nr_settings_before;
  struct pref_setting_ids * settings_before;
  int8_t nr_settings_after;
  struct pref_setting_ids * settings_after;
  int8_t nr_settings_before_all;
  struct pref_setting_ids * settings_before_all;
  int8_t nr_settings_after_all;
  struct pref_setting_ids * settings_after_all;
};

#define X86_ADAPT_PREF_CONFIG_STRING "x86_adapt"

int x86_adapt_read_from_config(void * info,struct config_t * cfg, char * buffer,
                               char * prefix);

int x86_adapt_reset(void);

int x86_adapt_process_before(void * info,int32_t cpu);
int x86_adapt_process_after(void * info,int32_t cpu);

char * x86_adapt_get_setting_string(uint64_t id,int length);
int x86_adapt_set_id(uint64_t setting,int length);
uint64_t x86_adapt_get_id(int length);

#endif /* X86_ADAPT_ITEMS_H_ */
