#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "benchmark_common.h"
#include "single_thread_models.h"
#include "single_thread_runner.h"
#include "type_definitions/sucess_code_definitions.h"

static int
compare_double(const void *left_ptr, const void *right_ptr)
{
    double left = *(const double *)left_ptr;
    double right = *(const double *)right_ptr;
    if (left < right)
    {
        return -1;
    }
    if (left > right)
    {
        return 1;
    }
    return 0;
}

static bool
scenario_matches(const bench_scenario *scenario, const char *test_id, const char *prefix)
{
    if (strcmp(test_id, "all") != 0 && strcmp(test_id, scenario->test_id) != 0)
    {
        return false;
    }

    if (prefix != NULL)
    {
        size_t prefix_len = strlen(prefix);
        if (prefix_len > 0 && strncmp(scenario->test_id, prefix, prefix_len) != 0)
        {
            return false;
        }
    }

    return true;
}

int
main(int argc, char **argv)
{
    const char *requested_scenario = "all";
    const char *requested_prefix = NULL;
    const char *results_file_path = NULL;

    for (int index = 1; index < argc; ++index)
    {
        if (strcmp(argv[index], "--scenario") == 0 && index + 1 < argc)
        {
            requested_scenario = argv[++index];
        }
        else if (strcmp(argv[index], "--scenario-prefix") == 0 && index + 1 < argc)
        {
            requested_prefix = argv[++index];
        }
        else if (strcmp(argv[index], "--results-file") == 0 && index + 1 < argc)
        {
            results_file_path = argv[++index];
        }
        else if (strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0)
        {
            print_benchmark_usage(argv[0]);
            return 0;
        }
        else
        {
            print_benchmark_usage(argv[0]);
            return 1;
        }
    }

    FILE *results_file = stdout;
    if (results_file_path != NULL)
    {
        results_file = fopen(results_file_path, "w");
        if (results_file == NULL)
        {
            fprintf(stderr, "Failed to open results file: %s\n", results_file_path);
            return 1;
        }
    }

    size_t scenario_count = 0;
    const bench_scenario *scenarios = single_thread_scenarios(&scenario_count);
    unsigned long executed = 0;
    unsigned long failed = 0;

    for (size_t index = 0; index < scenario_count; ++index)
    {
        const bench_scenario *scenario = &scenarios[index];
        if (!scenario_matches(scenario, requested_scenario, requested_prefix))
        {
            continue;
        }

        unsigned int repetitions = scenario->repetitions == 0 ? 1 : scenario->repetitions;
        bench_result *run_results = (bench_result *)calloc(repetitions, sizeof(bench_result));
        double *throughputs = (double *)calloc(repetitions, sizeof(double));

        if (run_results == NULL || throughputs == NULL)
        {
            free(run_results);
            free(throughputs);
            ++failed;
            continue;
        }

        unsigned int completed = 0;
        for (unsigned int run_index = 0; run_index < repetitions; ++run_index)
        {
            bench_result *run_result = &run_results[run_index];
            if (run_single_thread_scenario(scenario, run_result) != SUCCESS)
            {
                ++failed;
                break;
            }

            throughputs[run_index] = run_result->throughput_ops_sec;
            ++completed;

            if (!run_result->verification_passed || run_result->failure_count > 0 || run_result->missing_count > 0)
            {
                ++failed;
            }

            if (scenario->cooldown_ms > 0U)
            {
                struct timespec delay;
                delay.tv_sec = (time_t)(scenario->cooldown_ms / 1000U);
                delay.tv_nsec = (long)((scenario->cooldown_ms % 1000U) * 1000000UL);
                nanosleep(&delay, NULL);
            }
        }

        if (completed == repetitions)
        {
            double sum = 0.0;
            for (unsigned int run_index = 0; run_index < repetitions; ++run_index)
            {
                sum += throughputs[run_index];
            }

            double mean = sum / (double)repetitions;
            double variance = 0.0;
            for (unsigned int run_index = 0; run_index < repetitions; ++run_index)
            {
                double delta = throughputs[run_index] - mean;
                variance += delta * delta;
            }
            double stdev = sqrt(variance / (double)repetitions);
            double cv = (mean > 0.0) ? (stdev / mean) : 0.0;

            double *sorted = (double *)calloc(repetitions, sizeof(double));
            if (sorted != NULL)
            {
                memcpy(sorted, throughputs, repetitions * sizeof(double));
                qsort(sorted, repetitions, sizeof(double), compare_double);
            }
            double median = (sorted == NULL)
                                ? mean
                                : ((repetitions % 2U) == 0U ? (sorted[(repetitions / 2U) - 1U] + sorted[repetitions / 2U]) * 0.5 : sorted[repetitions / 2U]);

            for (unsigned int run_index = 0; run_index < repetitions; ++run_index)
            {
                char run_id[128];
                snprintf(run_id, sizeof(run_id), "%s-run-%u", scenario->test_id, run_index + 1U);

                run_results[run_index].run_id = run_id;
                run_results[run_index].run_index = run_index + 1U;
                run_results[run_index].mean_ops_sec = mean;
                run_results[run_index].median_ops_sec = median;
                run_results[run_index].stdev_ops_sec = stdev;
                run_results[run_index].cv_ops_sec = cv;

                print_single_thread_result_json(results_file, &run_results[run_index]);
            }

            free(sorted);
            ++executed;
        }

        for (unsigned int run_index = 0; run_index < repetitions; ++run_index)
        {
            free((void *)run_results[run_index].resize_events_timeline);
        }

        free(throughputs);
        free(run_results);
    }

    if (results_file_path != NULL)
    {
        fclose(results_file);
    }

    fprintf(stderr, "Executed %lu scenario(s), %lu scenario(s) reported issues.\n", executed, failed);
    return (failed == 0) ? 0 : 2;
}
