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

/* requied for strdup */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <cpufreq.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* #define VERBOSE 1 */

/* find the greatest common divisor, if x == 0 it returns y */
static unsigned long gcd(unsigned long x, unsigned long y) {
    unsigned long r;
    assert(y > 0);

    while ((r = x % y) != 0) {
        x = y;
        y = r;
    }
    return y;
}

typedef struct lenstr {
    char*         str;
    size_t        len;
    unsigned long freq;
} lenstr;

static unsigned int num_cpus = 0;
static lenstr* freq_lenstr_map      = NULL;
int *freq_fds                       = NULL;
static unsigned long *freq_settings = NULL;
static unsigned long freq_gcd       = 0;
static size_t num_freq_bins         = 0;
static unsigned long freq_turbo     = 0;

static int initialized = 0;

static size_t freq_index(const unsigned long frequency) {
    if (frequency == freq_turbo) {
        return 0;
    }
    assert(frequency % freq_gcd == 0);
    size_t idx = frequency / freq_gcd;
    assert(idx < num_freq_bins);
    return idx;
}

static void freq_str_init() {
    /* TODO: Check if other cpus have same frequencies */
    char freq_str[10];
    struct cpufreq_available_frequencies* caf_first = cpufreq_get_available_frequencies(0);
    assert(caf_first);
    struct cpufreq_available_frequencies* caf = caf_first;
    unsigned long max_frequency = 0;
    for (; caf != NULL; caf = caf->next) {
        if (caf->frequency % 10000 == 1000) {
            assert(0 == freq_turbo);
            freq_turbo = caf->frequency;
#ifdef VERBOSE
            printf("fcf: turbo %lu\n", freq_turbo);
#endif            
        }
        else {
            if (caf->frequency > max_frequency) {
                max_frequency = caf->frequency;
            }
            freq_gcd = gcd(freq_gcd, caf->frequency);
#ifdef VERBOSE
            printf("fcf: freq %lu, gcd %lu\n", caf->frequency, freq_gcd);
#endif
        }
    }
    num_freq_bins = 1 + (max_frequency / freq_gcd);
#ifdef VERBOSE
    printf("fcf: detected %zu frequency bins (gdc %lu) with maximum of %lu and %lu turbo\n", num_freq_bins, freq_gcd, max_frequency, freq_turbo);
#endif
    freq_lenstr_map = calloc(num_freq_bins, sizeof(*freq_lenstr_map));
    assert(freq_lenstr_map);
    for (caf = caf_first; caf != NULL; caf = caf->next) {
        snprintf(freq_str, sizeof(freq_str), "%lu", caf->frequency);
        char* str = strdup(freq_str);
        size_t idx = freq_index(caf->frequency);
        freq_lenstr_map[idx].str = str;
        freq_lenstr_map[idx].len = strlen(str);
        freq_lenstr_map[idx].freq = caf->frequency;
    }
    cpufreq_put_available_frequencies(caf_first);
}

static void freq_str_cleanup() {
    for (size_t i = 0; i < num_freq_bins; i++) {
        if (freq_lenstr_map[i].str) {
            free(freq_lenstr_map[i].str);
            freq_lenstr_map[i].str = 0;
            freq_lenstr_map[i].len = 0;
        }
    }
    free(freq_lenstr_map);
    freq_lenstr_map = NULL;
    freq_gcd        = 0;
    num_freq_bins       = 0;
}


/* The asserts have been removed. 
 * The state of the initialization is checked in fcf_set_frequency() as single entry point 
 */

static inline const lenstr* freq_get_lenstr(const unsigned long freq) {
    const lenstr* ls = &freq_lenstr_map[freq_index(freq)];
    return ls;
}

static inline int freq_get_fd(const int cpu) {
    const int fd = freq_fds[cpu];
    return fd;
}

#define PATH_TO_CPU "/sys/devices/system/cpu"
static void freq_fds_init() {
    char path[255];
    freq_fds = calloc(num_cpus, sizeof(*freq_fds));
    assert(freq_fds);
    for (unsigned cpu = 0; cpu < num_cpus; cpu++) {
        snprintf(path, sizeof(path),
                 PATH_TO_CPU "/cpu%u/cpufreq/scaling_setspeed",
                 cpu);
        freq_fds[cpu] = open(path, O_WRONLY);
        assert(freq_fds[cpu] != -1);
    }
}

static void freq_fds_cleanup() {
    for (unsigned cpu = 0; cpu < num_cpus; cpu++) {
        close(freq_fds[cpu]);
    }
    free(freq_fds);
}


long fcf_set_frequency(unsigned int cpu, unsigned long target_frequency) {
    
    if (!initialized) {
        return -1;
    }
    if (cpu >= num_cpus) {
        return -2;
    }
    
    if (freq_settings[cpu] == target_frequency) {
        return target_frequency;
    }

    const int fd     = freq_get_fd(cpu);
    const lenstr* ls = freq_get_lenstr(target_frequency);
#ifdef VERBOSE
    fprintf(stderr,"Setting frequency to %li %s!\n",target_frequency,ls->str);
#endif
    const ssize_t ret    = pwrite(fd, ls->str, ls->len, 0);
    if (ret != (ssize_t)ls->len) {
        fprintf(stderr, "libadapt ERROR: Failed to set frequency for cpu %d to %lu/'%s' (%zu): %s\n", cpu, target_frequency, ls->str, ls->len, strerror(errno));
        return -1;
    }
    freq_settings[cpu] = target_frequency;
#ifdef VERBOSE
    fprintf(stderr,"Return %li!\n",target_frequency);
#endif
    return target_frequency;
}


int fcf_init_once() {
    static int called = 0;
    int ret;
    if (called) {
        return 0;
    }
    called = 1;
    
    /* TODO: what to do with broken setups, e.g. the Haswell platform, 
     * where CPUs are not continuously numbered?
     *
     * Possible solution to parse /proc/cpuinfo
     * No Computer with discontinously cpu numbers found */
    num_cpus = sysconf(_SC_NPROCESSORS_CONF);
    for (unsigned cpu = 0; cpu < num_cpus; cpu++) {
        /* Missing cpu numbers are no problem for 
         * cpufreq_modify_policy_governor */
        ret = cpufreq_modify_policy_governor(cpu, "userspace");
        if (ret)
          return ret;
    }
    freq_settings = calloc(num_cpus, sizeof(*freq_settings));
    if (freq_settings == NULL)
        return ENOMEM;
    freq_str_init();
    freq_fds_init();
    initialized = 1;
    return 0;
}

int fcf_finalize() {
    freq_fds_cleanup();
    freq_str_cleanup();
    free(freq_settings);
    initialized = 0;
    return 0;
}
