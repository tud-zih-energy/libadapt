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
