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

#include "file.h"
#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>

/* if realloc fail something is particular fail 
 * but we're going on and process the settings there fit in the memory
 * */
#define BREAK_REALLOC_FAIL(ptr,size) \
    tmp = realloc(ptr,size); \
    if (tmp == NULL) \
        break; \
    else \
        ptr = tmp; \
    tmp = NULL;

int file_read_from_config(void * vp,struct config_t * cfg, char * buffer, char * prefix){

  int i;
  int was_set = 0;
  struct file_information * info = vp;
  config_setting_t *setting;
  /* Resetting Memory */
  memset(info,0,sizeof(struct file_information));

  for (i=0;i<32000;i++){
    /* TODO: Find better way to parse all the file container */

    /* we need the name of the file */
    sprintf(buffer, "%s.%s_%d.name", prefix, FILE_CONFIG_STRING,i);
    setting = config_lookup(cfg, buffer);

    if (setting) {
      /* another setting :) */
      /* make an "array" with all the filenames and the string that should
       * be wrtitten to the files */
      /* temporary void pointer for the macro
       * realloc gives us a void pointer back, so there is no problem*/
      void *tmp;
      BREAK_REALLOC_FAIL(info->filename,(i+1)*sizeof(char*));
      BREAK_REALLOC_FAIL(info->fd,(i+1)*sizeof(int));
      BREAK_REALLOC_FAIL(info->value_before,(i+1)*sizeof(char*));
      BREAK_REALLOC_FAIL(info->value_before_len,(i+1)*sizeof(size_t));
      BREAK_REALLOC_FAIL(info->value_after,(i+1)*sizeof(char*));
      BREAK_REALLOC_FAIL(info->value_after_len,(i+1)*sizeof(size_t));
      info->nr_files=i+1;


      /* get the settings */
      info->filename[i]=strdup(config_setting_get_string(setting));
      /* open file for later write */
      info->fd[i]=open(info->filename[i],O_CREAT | O_WRONLY | O_APPEND,S_IRUSR| S_IWUSR);

      /* string to write in file before */
      sprintf(buffer, "%s.%s_%d.before", prefix, FILE_CONFIG_STRING,i);
      setting = config_lookup(cfg, buffer);

      if (setting){
        info->value_before[i]=strdup(config_setting_get_string(setting));
        info->value_before_len[i]=strlen(info->value_before[i]);
        was_set=1;
      }
      else
        info->value_before[i]=NULL;

      /* string to write in file after */
      sprintf(buffer, "%s.%s_%d.after", prefix, FILE_CONFIG_STRING,i);
      setting = config_lookup(cfg, buffer);

      if (setting){
        info->value_after[i]=strdup(config_setting_get_string(setting));
        info->value_after_len[i]=strlen(info->value_after[i]);
        was_set=1;
      }
      else
        info->value_after[i]=NULL;

    } else
      break;

  }
  /* retrun if there was set any file writings */
  return was_set;
}

int file_process_before(void * vp, int ignored){
  int i;
  struct file_information * info=vp;
  if (info->value_before==NULL) return 0;

  for (i=0;i<info->nr_files;i++){
    if (info->value_before[i]!=NULL){
      if(write(info->fd[i],info->value_before[i],info->value_before_len[i]) != info->value_before_len[i]) {
        return 1;
      }
    }
  }
  return 0;
}

int file_process_after(void * vp, int ignored){
  int i;
  struct file_information * info=vp;
  if (info->value_after==NULL) return 0;

  for (i=0;i<info->nr_files;i++){
    if (info->value_after[i]!=NULL){
      if (write(info->fd[i],info->value_after[i],info->value_after_len[i]) != info->value_after_len[i])
        return 1;
    }
  }
  return 0;
}

