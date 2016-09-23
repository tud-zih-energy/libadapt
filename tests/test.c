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

#include <stdlib.h>
#include <stdint.h>
#include <omp.h>
#include <stdio.h>
#include <string.h>

#include "adapt.h"

#ifdef X86_ADAPT
#include "x86_adapt.h"
#endif

/* hardcoded path to control the frequency scaling */
#define SCALING_CUR_FREQ "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"

/* Print nice header for a category */
void
print_category(int message, char * category)
{
    if ( message == 1 )
        { printf("\n*********\n"); printf("Test %s", category); printf("\n*********\n"); }
}

/* Make adapt_def_region compact */
int
def_region(int message, char * category, uint64_t bid, int rid)
{
    int error;
    char * prefix = "test_";

    /* string magic to concatenate the string for the region name */
    char * buffer = calloc(strlen(prefix) + strlen(category) + 1, sizeof(char));
    strncpy(&buffer[0], prefix, strlen(prefix));
    strncpy(&buffer[strlen(prefix)], category, strlen(category));
    buffer[strlen(prefix) + strlen(category) + 1] = '\0';

    if ( message == 1 ) printf("\nDefine region\n");
    error = adapt_def_region(bid, buffer, rid);
    if ( message == 1 ) printf("Returned error code: %d\n", error);

    /* free the buffer for the region name */
    free(buffer);

    return error;
}

/* Make adapt_enter_stacks compact */
int
enter_stacks(int message, uint64_t bid, int rid)
{
    int error;

    if ( message == 1 ) printf("\nEnter region with stacks\n");
    error = adapt_enter_stacks(bid, 0, rid, 0);
    if ( message == 1 ) printf("Returned error code: %d\n", error);

    return error;
}

/* Make adapt_exit compact */
int
exit_stacks(int message, uint64_t bid, int rid)
{
    int error;

    if ( message == 1 ) printf("\nExit region\n");
    error = adapt_exit(bid, 0, 0);
    if ( message == 1 ) printf("Returned error code: %d\n", error);

    return error;
}

/* Print out a file like cat */
int
cat(char * filename, int overhead)
{
    int c;
    int i = 0;
    FILE *file;

    /* open the given file readonly */
    file = fopen(filename, "r");

    /* we can read the file if it's not a NULL-pointer */
    if (file) {
        /* loop over all characters until the EOF */
        while ((c = fgetc(file)) != EOF)
        {
            /* it is possible to skip a specific number of chars */
            if ( overhead > 0 )
            {
                overhead--;
                continue;
            }
            /* for cleaner output we replace every newline with a tab */
            if ( c == '\n' )
                c = '\t';
            /* now we can put the char to stdout */
            putchar(c);

            /* we count how many chars we give back */
            i++;
        }
        /* after EOF we can close the file */
        fclose(file);
    }
    else
        /* if something went wront we return an negative number because the file
         * can be empty and so i would be 0 */
        return -1;

    /* give the number of chars back */
    return i;
}

/* Print number of threads for testing */
int
print_threads(void)
{
    int lock = 0;

    /* lock is shared with all threads */
    #pragma omp parallel shared(lock)
    {
        /* this code is only exexcuted from one thread at the same time */
        #pragma omp critical
        {
            if ( lock != 1 )
                { printf("%d", omp_get_num_threads()); lock = 1; }
        }
    }
}

/* Start with the test environment */

/* threads/dct behavior */
int
test_dct(uint64_t bid, int message)
{
    char * category = "dct";
    int error = 0;
    uint32_t rid = 1;
    
    print_category(message, category);

    if ( message == 1 ) printf("Threads:\n");
    printf("init_dct_threads_before=");
    print_threads();
    printf("\n");

    error |= def_region(message, category, bid, rid);

    error |= enter_stacks(message, bid, rid);

    if ( message == 1 ) printf("Threads:\n");
    printf("bin_dct_threads_before=");
    print_threads();
    printf("\n");

    error |= exit_stacks(message, bid, rid);

    if ( message == 1 ) printf("Threads:\n");
    printf("bin_dct_threads_after=");
    print_threads();
    printf("\n");

    if ( message == 1 ) printf("*********\n");

    return error;
}

/* frequency/dvfs behavior */
int
test_dvfs(uint64_t bid, int message)
{
    char * category = "dvfs";
    int error = 0;
    uint32_t rid = 2;

    print_category(message, category);

    if ( message == 1 ) printf("\nFile %s \n", SCALING_CUR_FREQ);

    if ( message == 1 ) printf("\nFreq:\n");
    printf("init_dvfs_freq_before=");
    cat(SCALING_CUR_FREQ, 0);
    printf("\n");

    error |= def_region(message, category, bid, rid);

    error |= enter_stacks(message, bid, rid);

    if ( message == 1 ) printf("\nFreq:\n");
    printf("bin_dvfs_freq_before=");
    cat(SCALING_CUR_FREQ, 0);
    printf("\n");

    error |= exit_stacks(message, bid, rid);

    if ( message == 1 ) printf("\nFreq:\n");
    printf("bin_dvfs_freq_after=");
    cat(SCALING_CUR_FREQ, 0);
    printf("\n");

    if ( message == 1 ) printf("*********\n");

    return error;
}

/* file behavior */
int
test_file(uint64_t bid, int message, char * filename)
{
    char * category = "file";
    int error = 0, overhead = 0;
    uint32_t rid = 4;

    print_category(message, category);

    error |= def_region(message, category, bid, rid);

    error |= enter_stacks(message, bid, rid);

    if ( message == 1 ) printf("\nFile %s:\n", filename);
    printf("bin_file_before=");
    overhead = cat(filename, 0);
    printf("\n");

    if ( overhead == -1 )
        fprintf(stderr, "Error: %s not readable\n", filename);
    
    error |= exit_stacks(message, bid, rid);

    if ( message == 1 ) printf("\nFile %s:\n", filename);
    printf("bin_file_after=");
    cat(filename, overhead);
    printf("\n");

    if ( message == 1 ) printf("*********\n");

    return error;
}

#ifdef X86_ADAPT
/* frequency/dvfs behavior */
int
test_x86_adapt(uint64_t bid, int message, char * option)
{
    char * category = "x86_adapt";
    int error = 0;
    uint32_t rid = 8;

    int fd, index;
    uint64_t setting;

    print_category(message, category);

    error |= x86_adapt_init();
    if ( message == 1  ) printf("Init returned error code: %d\n", error);
    fd = x86_adapt_get_device(X86_ADAPT_CPU, 0);
    if ( fd < 0 ) error |= 1;
    index = x86_adapt_lookup_ci_name(X86_ADAPT_CPU, option);

    error |= def_region(message, category, bid, rid);

    error |= enter_stacks(message, bid, rid);

    if ( message == 1 ) printf("\nOption: %s\n", option);
    printf("bin_x86_adapt_before=");
    if ( x86_adapt_get_setting(fd, index, &setting) < 0 ) error |= 1;
    printf("%" PRIu64, setting);
    printf("\n");

    error |= exit_stacks(message, bid, rid);

    if ( message == 1 ) printf("\nOption: %s\n", option);
    printf("bin_x86_adapt_after=");
    if ( x86_adapt_get_setting(fd, index, &setting) < 0 ) error |= 1;
    printf("%" PRIu64, setting);
    printf("\n");

    x86_adapt_set_setting(fd, index, 0);

    x86_adapt_finalize();

    if ( message == 1 ) printf("*********\n");

    return error;
}
#endif


/*Main funtion to parse command line arguments and execute specific tests */
int
main(int argc, char **argv)
{
    /* standard filename for the test file to read */
    char * filename = "./test_file";

    /* standard option for x86_adapt */
    char * option = "Intel_Clock_Modulation";

    /* const variables for better code */
    static const int verbose = 1, machine = 2;
    int message = verbose;

    static int dct = 1, dvfs = 2, file = 4;
#ifdef X86_ADAPT
    static int x86_adapt = 8;
#else
    static int x86_adapt = 0;
#endif

    int test = 0;

    int error = 0;

    /* parse command line arguments */
    while(*++argv)
    {
        if ( strcmp(*argv, "help") == 0 )
        {
            printf("Use verbose, machine or simply nothing as parameter");
            printf("You can also add the unit that should be tested, like) == 0 )");
            printf("%s message dct", argv[0]);
            printf("Possibile values are) == 0 ) all, dct, dvfs and file");
            return 0;
        }
        else if ( strcmp(*argv, "verbose") == 0 )
            message = verbose;
        else if ( strcmp(*argv, "machine") == 0 )
            message = machine;
        else if ( strcmp(*argv, "dct") == 0 )
            test += dct;
        else if ( strcmp(*argv, "dvfs") == 0 )
            test += dvfs;
        else if ( strcmp(*argv, "file") == 0 )
            test += file;
        else if ( strcmp(*argv, "x86_adapt") == 0  )
            test += x86_adapt;
        else if ( strcmp(*argv, "all") == 0 )
            test = dct + dvfs + file + x86_adapt;
        else if ( strstr(*argv, "/") != NULL )
            filename = *argv;
        else if ( strstr(*argv, "x86_adapt_") != NULL  )
        {
            option = strstr(*argv, "x86_adapt_");
            option = &option[strlen("x86_adapt_")];
        }
    }
    /* if atfer all this the test scenario wasn't set we will set it */
    if ( test == 0 )
        test = dct + dvfs + file + x86_adapt;

    /* open adapt and init everything */
    if (message == 1) printf("\nOpen adapter \n");
    error |= adapt_open();
    if (message == 1) printf("Returned error code: %d\n", error);

    /* get binary id */
    if ( message == 1 ) printf("\nAdd binary \n");
    uint64_t bid = adapt_add_binary("test");

#ifdef X86_ADAPT
    /* test for x86_adapt_behavior */
    if ( test - x86_adapt >= 0 )
    {
        error |= test_x86_adapt(bid, message, option);
        test -= x86_adapt;
    }
#endif

    /* test for file handling */
    if ( test - file >= 0 )
    {
        error |= test_file(bid, message, filename);
        test -= file;
    }

    /* test for dvfs or frequency scaling */
    if ( test - dvfs >= 0)
    {
        error |= test_dvfs(bid, message);
        test -= dvfs;
        dvfs = 0;
    }

    /* test for dct or thread handling */
    if ( test - dct >= 0 )
    {
        error |= test_dct(bid, message);
        test -= dct;
        dct = 0;
    }
    
    /* test should be zero after all of these tests otherwise something went
     * wrong */
    if ( test != 0 )
        error |= 2;

    /* after our work we need to clean up */
    if ( message == 1 ) printf("\nClose adapter\n");
    adapt_close();

    /* and we should control if everything is set correctly after close */
    print_category(message, "after closing");

    if ( dvfs == 0  )
    {
        if ( message == 1 ) { printf("Test dvfs\n"); printf("Freq:\n"); }
        printf("init_dvfs_freq_after=");
        cat(SCALING_CUR_FREQ, 0);
        printf("\n");
        if ( message == 1 ) printf("\n");
    }

    if ( dct == 0 )
    {
        if ( message == 1 ) { printf("Test dct\n"); printf("Threads:\n"); }
        printf("init_dct_threads_after=");
        print_threads();
        printf("\n");
    }

    /* give back an good error code */
    return error;
}

