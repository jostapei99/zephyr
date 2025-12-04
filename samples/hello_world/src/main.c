/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include "syscalls/kernel.h"
#include "zephyr/kernel/thread.h"
#include "zephyr/sys/printk.h"
#include "zephyr/sys/time_units.h"
#include <stdbool.h>
#include <stdio.h>

#define T1_EXEC_TIME 340
#define T1_WEIGHT 5
#define T1_PERIOD 500

#define T2_EXEC_TIME 150
#define T2_WEIGHT 5
#define T2_PERIOD T1_PERIOD

#define ITERATIONS 5

int finish_t1[ITERATIONS];
int finish_t2[ITERATIONS];

K_MSGQ_DEFINE(t1_msg, sizeof(int), ITERATIONS, 4);
K_MSGQ_DEFINE(t2_msg, sizeof(int), ITERATIONS, 4);

void task(void *arg1, void *arg2, void *arg3);
void task2(void *arg1, void *arg2, void *arg3);
void master(void *arg1, void *arg2, void *arg3);

// Mission control task - highest priority
K_THREAD_DEFINE(thread1, 2048,
				task, NULL, NULL, NULL,
				5, 0, 0);

// Navigation task - high priority
K_THREAD_DEFINE(thread2, 2048,
				task2, NULL, NULL, NULL,
				5, 0, 0);

K_THREAD_DEFINE(mastert, 2048, master, NULL, NULL, NULL, 4, 0, 0);


void task(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

	int dummy;

	for (int i = 0; i < ITERATIONS; i++) {
		k_msgq_get(&t1_msg, &dummy, K_FOREVER);
		k_busy_wait((T1_EXEC_TIME / 2) * 1000);
		printk("Halfway thru task1\n");
		k_busy_wait((T1_EXEC_TIME / 2) * 1000);
		finish_t1[i] = k_cycle_get_32();
		k_thread_absolute_deadline_set(k_current_get(), k_current_get()->base.prio_deadline + k_ms_to_cyc_ceil32(T1_PERIOD)); 
	}
}

void task2(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

	int dummy;

	for (int i = 0; i < ITERATIONS; i++) {
		k_msgq_get(&t2_msg, &dummy, K_FOREVER);

		k_busy_wait((T2_EXEC_TIME / 2) * 1000);
		printk("Halfway thru task2\n");
		k_busy_wait((T2_EXEC_TIME / 2) * 1000);
		finish_t2[i] = k_cycle_get_32();
		k_thread_absolute_deadline_set(k_current_get(), k_current_get()->base.prio_deadline + k_ms_to_cyc_ceil32(T2_PERIOD)); 
	}
}

void master(void *arg1, void *arg2, void *arg3)
{
	k_thread_weight_set(thread1, T1_WEIGHT); // Weight has to be non-zero int (maybe 1 to 10?)
	thread1->base.usage.track_usage = true;
	k_thread_exec_time_set(thread1, k_ms_to_cyc_ceil32(T1_EXEC_TIME));

	k_thread_weight_set(thread2,T2_WEIGHT);
	thread2->base.usage.track_usage = true;
	k_thread_exec_time_set(thread2, k_ms_to_cyc_ceil32(T2_EXEC_TIME));

	

	int dummy = 0;

	int t1_start = k_cycle_get_32();
	printk("Start cycle for t1: %d\n", t1_start);
	k_thread_deadline_set(thread1, k_ms_to_cyc_ceil32(T1_PERIOD));
	k_msgq_put(&t1_msg, &dummy, K_NO_WAIT);

	int t2_start = k_cycle_get_32();
	printk("Start cycle for t2: %d\n", t2_start);
	k_thread_deadline_set(thread2, k_ms_to_cyc_ceil32(T2_PERIOD));
	k_msgq_put(&t2_msg, &dummy, K_NO_WAIT);
	// k_sleep(K_FOREVER);
	for (int i = 1; i < ITERATIONS; i++) {
		k_sleep(K_MSEC(500));
		printk("Sending more msgs\n");
		k_msgq_put(&t1_msg, &dummy, K_NO_WAIT);
		k_msgq_put(&t2_msg, &dummy, K_NO_WAIT);
		
	}

	k_thread_join(thread1, K_TIMEOUT_ABS_SEC(10));
	k_thread_join(thread2, K_TIMEOUT_ABS_SEC(10));

	int t1_deadlines_misses[ITERATIONS];
	int t2_deadline_misses[ITERATIONS];

	for (int i = 0; i < ITERATIONS; i++) {
		int miss = finish_t1[i] - (t1_start + k_ms_to_cyc_ceil32(T1_PERIOD * (i+1)));
		t1_deadlines_misses[i] = miss > 0 ? miss : 0;
		miss = finish_t2[i] - (t2_start + k_ms_to_cyc_ceil32(T2_PERIOD * (i+1)));
		t2_deadline_misses[i] = miss > 0 ? miss : 0;
	}

	uint64_t tardiness = 0;

	// Tardiness is sum of (time that deadline was missed * weight) for each thread
	for (int i = 0; i < ITERATIONS; i++) {
		// printk("Finish time t1: %d t2: %d\n",finish_t1[i],finish_t2[i]);
		tardiness += t1_deadlines_misses[i] * T1_WEIGHT;
		tardiness += t2_deadline_misses[i] * T2_WEIGHT;
		printk("Missed deadline t1: %d t2: %d\n",t1_deadlines_misses[i],t2_deadline_misses[i]);
	}

	// Calculate total utilization, > 1 means overload
	printk("Utilization: %f\n", (double)T1_EXEC_TIME / T1_PERIOD + (double)T2_EXEC_TIME / T2_PERIOD);
	printk("Tardiness: %lld\n", tardiness);
}

int main(void)
{
	return 0;
}
