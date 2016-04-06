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

/*************************************************************/
/**
* @file binary_handling.h
* @brief Header File for libadapts internal binary handling
*
* @author Sven Schiffner Sven.Schiffner@mailbox.tu-dreden.de
*
* libadapt
*
* @version 0.4
* 
*************************************************************/
#ifndef BINARY_HANDLING_H_
#define BINARY_HANDLING_H_

#include <stdint.h>
#include <errno.h>
#include <stdlib.h>

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

/**
 * @brief Get an ID for a given name
 *
 * Use this function to get an ID for a given name, calculated with
 * MurmurHash64A for a 64bit system. The IDs are used for e.g.
 * regions or binarys in the hashmaps
 * @param name that should be hashed
 * @return ID for the given name
 * */
uint64_t get_id(const char * name);

/**
 * @brief Initialize hashmaps
 *
 * Function to allocate hasmaps for further use
 * @param hash_size to use a specific hash_size, default will be 101
 * @param information_size the size of our adapt informations used by
 * add_binary_id for allocation
 * @return 0 if the init as sucessfully
 * */
int init_hashmaps(uint32_t hash_size, size_t information_size);

/**
 * @brief Add a Binary ID to the Hashmap
 *
 * Function to add a given binary id to the hashmap if the id not
 * already exists
 * @param binary_id the ID generated with adapt_add_binary()
 * @return pointer to the initialized added_binary_ids_struct
 * to save settings in there
 * */
struct added_binary_ids_struct * add_binary_id(uint64_t binary_id);

/**
 * @brief Get the Settings for the Binary ID
 *
 * Function to get the previous saved settings from the given binary
 * id from the hashmap
 * @param binary_id the ID generated with adapt_add_binary()
 * @return pointer to the added_binary_ids_struct for the binary id
 * <br> NULL if the binary id not exists
 * */
struct added_binary_ids_struct * get_bid(uint64_t binary_id);

/**
 * @brief Test if Binary ID exists
 *
 * Function to proof if the given binary id exists in the hashmap
 * @param binary_id the ID generated with adapt_add_binary()
 * @return 1 if the binary exists <br>
 * 0 if the binary doesn't exist
 * */
int exists_binary_id(uint64_t binary_id);

/**
 * @brief Test if Binary ID is used
 *
 * add_binary_id() will mark the binary as used if there exist any
 * settings in the config file for this binary name
 * @param binary_id the ID generated with adapt_add_binary()
 * @return used status <br>
 * 0 if the binary doesn't exist
 * */
int is_binary_id_used(uint64_t binary_id);

/**
 * @brief Set the Binary ID to a Used sate
 *
 * Set the Used state for the binary to an given integer
 * @param binary_id the ID generated with adapt_add_binary()
 * @param used the state to set
 * */
void set_binary_id_used(uint64_t binary_id,int used);

/**
 * @brief Add the given settings to the Constant Region ID
 *
 * Function to at the given settings from the configuration file to the
 * hashmap for the Constant Region ID
 * @param binary_id the ID generated with adapt_add_binary()
 * @param crid hashed function or region name
 * @param tmp_crid_to_config_struct settings that should be apply
 * @return pointer to the crid_to_config_struct wit the given crid and
 * binary id
 * */
struct crid_to_config_struct * add_crid2config(uint64_t binary_id, uint64_t crid, struct crid_to_config_struct * tmp_crid_to_config_struct);

/**
 * @brief define a region id for a constant region id
 * 
 * There are constant region IDs, e.g. the hash of a name of a function that is entered and region ids that can change from run to run (e.g. the measurement systems id or handle of that region)
 * this function checks whether there is some adaption to process for a specific region.
 * if so, it adds the region id to the processed region ids.
 * @param binary_id the id of a binary retrieved with adapt_add_binary()
 * @param rid the variable region id
 * @param crid the constant region id
 * @return 1 if there is no adaption definition or the binary id is not available or the region id is already registered<br>
 * 1 if the region has been added
 */
int add_rid2crid(uint64_t binary_id,uint32_t rid,uint64_t crid);

/**
 * @brief Get an Constant Region ID for the Region ID
 *
 * Function to get the constant region id for the given region id
 * @param binary_id the id of a binary retrieved with adapt_add_binary()
 * @param rid the variable region id
 * @return pointer to the rid_to_crid_struct that matches the region id
 * <br> NULL if the binary id and region id pair doesn't exist
 * */

struct rid_to_crid_struct * get_rid2crid(uint64_t binary_id,uint32_t rid);

/**
 * @brief Test for regular expressions
 *
 * We allow regular expressions in the binary name, so we need to test
 * for them
 * @param pattern that should be searched in the string
 * @param string that should be compared
 * @return 0 if something went wrong or the pattern was not found <br>
 * 1 if the pattern was found in the string
 * */
int regex_match(const char *pattern, char *string);

/**
 * @brief Free the used hashmaps
 *
 * Function to free th internal used hashmaps
 * @return 0 if the hasmaps was already freed <br>
 * 1 if everything is free
 * */
int free_hashmaps(void);

#endif /* BINARY_HANDLING_H_ */
