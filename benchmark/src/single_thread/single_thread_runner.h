#ifndef SINGLE_THREAD_RUNNER_H
#define SINGLE_THREAD_RUNNER_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "single_thread_models.h"

typedef struct
{
    const bench_scenario *scenario;
    const char *run_id;
    unsigned int run_index;
    double elapsed_seconds;
    size_t measured_operations;
    double throughput_ops_sec;
    double mean_ops_sec;
    double median_ops_sec;
    double stdev_ops_sec;
    double cv_ops_sec;
    unsigned long rss_delta_kb;
    unsigned long peak_rss_kb;
    unsigned int resize_count;
    unsigned int max_chain_depth;
    const char *resize_events_timeline;
    unsigned long success_count;
    unsigned long failure_count;
    unsigned long missing_count;
    bool verification_passed;
    bool latency_measured;
    uint64_t latency_p50_ns;
    uint64_t latency_p95_ns;
    uint64_t latency_p99_ns;
    uint64_t latency_max_ns;
} bench_result;

int run_single_thread_scenario(const bench_scenario *scenario, bench_result *result);
void print_single_thread_result_json(FILE *stream, const bench_result *result);

#endif
