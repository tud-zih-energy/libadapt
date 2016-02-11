/*
 * dct.h
 *
 *  Created on: 09.01.2012
 *      Author: rschoene
 */

#ifndef DCT_H_
#define DCT_H_

#include <stdint.h>
#include <dlfcn.h>
#include <libconfig.h>

#define DCT_CONFIG_STRING "dct"

struct dct_information{
  int32_t threads_before;
  int32_t threads_after;
};
int init_dct_information(void);

int dct_read_from_config(void * info,struct config_t * cfg, char * buffer, char * prefix);
int dct_process_before(void * info,int ignored);
int dct_process_after(void * info,int ignored);

int omp_dct_get_set_threads(void);

int omp_dct_get_max_threads(void);

void omp_dct_set_num_threads(int num);

void omp_dct_repeat_exit(void);


#endif /* DCT_H_ */
