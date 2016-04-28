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


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <execinfo.h>
#include <unistd.h>

#include "adapt.h"

/*
 * VampirTrace API Hooks
 *
 * */

static uint64_t vt_binary_id;

void adapt_vt_ext_open(void){
  char buffer[1024];
  char * binary_name;
  int binary_name_length;
  /* get binary name */
  binary_name_length = readlink("/proc/self/exe", buffer, 1024);
  if (binary_name_length < 0) {
    fprintf(stderr, "Could not read /proc/self/exe\n");
    return;
  }
  binary_name = strdup(buffer);
  adapt_open();
  vt_binary_id=adapt_add_binary(binary_name);
  free(binary_name);
}
void adapt_vt_ext_enter(uint32_t tid, uint64_t * ignored, uint32_t rid){
  adapt_enter_stacks(vt_binary_id,tid,rid,-1);
}
void adapt_vt_ext_exit(uint32_t tid, uint64_t * ignored){
  adapt_exit(vt_binary_id,tid,-1);
}


void adapt_vt_ext_def_region(uint32_t tid, const char* rname, uint32_t fid,
    uint32_t begln, uint32_t endln, const char* rgroup, uint8_t rtype, uint32_t rid){
  adapt_def_region(vt_binary_id,rname,rid);
}


void adapt_vt_ext_close(void){}
void adapt_vt_ext_close_by_signal(void){}
void adapt_vt_ext_reset(void){}

void adapt_vt_ext_vt_trace_on(uint32_t ignored0, uint8_t ignored1){};
void adapt_vt_ext_vt_trace_off(uint32_t ignored0, uint8_t ignored1, uint8_t ignored2){};


