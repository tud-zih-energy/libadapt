/*
 * x86_adapt_items.h
 *
 *  Created on: 24.10.2012
 *      Author: rschoene
 */

#ifndef X86_ADAPT_ITEMS_H_
#define X86_ADAPT_ITEMS_H_

#include <stdint.h>
#include <x86_adapt.h>

struct pref_setting_ids {
  uint64_t setting;
  int16_t id;
  int16_t device_type;
};

struct x86_adapt_pref_information {
  int8_t nr_settings_before;
  struct pref_setting_ids * settings_before;
  int8_t nr_settings_after;
  struct pref_setting_ids * settings_after;
  int8_t nr_settings_before_all;
  struct pref_setting_ids * settings_before_all;
  int8_t nr_settings_after_all;
  struct pref_setting_ids * settings_after_all;
};

#define X86_ADAPT_PREF_CONFIG_STRING "x86_adapt"

int x86_adapt_read_from_config(void * info,struct config_t * cfg, char * buffer,
                               char * prefix);

int x86_adapt_reset(void);

int x86_adapt_process_before(void * info,int32_t cpu);
int x86_adapt_process_after(void * info,int32_t cpu);

char * x86_adapt_get_setting_string(uint64_t id,int length);
int x86_adapt_set_id(uint64_t setting,int length);
uint64_t x86_adapt_get_id(int length);

#endif /* X86_ADAPT_ITEMS_H_ */
