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

#include <libconfig.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include <sched.h>

#include "adapt.h"
#include "adapt_internal.h"
#include "binary_handling.h"

// #define VERBOSE 1


/* Check if the given value is zero or not and return the
 * correspondeding ADAPT_STATUS 
 * */
#define RETURN_ADAPT_STATUS(settings) \
    if (settings) \
        return ADAPT_OK; \
    else \
        return ADAPT_ERROR_WHILE_ADAPT;

/* remove unsusable stuff in the case that not enough memory is avaible 
 * */
#define CHECK_INIT_MALLOC(a) if( (a) == NULL) { \
    config_destroy(&cfg); \
    free_hashmaps(); \
    CHECK_INIT_MALLOC_FREE(function_stacks); \
    CHECK_INIT_MALLOC_FREE(function_stack_sizes); \
    CHECK_INIT_MALLOC_FREE(default_infos); \
    CHECK_INIT_MALLOC_FREE(init_infos); \
    CHECK_INIT_MALLOC_FREE(knob_offsets); \
    return ENOMEM; \
}

/* These are the global error count to supress more than one error
 * message about the maximum number of threas */
static int supress_max_thread_count_error = 0;

/**
 * these are the offset for the different knob types' information within the infos
 */
static size_t * knob_offsets = NULL;

/**
 * These are the function stack sizes for different number of threads
 * They only support one binary. I think thats ok.
 * Otherwise one would have to use the binary number as thread_id, right?
 * */
static uint32_t ** function_stacks = NULL;
static uint32_t * function_stack_sizes = NULL;

/**
 * These (default_*_infos) are the defaults that are set whenever the binary
 * that is entered / exited is not defined in the config file
 * */
static char * default_infos = NULL;

/**
 * These (init_*_infos) are the initial values that are set when this library is loaded
 * */
static char * init_infos = NULL;

/* These are the global configurations from the config file  used in adapt_open and
 * adapt_add_binary
 * */
static struct config_t cfg;

/* default settings for stack */
static uint32_t max_threads = 256;
static uint32_t max_function_stack = 256;

/* every error message should use the same stream */
static FILE * error_stream;


int adapt_open()
{
  char *file_name;
  char * prefix_default="default";
  char * prefix_init="init";
  char buffer[1024];
  config_setting_t *setting = NULL;
  int set_default=0,set_init=0;
  uint32_t hash_set_size=0;

  error_stream = stderr;

  /* open config */
  file_name = getenv("ADAPT_CONFIG_FILE");
  if (file_name == NULL)
  {
    fprintf(error_stream, "\"ADAPT_CONFIG_FILE\" not set\n");
    return 1;
  }
  /* initialize config file*/
  config_init(&cfg);

  if (!config_read_file(&cfg, file_name))
  {
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

  /* create the hashmaps with the right hash size */
  if (init_hashmaps(hash_set_size, adapt_information_size))
      return ENOMEM;

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

uint64_t adapt_add_binary(char * binary_name)
{
  config_setting_t *setting = NULL;
  int knob_index,set=0;
  char buffer[1024];
  char prefix[1024];
  uint32_t binary_id_in_cfg_file;
  uint32_t function_id_in_cfg_file;
  uint64_t binary_id;
  const char * binary_name_in_cfg;
  struct added_binary_ids_struct * bid_struct;
  
  if(function_stacks == NULL)
  {
      fprintf(error_stream,"libadapt: ERROR: function_stacks NULL\n");
      return 0;
  }
  
  binary_id=get_id(binary_name);
  if (exists_binary_id(binary_id)) return binary_id;
  
  bid_struct=add_binary_id(binary_id);

  /* look if binary_name  exists*/
  for (binary_id_in_cfg_file = 0; binary_id_in_cfg_file < 32000; binary_id_in_cfg_file++)
  {
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
    }
    else
    {
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
  if (set)
  {
#ifdef VERBOSE
      fprintf(error_stream,"binary %s %llu %llu provides default values %d\n",binary_name,binary_id,get_id(binary_name),is_binary_id_used(binary_id));
#endif
    /* mark binary_id as used if there any setting */
    set_binary_id_used(binary_id,1);
  }

  /* Read Configuration for function in config structure and register it
   * with the binary_id */
  for (function_id_in_cfg_file = 0; function_id_in_cfg_file < 32000; function_id_in_cfg_file++)
  {
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

int adapt_def_region(uint64_t binary_id, const char* rname, uint32_t rid)
{
  uint64_t crid;
  if (function_stacks==NULL)
  {
      fprintf(error_stream,"libadapt: ERROR: function_stacks NULL\n");
      return 1;
  }

  if (!is_binary_id_used(binary_id))
      return 1;

  /* if it is not already registered */
  if (get_rid2crid(binary_id, rid) == NULL)
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
int adapt_enter_stacks(uint64_t binary_id, uint32_t tid, uint32_t rid,int32_t cpu)
{
    return adapt_enter_or_exit(binary_id, tid, rid, cpu, 1, 0);
}

/**
 * Use this if you have only enter handling, but no exit-handling
 */
int adapt_enter_no_stacks(uint64_t binary_id, uint32_t rid,int32_t cpu)
{
    return adapt_enter_or_exit(binary_id, 0, rid, cpu, 0, 0);
}

/**
 * Use this for both with optional stack switch
 */
int adapt_enter_or_exit(uint64_t binary_id, uint32_t tid, uint32_t rid,int32_t cpu, int stack_on, int exit)
{
  int knob_index;
  int ok=0;

#ifdef VERBOSE
  if (stack_on)
    fprintf(error_stream,"Enter test function stacks\n");
#endif

  if (function_stacks == NULL)
  {
#ifdef VERBOSE
    fprintf(error_stream,"libadapt: ERROR: function_stacks NULL\n");
#endif
    return ADAPT_NOT_INITITALIZED;
  }

  /* if the tid bigger than max_threads it woudn't append to the
   * function_stack and so we have nothing to do
   * Maybe the wrong tid was used */
  /* only use with stack_on but for enter_no_stacks tid will be zero an
   * so it will be lower than max_threads */
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

  /* binary not used -> use defaults */
  if ( !is_binary_id_used(binary_id) )
  {
#ifdef VERBOSE
    if (!exit)
        fprintf(error_stream,"Binary not used %llu, applying defaults\n",binary_id);
    else
        fprintf(error_stream,"Binary not used %llu, exit defaults\n",binary_id);
#endif
    if (default_infos)
    {
      for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
      {
        if (!exit)
        {
            if (knobs[knob_index].process_before)
                ok |= knobs[knob_index].process_before(&(default_infos[knob_offsets[knob_index]]),cpu);
        }
        else
        {
            if (knobs[knob_index].process_after)
                ok |= knobs[knob_index].process_after(&(default_infos[knob_offsets[knob_index]]),cpu);
        }
      }

      RETURN_ADAPT_STATUS(ok);
    }
    else
        if (!exit)
            return ADAPT_NO_ACTUAL_ADAPT;
  }
  /* unfortunately the dct exit does not work on all compilers, so we repeat it here :( */
#ifndef NO_DCT
    if (stack_on && !exit)
        omp_dct_repeat_exit();
#endif

  /* we only need the stack if we want to use them and if we don't want
   * to exit */
  if (stack_on && !exit)
  {
    /* add to stack / stack to large? */
    if (function_stacks[tid] == NULL)
        /* if the entry for the tid doesn't exist we create it */
        function_stacks[tid] = calloc(max_function_stack, sizeof(uint32_t));
  }
    /* check for max stack size or the if there any rid on the stack for
     * the specific tid
     * the difference makes the exit switch */
    /* this should be always executed if we don't use the stack
     * but if we use the stack we need to look for the stack size
     * */
  if (!stack_on || (!exit && (function_stack_sizes[tid] < max_function_stack)) || (exit && (function_stack_sizes[tid] - 1) >= 0) )
  {
      /* here will ok be zero, so if something went wrong
       * RETURN_ADAPT_STATUS will see it */
#ifdef VERBOSE
    if (!exit)
    {
        if (stack_on)
            fprintf(error_stream,"Enter %llu %lu %lu %lu\n",binary_id, tid, function_stack_sizes[tid], rid);
        else
            fprintf(error_stream,"Enter %llu %lu\n",binary_id, rid);
    }
    else
        fprintf(error_stream,"Exit %lu %lu %lu \n",tid, function_stack_sizes[tid], function_stacks[tid][function_stack_sizes[tid] - 1]);

#endif
    /* get the constant region id */
    struct rid_to_crid_struct * r2d = NULL;
    /* get the right structure for the rid */
    if (!exit)
        r2d = get_rid2crid(binary_id, rid);
    else
        /* This  function_stacks[tid][function_stack_sizes[tid] - 1] is the saved rid */
        r2d = get_rid2crid(binary_id,function_stacks[tid][function_stack_sizes[tid] - 1]);

    /* any adaption for region defined? */
    if (r2d != NULL)
    {
#ifdef VERBOSE
        fprintf(error_stream,"Crid %lu %llu\n", r2d->rid, r2d->crid);
#endif
        /* do adapt */
        for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
        {
            if (!exit)
            {
                if (knobs[knob_index].process_before)
                    ok |=knobs[knob_index].process_before(&(r2d->infos[knob_offsets[knob_index]]),cpu);
            }
            else
            {
                if (knobs[knob_index].process_after)
                    ok |= knobs[knob_index].process_after(&(r2d->infos[knob_offsets[knob_index]]),cpu);

            }
        }
    }
    /* no definition for reason -> use defaults */
    else
    {
#ifdef VERBOSE
        if (!exit)
            fprintf(error_stream,"Enter binary default\n");
        else
            fprintf(error_stream,"Exit binary default\n");
#endif
        /* get struct for defaults */
        struct added_binary_ids_struct * bid = get_bid(binary_id);

        /* apply default settings for binary */
        for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
        {
            if (!exit)
            {
                if (knobs[knob_index].process_before)
                    ok |= knobs[knob_index].process_before(&(bid->default_infos[knob_offsets[knob_index]]),cpu);
            }
            else
            {
                if (knobs[knob_index].process_after)
                    ok |= knobs[knob_index].process_after(&(bid->default_infos[knob_offsets[knob_index]]),cpu);
            }
        }
    }

    if (!exit)
    {
        if (stack_on)
        {
            /* save the rgeion id for this thread */
            function_stacks[tid][function_stack_sizes[tid]] = rid;
            /* increase stack size immediately afterwards */
            function_stack_sizes[tid]++;
        }
    }
    else
        /* decrease stack size */
        function_stack_sizes[tid]--;
  }

  RETURN_ADAPT_STATUS(ok);
}

int adapt_exit(uint64_t binary_id, uint32_t tid, int32_t cpu)
{
    return adapt_enter_or_exit(binary_id, tid, 0, cpu, 1, 1);
}

void adapt_close()
{
  int knob_index;
  int i;

  /* free the hashmaps */
  /* if the work was already done by another thread, we have nothing to do */
  if (free_hashmaps() == 0)
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

  for (knob_index = 0; knob_index < ADAPT_MAX; knob_index++ )
  {
    if (knobs[knob_index].fini)
      knobs[knob_index].fini();
  }
}

