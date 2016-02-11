/*
 * dvfs.h
 *
 *  Created on: 09.01.2012
 *      Author: rschoene
 */

#ifndef DVFS_H_
#define DVFS_H_

#include <stdint.h>
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
