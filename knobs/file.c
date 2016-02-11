#include "file.h"
#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>

int file_read_from_config(void * vp,struct config_t * cfg, char * buffer, char * prefix){

  int i;
  int was_set=0;
  struct file_information * info=vp;
  config_setting_t *setting;
  /* Resetting */
  memset(info,0,sizeof(struct file_information));

  for (i=0;i<32000;i++){

    /* we need the name of the file */
    sprintf(buffer, "%s.%s_%d.name", prefix, FILE_CONFIG_STRING,i);
    setting = config_lookup(cfg, buffer);

    if (setting) {
      /* another setting :) */
      info->filename=realloc(info->filename,(i+1)*sizeof(char*));
      info->fd=realloc(info->fd,(i+1)*sizeof(int));
      info->value_before=realloc(info->value_before,(i+1)*sizeof(char*));
      info->value_before_len=realloc(info->value_before_len,(i+1)*sizeof(size_t));
      info->value_after=realloc(info->value_after,(i+1)*sizeof(char*));
      info->value_after_len=realloc(info->value_after_len,(i+1)*sizeof(size_t));
      info->nr_files=i+1;


      /* get the settings */
      info->filename[i]=strdup(config_setting_get_string(setting));
      info->fd[i]=open(info->filename[i],O_CREAT | O_WRONLY | O_APPEND,S_IRUSR| S_IWUSR);


      sprintf(buffer, "%s.%s_%d.before", prefix, FILE_CONFIG_STRING,i);
      setting = config_lookup(cfg, buffer);

      if (setting){
        info->value_before[i]=strdup(config_setting_get_string(setting));
        info->value_before_len[i]=strlen(info->value_before[i]);
        was_set=1;
      }
      else
        info->value_before[i]=NULL;

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
  return was_set;
}

int file_process_before(void * vp, int ignored){
  int i;
  int ret;
  struct file_information * info=vp;
  if (info->value_before==NULL) return 0;

  for (i=0;i<info->nr_files;i++){
    if (info->value_before[i]!=NULL){
      ret=write(info->fd[i],info->value_before[i],info->value_before_len[i]);
      if (ret!=info->value_before_len[i])
      {
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
      if (write(info->fd[i],info->value_after[i],info->value_after_len[i])!=info->value_after_len[i])
        return 1;
    }
  }
  return 0;
}

