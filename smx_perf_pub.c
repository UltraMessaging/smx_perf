/* smx_perf_pub.c - measure one-way latency under load (publisher). */
/*
  Copyright (c) 2021 Informatica Corporation
  Permission is granted to licensees to use or alter this software for any
  purpose, including commercial applications, according to the terms laid
  out in the Software License Agreement.

  This source code example is provided by Informatica for educational
  and evaluation purposes only.

  THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES 
  EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF 
  NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR 
  PURPOSE.  INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE 
  UNINTERRUPTED OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES,
  BE LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR 
  INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE 
  TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF 
  THE LIKELIHOOD OF SUCH DAMAGES.
*/

/* This is needed for affinity setting. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "lbm/lbm.h"
#include "smx_perf.h"


/* Command-line options and their defaults */
static int o_affinity_cpu = 1;
static char *o_config = NULL;
static int o_flags = 0;
static int o_jitter_loops = 0;
static int o_linger_ms = 100;
static int o_msg_len = 25;
static int o_num_msgs = 10000000;
static int o_rate = 1000000;
static char *o_topic = NULL;  /* Default set in "get_my_opts()". */
static int o_warmup_loops = 10000;


char usage_str[] = "Usage: smx_perf_pub [-h] [-a affinity_cpu] [-c config] [-f flags] [-j jitter_loops] [-l linger_ms] [-m msg_len] [-n num_msgs] [-r rate] [-t topic] [-w warmup_loops]";

void usage(char *msg) {
  if (msg) fprintf(stderr, "%s\n", msg);
  fprintf(stderr, "%s\n", usage_str);
  exit(1);
}

void help() {
  fprintf(stderr, "%s\n", usage_str);
  fprintf(stderr, "where:\n"
      "  -h : print help\n"
      "  -a affinity_cpu : bitmap for CPU affinity for send thread [%d]\n"
      "  -c config : configuration file; can be repeated [%s]\n"
      "  -f flags : bitmap [0x%x]: 0x01: FLAGS_TIMESTAMP (to measure latency)\n"
      "                           0x02: FLAGS_NON_BLOCKING\n"
      "                           0x04: FLAGS_GENERIC_SRC\n"
      "  -j jitter_loops : jitter measurement loops [%d]\n"
      "  -l linger_ms : linger time before source delete [%d]\n"
      "  -m msg_len : message length [%d]\n"
      "  -n num_msgs : number of messages to send [%d]\n"
      "  -r rate : messages per second to send [%d]\n"
      "  -t topic : topic string [\"%s\"]\n"
      "  -w warmup_loops : messages to send before measurement loop [%d]\n"
      , o_affinity_cpu, (o_config == NULL) ? "" : o_config, o_flags
      , o_jitter_loops, o_linger_ms, o_msg_len, o_num_msgs, o_rate
      , o_topic, o_warmup_loops
  );
  exit(0);
}


void get_my_opts(int argc, char **argv)
{
  int opt;  /* Loop variable for getopt(). */
  int e;  /* Error indicator. */

  o_topic = strdup("smx_perf");  /* Set default. */

  while ((opt = getopt(argc, argv, "ha:c:f:j:l:m:n:r:t:w:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'a': SAFE_ATOI(optarg, o_affinity_cpu); break;
      /* Allow -c to be repeated, loading each config file in succession. */
      case 'c': if (o_config != NULL) { free(o_config); }
                o_config = strdup(optarg);
                E(lbm_config(o_config));
                break;
      case 'f': SAFE_ATOI(optarg, o_flags); break;
      case 'j': SAFE_ATOI(optarg, o_jitter_loops); break;
      case 'l': SAFE_ATOI(optarg, o_linger_ms); break;
      case 'm': SAFE_ATOI(optarg, o_msg_len); break;
      case 'n': SAFE_ATOI(optarg, o_num_msgs); break;
      case 'r': SAFE_ATOI(optarg, o_rate); break;
      case 't': free(o_topic); o_topic = strdup(optarg); break;
      case 'w': SAFE_ATOI(optarg, o_warmup_loops); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  if (optind != argc) { usage("Extra parameter(s)"); }
}  /* get_my_opts */


/* Measure the minimum and maximum duration of a timestamp. */
void jitter_loop()
{
  uint64_t ts_min_ns = 999999999;
  uint64_t ts_max_ns = 0;
  int i;

  for (i = 0; i < o_jitter_loops; i++) {
    struct timespec ts1;
    struct timespec ts2;
    uint64_t ts_this_ns;

    /* Two timestamps in a row measures the duration of the timestamp. */
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    DIFF_TS(ts_this_ns, ts2, ts1);
    /* Track maximum and minimum (average not needed). */
    if (ts_this_ns < ts_min_ns) ts_min_ns = ts_this_ns;
    if (ts_this_ns > ts_max_ns) ts_max_ns = ts_this_ns;
  }  /* for i */

  printf("ts_min_ns=%"PRIu64", ts_max_ns=%"PRIu64", \n",
      ts_min_ns, ts_max_ns);
}  /* jitter_loop */


int send_loop(lbm_src_t *src, int num_sends, uint64_t sends_per_sec, int flags)
{
  perf_msg_t *perf_msg;  /* SMX guarantees alignment to 64-bit. */
  struct timespec cur_ts;
  struct timespec start_ts;
  uint64_t num_sent;
  int i, e, lbm_send_flags;

  lbm_send_flags = 0;
  if ((flags & FLAGS_NON_BLOCKING) == FLAGS_NON_BLOCKING) {
    lbm_send_flags = LBM_SRC_NONBLOCK;
  }
  if ((flags & FLAGS_GENERIC_SRC) == 0) {  /* If using SMX src API */
    /* Get buffer from shared memory to construct first message. */
    E(lbm_src_buff_acquire(src, (void **)&perf_msg, o_msg_len,
        lbm_send_flags));
  }
  else {  /* Generic src API */
    perf_msg = (perf_msg_t *)malloc(o_msg_len);
  }

  /* Send messages evenly-spaced using busy looping. Based on algorithm:
   * http://www.geeky-boy.com/catchup/html/ */
  clock_gettime(CLOCK_MONOTONIC, &start_ts);
  cur_ts = start_ts;
  num_sent = 0;
  do {  /* while num_sent < num_sends */
    uint64_t ns_so_far;
    DIFF_TS(ns_so_far, cur_ts, start_ts);
    /* The +1 is because we want to send, then pause. */
    uint64_t should_have_sent = (ns_so_far * sends_per_sec)/1000000000 + 1;
    if (should_have_sent > num_sends) {
      should_have_sent = num_sends;
    }

    /* If we are behind where we should be, get caught up. */
    while (num_sent < should_have_sent) {
      /* Construct message in shared memory buffer. */
      perf_msg->flags = flags;
      if ((flags & FLAGS_TIMESTAMP) == FLAGS_TIMESTAMP) {
        clock_gettime(CLOCK_MONOTONIC, &perf_msg->send_ts);
      }
      perf_msg->msg_num = num_sent;

      if ((flags & FLAGS_GENERIC_SRC) == 0) {  /* If using SMX src API */
        /* Send message and get next buffer from shared memory. */
        E(lbm_src_buffs_complete_and_acquire(src, (void **)&perf_msg,
            o_msg_len, lbm_send_flags));
      }
      else {  /* Generic src API */
        E(lbm_src_send(src, (void *)perf_msg, o_msg_len, lbm_send_flags));
      }

      num_sent++;
    }  /* while num_sent < should_have_sent */
    clock_gettime(CLOCK_MONOTONIC, &cur_ts);
  } while (num_sent < num_sends);

  if ((flags & FLAGS_GENERIC_SRC) == 0) {  /* If using SMX src API */
    /* Release the last acauired buffer from shared memory. */
    E(lbm_src_buffs_cancel(src));
  }
  else {  /* Generic src API */
    free(perf_msg);
  }

  return num_sent;
}  /* send_loop */


int main(int argc, char **argv)
{
  cpu_set_t cpuset;
  lbm_context_t *ctx;
  lbm_topic_t *topic_obj;
  lbm_src_t *src;
  struct timespec start_ts;  /* struct timespec is used by clock_gettime(). */
  struct timespec end_ts;
  uint64_t duration_ns;
  int actual_sends;
  double result_rate;

  get_my_opts(argc, argv);

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("o_affinity_cpu=%d, o_config=%s, o_flags=0x%02x, o_jitter_loops=%d, o_linger_ms=%d, o_msg_len=%d, o_num_msgs=%d, o_rate=%d, o_topic='%s', o_warmup_loops=%d, \n",
      o_affinity_cpu, o_config, o_flags, o_jitter_loops, o_linger_ms, o_msg_len, o_num_msgs, o_rate, o_topic, o_warmup_loops);

  if (o_jitter_loops > 0) {
    CPU_ZERO(&cpuset);
    CPU_SET(o_affinity_cpu, &cpuset);
    errno = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (errno != 0) { PERRNO("pthread_setaffinity_np"); }

    jitter_loop();

    exit(0);
  }

  /* Publisher's UM objects. */
  E(lbm_context_create(&ctx, NULL, NULL, NULL));
  E(lbm_src_topic_alloc(&topic_obj, ctx, o_topic, NULL));
  E(lbm_src_create(&src, ctx, topic_obj, NULL, NULL, NULL));

  usleep(500*1000);  /* Let topic resolution happen. */

  /* Pin time-critical thread to requested CPU core. */
  CPU_ZERO(&cpuset);
  CPU_SET(o_affinity_cpu, &cpuset);
  errno = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  if (errno != 0) { PERRNO("pthread_setaffinity_np"); }

  if (o_warmup_loops > 0) {
    /* Warmup loops to get CPU caches loaded
     * (Ignore timestamp and non-block flags for warmup). */
    send_loop(src, o_warmup_loops, 10000, o_flags & FLAGS_GENERIC_SRC);
  }

  /* Measure overall send rate by timing the main send loop. */
  clock_gettime(CLOCK_MONOTONIC, &start_ts);
  actual_sends = send_loop(src, o_num_msgs, o_rate, o_flags);
  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  DIFF_TS(duration_ns, end_ts, start_ts);

  result_rate = (double)(duration_ns);
  result_rate /= (double)1000000000;
  result_rate = (double)actual_sends / result_rate;

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("actual_sends=%d, duration_ns=%"PRIu64", result_rate=%f, \n",
      actual_sends, duration_ns, result_rate);

  /* Just in case the receiver is running behind, delay deleting the source
   * so that the receiver can get caught up. */
  if (o_linger_ms > 0) {
    usleep(o_linger_ms*1000);
  }

  E(lbm_src_delete(src));
  E(lbm_context_delete(ctx));
}  /* main */
