
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

#include <regex.h>
#include <string.h>
#include <execinfo.h>

#include "binary_handling.h"


/* maps described in struct definition */
static struct crid_to_config_struct * c2conf_hashmap = NULL;
static struct rid_to_crid_struct * r2c_hashmap = NULL;
static struct added_binary_ids_struct * bids_hashmap = NULL;

/* default value for hash size */
static uint32_t hash_set_size = 101;

static size_t adapt_information_size;

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


/* unsusable stuff in the case that not enough memory is avaible 
 * */
#define CHECK_INIT_MALLOC_HASHMAPS(a) if( (a) == NULL) { \
    CHECK_INIT_MALLOC_FREE(c2conf_hashmap); \
    CHECK_INIT_MALLOC_FREE(r2c_hashmap); \
    CHECK_INIT_MALLOC_FREE(bids_hashmap); \
    return ENOMEM; \
}
/**
 * MurmurHash2, 64-bit versions, by Austin Appleby
 *
 * The same caveats as 32-bit MurmurHash2 apply here - beware of alignment
 * and endian-ness issues if used across multiple platforms.
 *
 * 64-bit hash for 64-bit platforms
 * */

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

/* wrapper function for MurmurHash64A */
uint64_t get_id(const char * name)
{
    return MurmurHash64A(name, strlen(name), 0);
}

int init_hashmaps(uint32_t hash_size, size_t information_size)
{
    /* set global hash size */
    if (hash_size != 0)
        hash_set_size = hash_size;
    /* set global information_size for adapt */
    adapt_information_size = information_size;

    /* create hashmaps */
    CHECK_INIT_MALLOC_HASHMAPS(c2conf_hashmap=calloc(hash_set_size,sizeof(struct crid_to_config_struct)));
    CHECK_INIT_MALLOC_HASHMAPS(r2c_hashmap=calloc(hash_set_size,sizeof(struct rid_to_crid_struct)));
    CHECK_INIT_MALLOC_HASHMAPS(bids_hashmap=calloc(hash_set_size,sizeof(struct added_binary_ids_struct)));

    return 0;
}

struct added_binary_ids_struct * add_binary_id(uint64_t binary_id)
{
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

struct added_binary_ids_struct * get_bid(uint64_t binary_id)
{
    struct added_binary_ids_struct * current=&bids_hashmap[binary_id%hash_set_size];
    while (current->next){
        if ((current->binary_id==binary_id)) return current;
        current=current->next;
    }
    if ((current->binary_id==binary_id)) return current;
    return NULL;

}

int exists_binary_id(uint64_t binary_id)
{
    if (get_bid(binary_id)) return 1;
    return 0;
}

int is_binary_id_used(uint64_t binary_id)
{
    struct added_binary_ids_struct * current=&bids_hashmap[binary_id%hash_set_size];
    while (current->next){
        if ((current->binary_id==binary_id)) return current->used;
        current=current->next;
    }
    if ((current->binary_id==binary_id)) return current->used;
    return 0;
}

void set_binary_id_used(uint64_t binary_id,int used)
{
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
struct crid_to_config_struct * add_crid2config(uint64_t binary_id,uint64_t crid,struct crid_to_config_struct * tmp_crid_to_config_struct)
{
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
struct crid_to_config_struct * get_crid2config(uint64_t binary_id,uint64_t crid)
{
    struct crid_to_config_struct * current=&c2conf_hashmap[crid%hash_set_size];
    while (current->next){
        if ((current->binary_id==binary_id)&&(current->crid==crid)) return current;
        current=current->next;
    }
    if ((current->binary_id==binary_id)&&(current->crid==crid)) return current;
    return NULL;
}

/* define a region id for a constant region id */
int add_rid2crid(uint64_t binary_id,uint32_t rid,uint64_t crid)
{
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
struct rid_to_crid_struct * get_rid2crid(uint64_t binary_id,uint32_t rid)
{
    struct rid_to_crid_struct * current=&r2c_hashmap[rid%hash_set_size];
    while (current->next){
        if ((current->binary_id==binary_id)&&(current->rid==rid)) return current;
        current=current->next;
    }
    if ((current->binary_id==binary_id)&&(current->rid==rid)) return current;
    return NULL;
}

int regex_match(const char *pattern, char *string)
{
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

int free_hashmaps(void)
{
    int i;

    /* memory was already freed or not initialized */
    if (c2conf_hashmap == NULL)
        return 0;

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

    /* everything works */
    return 1;

}
