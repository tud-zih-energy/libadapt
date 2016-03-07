/*
 * dct.c
 *
 *  Created on: 09.01.2012
 *      Author: rschoene
 */

#include "dct.h"
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

/*#define VERBOSE 1 */

static int dct_enabled=0;
static int last_set_threads = 0;
static int initial_num_threads = 0;

/* The OMP runtime does not have to support dynamic, but we do */
/* Therefor we have to disable their dynamic and return true */
/* If we're asked for a dynamic omp runtime */

int omp_get_dynamic(){
  return dct_enabled;
}

void omp_set_dynamic(int enabled){
  dct_enabled=enabled;
}

int omp_dct_get_set_threads(){
  return last_set_threads;
}

static union{
  void * vp;
  int (*function)(void);
}omp_dct_orig_get_thread_num;

static union{
  void * vp;
  int (*function)(void);
}omp_dct_orig_get_max_threads;

static union{
  void * vp;
  int (*function)(void);
}omp_dct_orig_get_num_threads;

static union{
  void * vp;
  int (*function)(void);
} omp_dct_orig_get_dynamic;

static union{
  void * vp;
  void (*function)(int);
} omp_dct_orig_set_dynamic;

static union{
  void * vp;
  void (*function)(int);
}omp_dct_orig_set_num_threads;

int omp_dct_get_thread_num(void){
  if (omp_dct_orig_get_thread_num.vp)
    return omp_dct_orig_get_thread_num.function();
  return 0;
}

int omp_dct_get_max_threads(void){
  if (omp_dct_orig_get_max_threads.vp)
    return omp_dct_orig_get_max_threads.function();
  return 0;
}
int omp_dct_get_num_threads(void){
  if (omp_dct_orig_get_num_threads.vp)
    return omp_dct_orig_get_num_threads.function();
  return 0;
}
int omp_dct_get_dynamic_orig(void){
  if (omp_dct_orig_get_dynamic.vp)
    return omp_dct_orig_get_dynamic.function();
  return 0;
}
void omp_set_dynamic_orig(int dyn){
  if (omp_dct_orig_set_dynamic.vp)
    omp_dct_orig_set_dynamic.function(dyn);
}

void omp_dct_set_num_threads(int num){
  if (omp_dct_orig_set_num_threads.vp){
    last_set_threads = num;
    omp_dct_orig_set_num_threads.function(num);
  }
}
void load_omp_lib(){
  static int failed=0;
  if (failed) return;

  omp_dct_orig_get_thread_num.vp = dlsym(RTLD_DEFAULT, "omp_get_thread_num");
  if (omp_dct_orig_get_thread_num.vp == NULL)
    omp_dct_orig_get_thread_num.vp = dlsym(RTLD_NEXT, "omp_get_thread_num");
  if (omp_dct_orig_get_thread_num.vp==NULL){
    fprintf(stderr,"Error loading function %s (%s)\n","omp_get_thread_num",dlerror());
    failed=1;
    return;
  }

  omp_dct_orig_get_max_threads.vp = dlsym(RTLD_DEFAULT, "omp_get_max_threads");
  if (omp_dct_orig_get_max_threads.vp==NULL)
    omp_dct_orig_get_max_threads.vp = dlsym(RTLD_NEXT, "omp_get_max_threads");
  if (omp_dct_orig_get_max_threads.vp==NULL){
    fprintf(stderr,"Error loading function %s (%s)\n","omp_get_max_threads",dlerror());
    failed=1;
    return;
  }

  omp_dct_orig_get_num_threads.vp = dlsym(RTLD_DEFAULT, "omp_get_num_threads");
  if (omp_dct_orig_get_num_threads.vp==NULL)
    omp_dct_orig_get_num_threads.vp = dlsym(RTLD_NEXT, "omp_get_num_threads");
  if (omp_dct_orig_get_num_threads.vp==NULL){
    fprintf(stderr,"Error loading function %s (%s)\n","omp_get_num_threads",dlerror());
    failed=1;
    return;
  }

  omp_dct_orig_get_dynamic.vp = dlsym(RTLD_DEFAULT, "omp_get_dynamic");
  if (omp_dct_orig_get_dynamic.vp==NULL)
    omp_dct_orig_get_dynamic.vp = dlsym(RTLD_NEXT, "omp_get_dynamic");
  if (omp_dct_orig_get_dynamic.vp==NULL){
    fprintf(stderr,"Error loading function %s (%s)\n","omp_get_dynamic",dlerror());
    failed=1;
    return;
  }


  omp_dct_orig_set_num_threads.vp = dlsym(RTLD_DEFAULT, "omp_set_num_threads");
  if (omp_dct_orig_set_num_threads.vp==NULL)
    omp_dct_orig_set_num_threads.vp = dlsym(RTLD_NEXT, "omp_set_num_threads");
  if (omp_dct_orig_set_num_threads.vp==NULL){

    fprintf(stderr,"Error loading function %s (%s)\n","omp_set_num_threads",dlerror());
    failed=1;
    return;
  }

  omp_dct_orig_set_dynamic.vp = dlsym(RTLD_DEFAULT, "omp_set_dynamic");
  if (omp_dct_orig_set_dynamic.vp==NULL)
    omp_dct_orig_set_dynamic.vp = dlsym(RTLD_NEXT, "omp_set_dynamic");
  if (omp_dct_orig_set_dynamic.vp==NULL){
      fprintf(stderr,"Error loading function %s (%s)\n","omp_set_dynamic",dlerror());
      failed=1;
      return;
  }
}

int init_dct_information(){
  if (omp_dct_orig_get_thread_num.vp!=NULL){
    return 1;
  }
  load_omp_lib();
  return 0;
}

int dct_read_from_config(void * vp,struct config_t * cfg, char * buffer, char * prefix){
  config_setting_t *setting;
  struct dct_information * info = vp;
  info->threads_before=-1;
  info->threads_after=-1;
  init_dct_information();
  initial_num_threads=omp_dct_get_max_threads();

  sprintf(buffer, "%s.%s_threads_before",
      prefix, DCT_CONFIG_STRING);
  setting = config_lookup(cfg, buffer);
  if (setting){
    info->threads_before = config_setting_get_int(setting);
    if (info->threads_before==0)
      info->threads_before=initial_num_threads;
    dct_enabled=1;
  } else {
    info->threads_before = -1;
  }
  sprintf(buffer, "%s.%s_threads_after",
      prefix, DCT_CONFIG_STRING);
  setting = config_lookup(cfg, buffer);
  if (setting){
    info->threads_after = config_setting_get_int(setting);
    if (info->threads_after==0)
      info->threads_after=initial_num_threads;
    dct_enabled=1;
  } else {
    info->threads_after = -1;
  }
  if (dct_enabled)
  {
    omp_set_dynamic_orig(0);
    return 1;
  }
  else
    return 0;
}

int dct_process_before(void * vp,int ignore){
  struct dct_information * info = vp;
  if (dct_enabled)
    if (info->threads_before > 0) {

#ifdef VERBOSE
      fprintf(stderr,"adapting threads to %d\n",info->threads_before);
#endif
      omp_dct_set_num_threads(info->threads_before);
    }
  return 0;
}
static int last_exit_threads=0;

int dct_process_after(void * vp,int ignore){
  struct dct_information * info = vp;
  if (dct_enabled)
    if (info->threads_after > 0) {

#ifdef VERBOSE
      fprintf(stderr,"adapting threads to %d\n",info->threads_after);
#endif
      omp_dct_set_num_threads(info->threads_after);
      last_exit_threads=info->threads_after;
    }
  return 0;
}
void omp_dct_repeat_exit(){
  if (dct_enabled)
    if (last_exit_threads){
      omp_dct_set_num_threads(last_exit_threads);
      last_exit_threads=0;
    }
}




