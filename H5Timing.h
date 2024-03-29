#ifndef H5_TIMING_H

#define H5_TIMING_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <string>
#undef H5_TIMING_ENABLE

#ifdef H5_TIMING_ENABLE
typedef struct H5Timer{
    double start;
    double end;
    size_t data_size;
    char *name;
} H5Timer;

typedef struct H5TimerClass{
    std::vector<H5Timer> *dataset_timers;
    std::vector<H5Timer> *dataset_sz_timers;
    std::vector<H5Timer> *dataset_read_timers;
    std::vector<H5Timer> *dataset_sz_read_timers;
    int H5Dwrite_count;
    int H5Dread_count;

    double H5Dclose_time;
    double wrap_requests_time;
    double merge_requests_time;
    double H5Dwrite_time;
    double H5Dread_time;
    double total_start_time;
    double total_end_time;
} H5TimerClass;

int init_timers();
int register_timer_start(double *start_time);
int register_merge_requests_timer_end(double start_time);
int register_wrap_requests_timer_end(double start_time);
int register_H5Dclose_timer_end(double start_time);
int register_H5Dwrite_timer_end(double start_time);

int register_dataset_timer_start(const char *name);
int register_dataset_timer_end(size_t data_size);
int register_dataset_sz_timer_start(const char *name);
int register_dataset_sz_timer_end(size_t data_size);
int register_dataset_read_timer_start(const char *name);
int register_dataset_read_timer_end(size_t data_size);
int register_dataset_sz_read_timer_start(const char *name);
int register_dataset_sz_read_timer_end(size_t data_size);
int increment_H5Dwrite();
int increment_H5Dread();
int finalize_timers();
#endif

#endif
