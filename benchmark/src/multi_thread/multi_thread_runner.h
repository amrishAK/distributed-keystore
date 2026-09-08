#ifndef MULTI_THREAD_RUNNER_H
#define MULTI_THREAD_RUNNER_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "multi_thread_models.h"

typedef struct
{
    const mt_scenario *scenario;
    const char *run_id;
    unsigned int run_index;
    double elapsed_seconds;
    double mean_ops_sec;
    double median_ops_sec;
    double stdev_ops_sec;
    double cv_ops_sec;
    double throughput_ops_sec;
    unsigned long rss_delta_kb;
    unsigned long peak_rss_kb;
    unsigned long timed_operations;
    unsigned long set_operations;
    unsigned long get_operations;
    unsigned long delete_operations;
    unsigned long failure_count;
    unsigned long missing_count;
    unsigned long resize_count;
    unsigned int max_chain_depth;
    const char *resize_events_timeline;
    bool verification_passed;
    bool latency_measured;
    uint64_t latency_p50_ns;
    uint64_t latency_p95_ns;
    uint64_t latency_p99_ns;
    uint64_t latency_max_ns;
} mt_result;

int run_multi_thread_scenario(const mt_scenario *scenario, mt_result *result);
void print_multi_thread_result_json(FILE *stream, const mt_result *result);

#endif
