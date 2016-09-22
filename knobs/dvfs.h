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
 * dvfs.h
 *
 *  Created on: 09.01.2012
 *      Author: rschoene
 */

#ifndef DVFS_H_
#define DVFS_H_

#include <inttypes.h>
#include <dlfcn.h>
#include <libconfig.h>

#define DVFS_CONFIG_STRING "dvfs"

struct dvfs_information{
  int32_t freq_before;
  int32_t freq_after;
};

int dvfs_read_from_config(void * info,struct config_t * cfg, char * buffer, char * prefix);

int dvfs_process_before(void * info, int32_t cpu);
int dvfs_process_after(void * info, int32_t cpu);

uint16_t dvfs_get_freq_id(void);
void dvfs_set_freq_id(uint16_t id);

int init_dvfs(void);
int fini_dvfs(void);

#endif /* DVFS_H_ */
