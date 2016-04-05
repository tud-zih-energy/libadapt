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

#include <execinfo.h>
#include <libconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <errno.h>

#include <sched.h>

#include "adapt.h"
#include "adapt_internal.h"

// #define VERBOSE 1

/* TODO: Reduce SoC in this file
 * put functions for the binary handling and for the settings handling
 * in separated files from the adapt realted stuff
 * to make it more readable
 * 
 * separate hashmaps should be a lot easier as the settings
 * default_infos and init_infos can stay here
 * so only the hashmaps need to move, that are only used outside of the
 * adapt functions
 * */


/* Check if the given value is zero or not and return the
 * correspondeding ADAPT_STATUS 
 * */
#define RETURN_ADAPT_STATUS(settings) \
    if (settings) \
        return ADAPT_OK; \
    else \
        return ADAPT_ERROR_WHILE_ADAPT;

/* Free and set the given pointer to NULL 
 * */
#define FREE_AND_NULL(a) {\
        free(a);\
        a = NULL;\
}

/* Check pointer and then free it
 * */
#define CHECK_INIT_MALLOC_FREE(a) if(a)\
    FREE_AND_NULL(a)

/* unsusable stuff in the case that not enough memory is avaible 
 * */
#define CHECK_INIT_MALLOC(a) if( (a) == NULL) { \
    config_destroy(&cfg); \
    CHECK_INIT_MALLOC_FREE(c2conf_hashmap); \
    CHECK_INIT_MALLOC_FREE(r2c_hashmap); \
    CHECK_INIT_MALLOC_FREE(bids_hashmap); \
    CHECK_INIT_MALLOC_FREE(function_stacks); \
    CHECK_INIT_MALLOC_FREE(function_stack_sizes); \
    CHECK_INIT_MALLOC_FREE(default_infos); \
    CHECK_INIT_MALLOC_FREE(init_infos); \
    CHECK_INIT_MALLOC_FREE(knob_offsets); \
    return ENOMEM; \
}


/* Free a list
 * static function would be nicer but is not possible for different
 * structures 
 * */
#define FREE_LIST(current_ptr) \
    if (current_ptr->next){ \
        __auto_type next_ptr = current_ptr; \
        current_ptr = current_ptr->next; \
        FREE_AND_NULL(next_ptr);\
        while(current_ptr->next){ \
            next_ptr = current_ptr->next; \
            FREE_AND_NULL(current_ptr); \
            current_ptr = next_ptr; \
        } \
        FREE_AND_NULL(current_ptr); \
    }

/* relates dynamic region id (rid) passed from instrumentation framework
 * to libadapt to a constant region id (crid) computed from the hash of
 * the region name. */
struct rid_to_crid_struct{
  uint32_t rid;
  uint64_t crid;
  uint64_t binary_id;
  char * infos;
  int initialized;
  struct rid_to_crid_struct * next;
};

/* relates constant region id (crid) computed from the hash of a region
 * name to a configuration. This struct is created when parsing the
 * configuration file. */
struct crid_to_config_struct{
  uint64_t crid;
  uint64_t binary_id;
  char * infos;
  int initialized;
  struct crid_to_config_struct * next;
};

/* This struct is used to allow multi binary support.
 * If you monitor a system for example that allows the concurrent execution
 * of different tasks, you create such a struct for each task
 * */
struct added_binary_ids_struct{
  uint64_t binary_id;
  int used;
  char * default_infos;
  struct added_binary_ids_struct * next;
};

/* These are the global error count to supress more than one error
 * message about the maximum number of threas */
static int supress_max_thread_count_error = 0;

/* These are the global configurations from the config file  used in adapt_open and
 * adapt_add_binary
 * */
static struct config_t cfg;

/*
 * These (default_*_infos) are the defaults that are set whenever the binary
 * that is entered / exited is not defined in the config file
 * */
static char * default_infos = NULL;

/*
 * These (init_*_infos) are the initial values that are set when this library is loaded
 * */
static char * init_infos = NULL;

/**
 * these are the offset for the different knob types' information within the infos
 */
static size_t * knob_offsets = NULL;

/*
 * These are the function stack sizes for different number of threads
 * They only support one binary. I think thats ok.
 * Otherwise one would have to use the binary number as thread_id, right?
 * */

static uint32_t ** function_stacks = NULL;
static uint32_t * function_stack_sizes = NULL;

/* maps described in struct definition */
static struct crid_to_config_struct * c2conf_hashmap = NULL;
static struct rid_to_crid_struct * r2c_hashmap = NULL;
static struct added_binary_ids_struct * bids_hashmap = NULL;


static uint32_t max_threads = 256;
static uint32_t hash_set_size = 101;
static uint32_t max_function_stack = 256;

/* every error message should use the same stream */
static FILE * error_stream;

/*-----------------------------------------------------------------------------
 MurmurHash2, 64-bit versions, by Austin Appleby

 The same caveats as 32-bit MurmurHash2 apply here - beware of alignment
 and endian-ness issues if used across multiple platforms.

 64-bit hash for 64-bit platforms
*/

uint64_t MurmurHash64A ( const void * key, int len, uint64_t seed )
{
  const uint64_t m = 0xc6a4a7935bd1e995ULL;
  const int r = 47;

  uint64_t h = seed ^ (len * m);

  const uint64_t * data = (const uint64_t *)key;
  const uint64_t * end = data + (len/8);

  const unsigned char * data2;

  while(data != end)
  {
    uint64_t k = *data++;

    k *= m;
    k ^= k >> r;
    k *= m;

    h ^= k;
    h *= m;
  }

  data2 = (const unsigned char*)data;

  switch(len & 7)
  {
  case 7: h ^= (uint64_t)data2[6] << 48;
  case 6: h ^= (uint64_t)data2[5] << 40;
  case 5: h ^= (uint64_t)data2[4] << 32;
  case 4: h ^= (uint64_t)data2[3] << 24;
  case 3: h ^= (uint64_t)data2[2] << 16;
  case 2: h ^= (uint64_t)data2[1] << 8;
  case 1: h ^= (uint64_t)data2[0];
          h *= m;
  };

  h ^= h >> r;
  h *= m;
  h ^= h >> r;

  return h;
}

static inline uint64_t get_id(const char * name){
  return MurmurHash64A(name, strlen(name), 0);
}

struct added_binary_ids_struct * add_binary_id(uint64_t binary_id){
  struct added_binary_ids_struct * current=&bids_hashmap[binary_id%hash_set_size];
  while (current->next){
    if ((current->binary_id==binary_id)) return current;
    current=current->next;
  }
  if ((current->binary_id==binary_id)) return current;
  current->binary_id=binary_id;
  current->default_infos=calloc(1,adapt_information_size);
  current->next=calloc(1,sizeof(struct added_binary_ids_struct));
  return current;
}

struct added_binary_ids_struct * get_bid(uint64_t binary_id){
  struct added_binary_ids_struct * current=&bids_hashmap[binary_id%hash_set_size];
  while (current->next){
    if ((current->binary_id==binary_id)) return current;
    current=current->next;
  }
  if ((current->binary_id==binary_id)) return current;
  return NULL;

}

int exists_binary_id(uint64_t binary_id){
  if (get_bid(binary_id)) return 1;
  return 0;
}

int is_binary_id_used(uint64_t binary_id){
  struct added_binary_ids_struct * current=&bids_hashmap[binary_id%hash_set_size];
  while (current->next){
    if ((current->binary_id==binary_id)) return current->used;
    current=current->next;
  }
  if ((current->binary_id==binary_id)) return current->used;
  return 0;
}

void set_binary_id_used(uint64_t binary_id,int used){
  struct added_binary_ids_struct * current=&bids_hashmap[binary_id%hash_set_size];
  while (current->next){
    if ((current->binary_id==binary_id)){
      current->used=used;
      return;
    }
    current=current->next;
  }
  if ((current->binary_id==binary_id)){
    current->used=used;
    return;
  }
  return;
}

/* add crid_to_config_struct */
struct crid_to_config_struct * add_crid2config(uint64_t binary_id,uint64_t crid,struct crid_to_config_struct * tmp_crid_to_config_struct){
  struct crid_to_config_struct * current=&c2conf_hashmap[crid%hash_set_size];
  while (current->next){
    if ((current->binary_id==binary_id)&&(current->crid==crid)) return current;
    current=current->next;
  }
  if ((current->binary_id==binary_id)&&(current->crid==crid)) return current;

  if (current->initialized){
    current->next=calloc(1,sizeof(struct crid_to_config_struct));
    current=current->next;
  }
  memcpy(current,tmp_crid_to_config_struct,sizeof(struct crid_to_config_struct));
  current->crid=crid;
  current->binary_id=binary_id;
  current->next=NULL;
  current->initialized=1;
  return current;
}
/* get crid_to_config_struct */
struct crid_to_config_struct * get_crid2config(uint64_t binary_id,uint64_t crid){
  struct crid_to_config_struct * current=&c2conf_hashmap[crid%hash_set_size];
  while (current->next){
    if ((current->binary_id==binary_id)&&(current->crid==crid)) return current;
    current=current->next;
  }
  if ((current->binary_id==binary_id)&&(current->crid==crid)) return current;
  return NULL;
}

/**
 * @brief define a region id for a constant region id
 * 
 * There are constant region IDs, e.g. the hash of a name of a function that is entered and region ids that can change from run to run (e.g. the measurement systems id or handle of that region)
 * this function checks whether there is some adaption to process for a specific region.
 * if so, it adds the region id to the processed region ids.
 * @param binary_id the id of a binary retrieved with adapt_add_binary
 * @param rid the variable region id
 * @param the constant region id
 * @return 1 if there is no adaption definition or the binary id is not available or the region id is already registered<br>
 * 1 if the region has been added
 */
int add_rid2crid(uint64_t binary_id,uint32_t rid,uint64_t crid){
  /* exists config? */
  struct crid_to_config_struct * c2d = get_crid2config(binary_id,crid);
  struct rid_to_crid_struct * current;
  if (c2d==NULL) return 1;
  current=&r2c_hashmap[rid%hash_set_size];
  while (current->next){
    if ((current->binary_id==binary_id)&&(current->rid==rid)) return 1;
    current=current->next;
  }
  if ((current->binary_id==binary_id)&&(current->rid==rid)) return 1;

  if (current->initialized){
    current->next=calloc(1,sizeof(struct rid_to_crid_struct));
    current=current->next;
  }
  current->rid=rid;
  memcpy(&(current->crid),c2d,sizeof(struct crid_to_config_struct));
  current->next=NULL;
  return 0;
}
/* get rid_to_crid */
struct rid_to_crid_struct * get_rid2crid(uint64_t binary_id,uint32_t rid){
  struct rid_to_crid_struct * current=&r2c_hashmap[rid%hash_set_size];
  while (current->next){
    if ((current->binary_id==binary_id)&&(current->rid==rid)) return current;
    current=current->next;
  }
  if ((current->binary_id==binary_id)&&(current->rid==rid)) return current;
  return NULL;
}

static int regex_match(const char *pattern, char *string) {
  int status;
  regex_t re;

  if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) != 0) {
    return (0); /* report error */
  }
  status = regexec(&re, string, (size_t) 0, NULL, 0);
  regfree(&re);
  if (status != 0) {
    return (0); /* report error */
  }
  return (1);
}

int adapt_open(){
  char *file_name;
  char * prefix_default="default";
  char * prefix_init="init";
  char buffer[1024];
  config_setting_t *setting = NULL;
  int set_default=0,set_init=0;

  error_stream = stderr;

  /* open config */
  file_name = getenv("ADAPT_CONFIG_FILE");
  if (file_name == NULL) {
    fprintf(error_stream, "\"ADAPT_CONFIG_FILE\" not set\n");
    return 1;
  }
  /* initialize config file*/
  config_init(&cfg);

  if (!config_read_file(&cfg, file_name)) {
    fprintf(error_stream, "reading config file %s failed\n", file_name);
    return 1;
  }

  /* max_threads set? */
  setting = config_lookup(&cfg, "max_threads");
  if (setting)
    max_threads = config_setting_get_int(setting);
  /* hash_set_size? */
  setting = config_lookup(&cfg, "hash_set_size");
  if (setting)
    hash_set_size = config_setting_get_int(setting);
  /* function_stack size? */
  setting = config_lookup(&cfg, "max_function_stack");
  if (setting)
    max_function_stack = config_setting_get_int(setting);

  /* function_stack size? */
  setting = config_lookup(&cfg, "error_file");
  if (setting)
    error_stream = fopen(config_setting_get_string(setting),"w+");

  if (function_stacks != NULL)
  {
    fprintf(error_stream, "libadapt already initialized\n");
    return 1;
  }

  /* create hashmaps */
  CHECK_INIT_MALLOC(c2conf_hashmap=calloc(hash_set_size,sizeof(struct crid_to_config_struct)));
  CHECK_INIT_MALLOC(r2c_hashmap=calloc(hash_set_size,sizeof(struct rid_to_crid_struct)));
  CHECK_INIT_MALLOC(bids_hashmap=calloc(hash_set_size,sizeof(struct added_binary_ids_struct)));
  /* create function_stacks */
  CHECK_INIT_MALLOC(function_stacks=calloc(max_threads,sizeof(uint32_t *)));
  CHECK_INIT_MALLOC(function_stack_sizes=calloc(max_threads,sizeof(uint32_t)));
  /* create settings structure */
  CHECK_INIT_MALLOC(default_infos=calloc(1,adapt_information_size));
  CHECK_INIT_MALLOC(init_infos=calloc(1,adapt_information_size));

  CHECK_INIT_MALLOC(knob_offsets=calloc(sizeof(size_t),ADAPT_MAX));
  size_t current_offset=0;
  int knob_index;
  for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
  {
    knob_offsets[knob_index]=current_offset;
    current_offset+=knobs[knob_index].information_size;
  }

  /* initialize */
  for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
  {
    if (knobs[knob_index].init != NULL)
      if (knobs[knob_index].init())
      {
        fprintf(error_stream, "Error initializing knob category \"%s\"\n",knobs[knob_index].name);
        knobs[knob_index].read_from_config = NULL;
        knobs[knob_index].process_before = NULL;
        knobs[knob_index].process_after = NULL;
        knobs[knob_index].fini = NULL;
      }
  }
  /* get inits and defaults and apply inits */
  for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
  {
    set_init=0;
    if (knobs[knob_index].read_from_config)
    {
      set_default |= knobs[knob_index].read_from_config(&(default_infos[knob_offsets[knob_index]]),&cfg,buffer,prefix_default);

      set_init = knobs[knob_index].read_from_config(&(init_infos[knob_offsets[knob_index]]),&cfg,buffer,prefix_init);

      if (set_init)
      {
        if (knobs[knob_index].process_before)
          /* apply setting for initialize for the current cpu */
          knobs[knob_index].process_before(&(init_infos[knob_offsets[knob_index]]),sched_getcpu());
      }
    }
  }

  if (!set_default)
  {
    FREE_AND_NULL(default_infos);
  }

  return 0;
}

uint64_t adapt_add_binary(char * binary_name){
  config_setting_t *setting = NULL;
  int knob_index,set=0;
  char buffer[1024];
  char prefix[1024];
  uint32_t binary_id_in_cfg_file;
  uint32_t function_id_in_cfg_file;
  uint64_t binary_id;
  const char * binary_name_in_cfg;
  struct added_binary_ids_struct * bid_struct;
  if(function_stacks==NULL) return 0;
  binary_id=get_id(binary_name);
  if (exists_binary_id(binary_id)) return binary_id;
  bid_struct=add_binary_id(binary_id);

  /* look if binary_name  exists*/
  for (binary_id_in_cfg_file = 0; binary_id_in_cfg_file < 32000; binary_id_in_cfg_file++) {
    sprintf(buffer, "binary_%d.name", binary_id_in_cfg_file);
    setting = config_lookup(&cfg, buffer);
    if (setting) {
      /* getting name */
      binary_name_in_cfg = config_setting_get_string(setting);
      /* break if the strings are equal */
      if (strcmp(binary_name, binary_name_in_cfg) == 0) {
        break;
      }
      /* maybe the given name is part of a regular expression in the
      * config because we allow regular expressions */
      if (regex_match(binary_name_in_cfg, binary_name)) {
        break;
      }
    } else {
    /* Not Found, break polite out of the function 
     * because the setting not exist and in the previous settings were
     * no matching binary name */

#ifdef VERBOSE
      fprintf(error_stream,"ending after %d checks for binary information blubb %s\n",binary_id_in_cfg_file,binary_name);
#endif

      return 0;
    }
  }

  /* binary_id_in_cfg_file has now the fitting value for the binary_name
   * */

  /* get defaults from the config */
  sprintf(prefix, "binary_%d", binary_id_in_cfg_file);
  for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
  {
    if (knobs[knob_index].read_from_config)
      set |= knobs[knob_index].read_from_config(&(bid_struct->default_infos[knob_offsets[knob_index]]),&cfg,buffer,prefix);
  }
  if (set){
#ifdef VERBOSE
      fprintf(error_stream,"binary %s %llu %llu provides default values %d\n",binary_name,binary_id,get_id(binary_name),is_binary_id_used(binary_id));
#endif
    /* mark binary_id as used if there any setting */
    set_binary_id_used(binary_id,1);
  }

  /* Read Configuration for function in config structure and register it
   * with the binary_id */
  for (function_id_in_cfg_file = 0; function_id_in_cfg_file < 32000; function_id_in_cfg_file++) {
    sprintf(buffer, "binary_%d.function_%d.name", binary_id_in_cfg_file, function_id_in_cfg_file);
    setting = config_lookup(&cfg, buffer);
    if (setting) {
      int set = 0;
      sprintf(prefix, "binary_%d.function_%d", binary_id_in_cfg_file, function_id_in_cfg_file);
      uint64_t crid;
      const char * function_name_in_cfg = config_setting_get_string(setting);
      struct crid_to_config_struct tmp_crid_to_config_struct;
      memset(&tmp_crid_to_config_struct,0,sizeof(struct crid_to_config_struct));

      /* this is later used in the crid2config struct, so there is no need to free it here */
      tmp_crid_to_config_struct.infos=calloc(1,adapt_information_size);

      crid = get_id(function_name_in_cfg);

#ifdef VERBOSE
      fprintf(error_stream,"Function definition:%s/%s %s %lu %lu %llu\n",binary_name_in_cfg,binary_name,function_name_in_cfg,binary_id_in_cfg_file, function_id_in_cfg_file,crid);
#endif

      for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
      {
        if ( knobs[knob_index].read_from_config )
          set |= knobs[knob_index].read_from_config(&(tmp_crid_to_config_struct.infos[knob_offsets[knob_index]]),&cfg,buffer,prefix);
      }
      
      if (set)
      {
        /* register in hashmap */
        add_crid2config(binary_id,crid,&tmp_crid_to_config_struct);
        /* makr binary as used if there any function according to it */
        set_binary_id_used(binary_id,1);
      }
    }
    else /* Not Found, break out of the loop. */
    {

#ifdef VERBOSE
      fprintf(error_stream,"ending after %d checks for function information, %d %llx\n",function_id_in_cfg_file,is_binary_id_used(binary_id),binary_id);
#endif
      break;
    }
  }
  return binary_id;
}

int adapt_def_region(uint64_t binary_id, const char* rname, uint32_t rid){
  uint64_t crid;
  if (function_stacks==NULL) return 1;
  if (!is_binary_id_used(binary_id)) return 1;

  /* if it is not already registered */
  if (get_rid2crid(binary_id, rid)==NULL)
  {
    /*
     * build rid2crid which maps region id to constant region ids
     * (same over multiple runs)
     */
    crid = get_id(rname);
#ifdef VERBOSE
    fprintf(error_stream,"Add Function definition:%s %llu %lu\n",rname,crid,rid);
#endif
    return add_rid2crid(binary_id,rid,crid);
  }
  return 1;
}

/**
 * Use this if you have enter AND exit handling
 */
int adapt_enter_stacks(uint64_t binary_id, uint32_t tid, uint32_t rid,int32_t cpu){
    return adapt_enter_opt_stacks(binary_id, tid, rid, cpu, 1);
}

/**
 * Use this if you have only enter handling, but no exit-handling
 */
int adapt_enter_no_stacks(uint64_t binary_id, uint32_t rid,int32_t cpu){
    return adapt_enter_opt_stacks(binary_id, 0, rid, cpu, 0);
}

/**
 * Use this for both with optional stack switch
 */
int adapt_enter_opt_stacks(uint64_t binary_id, uint32_t tid, uint32_t rid,int32_t cpu, int exit_on)
{
  int knob_index;
  int ok=0;

#ifdef VERBOSE
  fprintf(error_stream,"Enter test function stacks\n");
#endif

  if (function_stacks == NULL)
  {
    fprintf(error_stream,"libadapt: ERROR: function_stacks NULL\n");
    return ADAPT_NOT_INITITALIZED;
  }

  if (exit_on)
  {
    if (tid >= max_threads)
    {
        if (!supress_max_thread_count_error)
        {
        supress_max_thread_count_error = 1;
        fprintf(error_stream, "libadapt: ERROR: maximum thread count (%d) exceeded."
                      "Increase max_threads or set VT_PTHREAD_REUSE=yes\n"
                      "This error is only shown once.\n",
                      max_threads);
        }
        return ADAPT_ERROR_WHILE_ADAPT;
    }
  }
  /* binary not used -> use defaults */
  if (!is_binary_id_used(binary_id) )
  {
#ifdef VERBOSE
    fprintf(error_stream,"Binary not used %llu, applying defaults\n",binary_id);
#endif
    if (default_infos)
    {
      for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
      {
        if (knobs[knob_index].process_before)
            ok |= knobs[knob_index].process_before(&(default_infos[knob_offsets[knob_index]]),cpu);
      }

      RETURN_ADAPT_STATUS(ok);
    }
    else
        return ADAPT_NO_ACTUAL_ADAPT;
  }
  /* unfortunately the dct exit does not work on all compilers, so we repeat it here :( */
#ifndef NO_DCT
  {
      if (exit_on)
          omp_dct_repeat_exit();
  }
#endif

  if (exit_on)
  {
    /* add to stack / stack to large? */
    if (function_stacks[tid] == NULL)
        function_stacks[tid] = calloc(max_function_stack,sizeof(uint32_t));
  }
    /* check for max stack size */
    /* this should be always executed if we don't use stack
     * but if we use stacks we need to look for the stack size
     * */
  if (!exit_on || function_stack_sizes[tid] < max_function_stack)
  {

#ifdef VERBOSE
    if (exit_on)
        fprintf(error_stream,"Enter %llu %lu %lu %lu\n",binary_id,tid,function_stack_sizes[tid],rid);
    else
        fprintf(error_stream,"Enter %lu\n",rid);
#endif
    /* get the constant region id */
    struct rid_to_crid_struct * r2d = get_rid2crid(binary_id,rid);

    /* if there is something to change */
    /* any adaption for region defined? */
    if (r2d != NULL)
    {
#ifdef VERBOSE
        fprintf(error_stream,"Crid %lu %llu\n",r2d->rid,r2d->crid);
#endif
        /* do adapt */
        for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
        {
            if (knobs[knob_index].process_before)
                ok |=knobs[knob_index].process_before(&(r2d->infos[knob_offsets[knob_index]]),cpu);
        }
    }
    /* no definition for reason -> use defaults */
    else
    {
#ifdef VERBOSE
        fprintf(error_stream,"Enter binary default\n");
#endif
        /* get struct for defaults */
        struct added_binary_ids_struct * bid = get_bid(binary_id);

        /* apply default settings for binary */
        for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
        {
            if (knobs[knob_index].process_before)
                ok |= knobs[knob_index].process_before(&(bid->default_infos[knob_offsets[knob_index]]),cpu);
        }
    }

    if (exit_on)
    {
        /* save the rgeion id for this thread */
        function_stacks[tid][function_stack_sizes[tid]] = rid;
        /* increase stack size immediately afterwards */
        function_stack_sizes[tid]++;

    }
  }

  RETURN_ADAPT_STATUS(ok);
}

int adapt_exit(uint64_t binary_id,uint32_t tid,int32_t cpu)
{
  int knob_index;
  int ok = 0;

  if (function_stacks == NULL)
  {
#ifdef VERBOSE
    fprintf(error_stream,"libadapt: ERROR: function_stacks NULL\n");
#endif
    return ADAPT_NOT_INITITALIZED;
  }
  if (tid >= max_threads)
  {
    if (!supress_max_thread_count_error)
    {
      supress_max_thread_count_error = 1;
      fprintf(error_stream, "libadapt: ERROR: maximum thread count (%d) exceeded."
                      "Increase max_threads or set VT_PTHREAD_REUSE=yes\n"
                      "This error is only shown once.\n",
                      max_threads);
    }
    return ADAPT_ERROR_WHILE_ADAPT;
  }

  /* binary not defined? use defaults! */
  if (!is_binary_id_used(binary_id))
  {
#ifdef VERBOSE
    fprintf(error_stream,"Exit default\n");
#endif

    /* if there are defaults, apply them */
    if (default_infos)
      for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
      {
        if (knobs[knob_index].process_after)
          ok |= knobs[knob_index].process_after(&(default_infos[knob_offsets[knob_index]]),cpu);
      }

      RETURN_ADAPT_STATUS(ok);
  }
  /* only if region is on stack */
  if ((function_stack_sizes[tid] - 1) >= 0)
  {
#ifdef VERBOSE
    fprintf(error_stream,"Exit %lu %lu %lu \n",tid,function_stack_sizes[tid],function_stacks[tid][function_stack_sizes[tid] - 1]);
#endif
    /* get region definition */
    struct rid_to_crid_struct * r2d = get_rid2crid(binary_id,function_stacks[tid][function_stack_sizes[tid] - 1]);

    /* if there is a region definition */
    if (r2d != NULL)
    {
	    for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
	    {
	      if (knobs[knob_index].process_after)
	        ok |= knobs[knob_index].process_after(&(r2d->infos[knob_offsets[knob_index]]),cpu);
        }

    }
    else /* if there is no region definition, apply defaults */
    {
      struct added_binary_ids_struct * bid = get_bid(binary_id);

      for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
      {
        if (knobs[knob_index].process_after)
          ok |= knobs[knob_index].process_after(&(bid->default_infos[knob_offsets[knob_index]]),cpu);
      }
    }
    /* decrease stack size */
    function_stack_sizes[tid]--;
  }

  RETURN_ADAPT_STATUS(ok);
}

void adapt_close()
{
  int knob_index;
  int i;
  /* if the work was already done by another thread, we have nothing to do */
  if (!c2conf_hashmap)
    return;

  /* first look if the work was done by another thread */
  if (function_stacks != NULL)
  {
    uint32_t ** tmp = function_stacks;
    /* make closing ready for threads
    * for free the function_stack use another pointer */
    function_stacks = NULL;

    for (i=0; i < max_function_stack; i++ )
    {
        FREE_AND_NULL(tmp[i]);
    }
    FREE_AND_NULL(tmp);
    FREE_AND_NULL(function_stack_sizes);
  }

  /* init infos? */
  if ( init_infos )
    for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
    {
      if (knobs[knob_index].process_after)
        /* apply initial setting for current cpu */
        knobs[knob_index].process_after(&(init_infos[knob_offsets[knob_index]]),sched_getcpu());
    }

  for (i=0; i < hash_set_size; i++ )
  {
    if ((&c2conf_hashmap[i]) != NULL)
    {
      struct crid_to_config_struct * current=&c2conf_hashmap[i];
      FREE_LIST(current);
    }
    if ((&r2c_hashmap[i]) != NULL)
    {
      struct rid_to_crid_struct * current=&r2c_hashmap[i];
      FREE_LIST(current);
    }
    if ((&bids_hashmap[i]) != NULL)
    {
      struct added_binary_ids_struct * current=&bids_hashmap[i];
      FREE_LIST(current);
    }
  }
  FREE_AND_NULL(c2conf_hashmap);
  FREE_AND_NULL(r2c_hashmap);
  FREE_AND_NULL(bids_hashmap);

  for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
  {
    if (knobs[knob_index].fini)
      knobs[knob_index].fini();
  }
}

