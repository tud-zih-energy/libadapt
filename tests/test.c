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

#include <stdint.h>
#include <omp.h>
#include <stdio.h>
#include <string.h>

#include "adapt.h"

#define SCALING_CUR_FREQ "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"

/* Function to print out a file */
int
cat(char * filename)
{
    int c;
    FILE *file;
    file = fopen(filename, "r");

    if (file) {
        while ((c = getc(file)) != EOF)
            putchar(c);
        fclose(file);
    }
    else
        return 1;

    return 0;
}

/* Function to do print threads for testing */
int
print_threads(int message)
{
    message = 2;
    if ( message == 1 )
        printf("Thread %d from %d \n", omp_get_thread_num() + 1, omp_get_num_threads());
    else
        printf("Threads: %d \n", omp_get_num_threads());
}

/* Function to test dct behavior */
void
test_dct(uint64_t bid, int message)
{
    int error;
    uint32_t rid = 42;

    printf("\n"); printf("Define region \n");
    error = adapt_def_region(bid, "test_dct", rid);
    printf("Returned error code: %d\n", error);

#pragma omp parallel
    {
        // test thread behavior
        print_threads(message);
    }

    printf("\n"); printf("Enter region with stacks in Thread: %i \n", omp_get_thread_num());
    error = adapt_enter_stacks(bid, omp_get_thread_num(), rid, 0);
    printf("Returned error code: %d\n", error);

#pragma omp parallel
    {
        // test thread behavior
        print_threads(message);
    }

    printf("\n"); printf("Exit region in Thread %i \n", omp_get_thread_num());
    error = adapt_exit(bid, omp_get_thread_num(), 0);
    printf("Returned error code: %d\n", error);

#pragma omp parallel
    {
        // test thread behavior
        print_threads(message);
    }

}

/* Function to test file behavior */
void
test_file(uint64_t bid, int message)
{
    int error;
    uint32_t rid = 1;
    char filename[] = "./test_file";

    printf("\n"); printf("Define region \n");
    error = adapt_def_region(bid, "test_file", rid);
    printf("Returned error code: %d\n", error);
    
    printf("\n"); printf("Enter region with stacks \n");
    error = adapt_enter_stacks(bid, 0, rid, 0);
    printf("Returned error code: %d\n", error);

    printf("\n"); printf("File %s \n", filename);
    cat(filename);

    printf("\n"); printf("Exit region \n");
    error = adapt_exit(bid, 0, 0);
    printf("Returned error code: %d\n", error);
    

    printf("\n"); printf("File %s \n", filename);
    cat(filename);

}

/* Function to tes frequency behavior */
void
test_dvfs(uint64_t bid, int message)
{
    int error;
    uint32_t rid = 2;

    printf("\n"); printf("Define region \n");
    error = adapt_def_region(bid, "test_dvfs", rid);
    printf("Returned error code: %d\n", error);

    printf("\n"); printf("File %s \n", SCALING_CUR_FREQ);
    cat(SCALING_CUR_FREQ);
    printf("\n");

    printf("\n"); printf("Enter region with stacks \n");
    error = adapt_enter_stacks(bid, 0, rid, 0);
    printf("Returned error code: %d\n", error);

    printf("\n"); printf("File %s \n", SCALING_CUR_FREQ);
    cat(SCALING_CUR_FREQ);
    printf("\n");

    printf("\n"); printf("Exit region \n");
    error = adapt_exit(bid, 0, 0);
    printf("Returned error code: %d\n", error);

    printf("\n"); printf("File %s \n", SCALING_CUR_FREQ);
    cat(SCALING_CUR_FREQ);
    printf("\n");
}


int
main(int argc, char **argv)
{
    /* const variables for better code */
    static const int verbose = 1;
    static const int quiet = 2;
    static const int machine = 4;
    int message = verbose;

    static int dct = 1;
    static int dvfs = 2;
    static int file = 4;
    int test = 0;

    int error = 0;

    /* parse command line arguments */
    while(*++argv)
    {
        if ( strcmp(*argv, "help") == 0 )
        {
            printf("Use verbose, quiet, machine or simply nothing as parameter");
            printf("You can also add the unit that should be tested, like) == 0 )");
            printf("%s message dct", argv[0]);
            printf("Possibile values are) == 0 ) all, dct, dvfs and file");
            return 0;
        }
        else if ( strcmp(*argv, "machine") == 0 )
            message = machine;
        else if ( strcmp(*argv, "verbose") == 0 )
            message = verbose;
        else if ( strcmp(*argv, "quiet") == 0 )
            message = quiet;
        else if ( strcmp(*argv, "dct") == 0 )
            test += dct;
        else if ( strcmp(*argv, "dvfs") == 0 )
            test += dvfs;
        else if ( strcmp(*argv, "file") == 0 )
            test += file;
        else if ( strcmp(*argv, "all") == 0 )
            test = dct + dvfs + file;
    }
    /* if atfer all this the test scenario wasn't set we will set ist */
    if ( test == 0 )
        test = dct + dvfs + file;

    /* open adapt and init anything */
    printf("Open adapter \n");
    error = adapt_open();
    printf("Returned error code: %d\n", error);

    /* get binary id */
    if ( message == 1 )
        printf("\n"); printf("Add binary \n");
    uint64_t bid = adapt_add_binary("test");

    /* needed to be able to set the number of threads */
    omp_set_dynamic(1);

    /* test for file handling */
    if ( test - file >= 0 )
    {
        if ( message == 1 )
        {
            printf("\n"); printf("*********\n"); printf("Test file \n"); printf("*********\n");
        }
        test_file(bid, message);
        if ( message == 1 )
            printf("*********\n");
        test -= file;
    }

    /* test for dvfs or frequency scaling */
    if ( test - dvfs >= 0)
    {
        if ( message == 1 )
        {
            printf("\n"); printf("*********\n"); printf("Test dvfs \n"); printf("*********\n");
        }
        test_dvfs(bid, message);
        if ( message == 1 )
            printf("*********\n");
        test -= dvfs;
        dvfs = 0;
    }

    /* test for dct or thread handling */
    if ( test - dct >= 0 )
    {
        if ( message == 1 )
        {
            printf("\n"); printf("*********\n"); printf("Test dct \n"); printf("*********\n");
        }
        test_dct(bid, message);
        if ( message == 0   )
            printf("*********\n");
        test -= dct;
        dct = 0;
    }
    
    /* test should be zero after all of these tests otherwise something went
     * wrong */
    if ( test != 0 )
        error = 2;

    /* after our work we need to clean up */
    if ( message == 1 )
        printf("\n"); printf("Close adapter \n");
    adapt_close();

    if ( message == 1 )
    {
        printf("\n"); printf("*********\n"); printf("Test after closing \n"); printf("*********\n");
    }

    if ( dvfs == 0  )
    {
        printf("dvfs");
        printf("\n"); printf("File %s \n", SCALING_CUR_FREQ);
        cat(SCALING_CUR_FREQ);
        printf("\n\n");
    }

    if ( dct == 0 )
    {
        printf("dct \n");
#pragma omp parallel
        {
            // test thread behavior
            print_threads(message);
        }
    }
    /* give back an good error code */
    return error;
}

