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
