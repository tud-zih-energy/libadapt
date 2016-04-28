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

#ifndef FILE_H_
#define FILE_H_

#include <stdint.h>
#include <stddef.h>
#include <libconfig.h>

#define FILE_CONFIG_STRING "file"

struct file_information{
  char ** filename;
  int * fd;
  char ** value_before;
  size_t * value_before_len;
  char ** value_after;
  size_t * value_after_len;
  int nr_files;
};

int file_read_from_config(void * info,struct config_t * cfg, char * buffer, char * prefix);

int file_process_before(void * info,int ignored);
int file_process_after(void * info,int ignored);


#endif /* FILE_H_ */
