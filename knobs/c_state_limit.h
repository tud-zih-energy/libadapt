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
 * c_state_limit.h
 *
 *  Created on: 09.01.2012
 *      Author: rschoene
 */

#ifndef CSTATE_LIMIT_H_
#define CSTATE_LIMIT_H_

#include <stdint.h>
#include <dlfcn.h>
#include <libconfig.h>

#define CSTATE_LIMIT_CONFIG_STRING "csl"

struct csl_information{
  int32_t csl_before;
  int32_t csl_after;
};

int csl_init(void);

int csl_read_from_config(void * info,struct config_t * cfg, char * buffer, char * prefix);

int csl_process_before(void * info, int32_t cpu);
int csl_process_after(void * info, int32_t cpu);

int csl_fini(void);

#endif /* CSTATE_LIMIT_H_ */
