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

#define SUBTRACT 20

#define T1_EXEC_TIME (340 - SUBTRACT)
#define T1_WEIGHT 5
#define T1_PERIOD 500

#define T2_EXEC_TIME (150 - SUBTRACT)
#define T2_WEIGHT 6
#define T2_PERIOD T1_PERIOD

// New task 3 parameters
#define T3_EXEC_TIME (200 - SUBTRACT)
#define T3_WEIGHT 3
#define T3_PERIOD T1_PERIOD

#define ITERATIONS 5

int finish_t1[ITERATIONS];
int finish_t2[ITERATIONS];
int finish_t3[ITERATIONS];   // NEW

K_MSGQ_DEFINE(t1_msg, sizeof(int), ITERATIONS, 4);
K_MSGQ_DEFINE(t2_msg, sizeof(int), ITERATIONS, 4);
K_MSGQ_DEFINE(t3_msg, sizeof(int), ITERATIONS, 4);   // NEW

void task(void *arg1, void *arg2, void *arg3);
void task2(void *arg1, void *arg2, void *arg3);
void task3(void *arg1, void *arg2, void *arg3);      // NEW
void master(void *arg1, void *arg2, void *arg3);

// Mission control task - T1
K_THREAD_DEFINE(thread1, 2048,
				task, NULL, NULL, NULL,
				5, 0, 0);

// Navigation task - T2
K_THREAD_DEFINE(thread2, 2048,
				task2, NULL, NULL, NULL,
				5, 0, 0);

// New instrumentation payload task - T3
K_THREAD_DEFINE(thread3, 2048,
                task3, NULL, NULL, NULL,
                5, 0, 0);    // same base priority as others

// Master scheduler
K_THREAD_DEFINE(mastert, 2048, master, NULL, NULL, NULL, 1, 0, 0);


/*************** TASK 1 ***************/
void task(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);

	int dummy;

	for (int i = 0; i < ITERATIONS; i++) {
		k_msgq_get(&t1_msg, &dummy, K_FOREVER);

		k_busy_wait((T1_EXEC_TIME / 2) * 1000);
		printk("Halfway thru task1\n");
		k_busy_wait((T1_EXEC_TIME / 2) * 1000);

		finish_t1[i] = k_cycle_get_32();

		k_thread_absolute_deadline_set(k_current_get(),
			k_current_get()->base.prio_deadline + k_ms_to_cyc_ceil32(T1_PERIOD));
	}
}


/*************** TASK 2 ***************/
void task2(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);

	int dummy;

	for (int i = 0; i < ITERATIONS; i++) {
		k_msgq_get(&t2_msg, &dummy, K_FOREVER);

		k_busy_wait((T2_EXEC_TIME / 2) * 1000);
		printk("Halfway thru task2\n");
		k_busy_wait((T2_EXEC_TIME / 2) * 1000);

		finish_t2[i] = k_cycle_get_32();

		k_thread_absolute_deadline_set(k_current_get(),
			k_current_get()->base.prio_deadline + k_ms_to_cyc_ceil32(T2_PERIOD));
	}
}


/*************** TASK 3  (NEW) ***************/
void task3(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);

	int dummy;

	for (int i = 0; i < ITERATIONS; i++) {
		k_msgq_get(&t3_msg, &dummy, K_FOREVER);

		k_busy_wait((T3_EXEC_TIME / 2) * 1000);
		printk("Halfway thru task3\n");
		k_busy_wait((T3_EXEC_TIME / 2) * 1000);

		finish_t3[i] = k_cycle_get_32();

		k_thread_absolute_deadline_set(k_current_get(),
			k_current_get()->base.prio_deadline + k_ms_to_cyc_ceil32(T3_PERIOD));
	}
}


/**************** MASTER THREAD ****************/
void master(void *arg1, void *arg2, void *arg3)
{
	k_thread_weight_set(thread1, T1_WEIGHT);
	thread1->base.usage.track_usage = true;
	k_thread_exec_time_set(thread1, k_ms_to_cyc_ceil32(T1_EXEC_TIME));

	k_thread_weight_set(thread2, T2_WEIGHT);
	thread2->base.usage.track_usage = true;
	k_thread_exec_time_set(thread2, k_ms_to_cyc_ceil32(T2_EXEC_TIME));

	k_thread_weight_set(thread3, T3_WEIGHT);                 // NEW
	thread3->base.usage.track_usage = true;
	k_thread_exec_time_set(thread3, k_ms_to_cyc_ceil32(T3_EXEC_TIME));

	int dummy = 0;

	int t1_start = k_cycle_get_32();
	printk("Start cycle T1: %d\n", t1_start);
	k_thread_deadline_set(thread1, k_ms_to_cyc_ceil32(T1_PERIOD));
	k_msgq_put(&t1_msg, &dummy, K_NO_WAIT);

	int t2_start = k_cycle_get_32();
	printk("Start cycle T2: %d\n", t2_start);
	k_thread_deadline_set(thread2, k_ms_to_cyc_ceil32(T2_PERIOD));
	k_msgq_put(&t2_msg, &dummy, K_NO_WAIT);

	int t3_start = k_cycle_get_32();                          // NEW
	printk("Start cycle T3: %d\n", t3_start);
	k_thread_deadline_set(thread3, k_ms_to_cyc_ceil32(T3_PERIOD));
	k_msgq_put(&t3_msg, &dummy, K_NO_WAIT);

	for (int i = 1; i < ITERATIONS; i++) {
		k_sleep(K_MSEC(500));
		printk("Sending more msgs\n");
		k_msgq_put(&t1_msg, &dummy, K_NO_WAIT);
		k_msgq_put(&t2_msg, &dummy, K_NO_WAIT);
		k_msgq_put(&t3_msg, &dummy, K_NO_WAIT);
	}

	k_thread_join(thread1, K_TIMEOUT_ABS_SEC(10));
	k_thread_join(thread2, K_TIMEOUT_ABS_SEC(10));
	k_thread_join(thread3, K_TIMEOUT_ABS_SEC(10));            // NEW

	int t1_deadline_misses[ITERATIONS];
	int t2_deadline_misses[ITERATIONS];
	int t3_deadline_misses[ITERATIONS];

	for (int i = 0; i < ITERATIONS; i++) {

		int miss;

		miss = finish_t1[i] - (t1_start + k_ms_to_cyc_ceil32(T1_PERIOD * (i+1)));
		t1_deadline_misses[i] = miss > 0 ? miss : 0;

		miss = finish_t2[i] - (t2_start + k_ms_to_cyc_ceil32(T2_PERIOD * (i+1)));
		t2_deadline_misses[i] = miss > 0 ? miss : 0;

		miss = finish_t3[i] - (t3_start + k_ms_to_cyc_ceil32(T3_PERIOD * (i+1)));
		t3_deadline_misses[i] = miss > 0 ? miss : 0;
	}

	uint64_t tardiness = 0;

	for (int i = 0; i < ITERATIONS; i++) {
		printk("Missed deadline t1: %d  t2: %d  t3: %d\n",
		       t1_deadline_misses[i],
		       t2_deadline_misses[i],
		       t3_deadline_misses[i]);

		tardiness += t1_deadline_misses[i] * T1_WEIGHT;
		tardiness += t2_deadline_misses[i] * T2_WEIGHT;
		tardiness += t3_deadline_misses[i] * T3_WEIGHT;  // NEW
	}

	printk("Utilization: %f\n",
		(double)T1_EXEC_TIME / T1_PERIOD +
		(double)T2_EXEC_TIME / T2_PERIOD +
		(double)T3_EXEC_TIME / T3_PERIOD);

	printk("Tardiness: %lld\n", tardiness);
}

int main(void)
{
	return 0;
}
