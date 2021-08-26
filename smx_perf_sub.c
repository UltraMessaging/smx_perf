/* smx_perf_sub.c - measure one-way latency under load (subscriber). */
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

/* Options and their defaults */
static int o_affinity_cpu = 0;
static char *o_config = NULL;
static int o_fast = 0;
static int o_spin_cnt = 0;
static char *o_topic = NULL;  /* Default set in "get_my_opts()". */


char usage_str[] = "Usage: smx_perf_sub [-h] [-a affinity_cpu] [-c config] [-f] [-s spin_cnt] [-t topic] [-v]]";

void usage(char *msg) {
  if (msg) fprintf(stderr, "%s\n", msg);
  fprintf(stderr, "%s\n", usage_str);
  exit(1);
}

void help() {
  fprintf(stderr, "%s\n", usage_str);
  fprintf(stderr, "where:\n"
      "  -h : print help\n"
      "  -a affinity_cpu : CPU number (0..N-1) for SMX receive thread [%d]\n"
      "  -c config : configuration file; can be repeated [%s]\n"
      "  -f : fast - minimal processing in message receive (no latency calcs)\n"
      "  -s spin_cnt : empty loop inside receiver callback [%d]\n"
      "  -t topic : topic string [%s]\n"
      , o_affinity_cpu, (o_config == NULL) ? "" : o_config, o_spin_cnt, o_topic
  );
  exit(0);
}


void get_my_opts(int argc, char **argv)
{
  int opt;  /* Loop variable for getopt(). */
  int e;  /* Error indicator. */

  o_topic = strdup("smx_perf");  /* Set default. */

  while ((opt = getopt(argc, argv, "ha:c:fs:t:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'a': SAFE_ATOI(optarg, o_affinity_cpu); break;
      /* Allow -c to be repeated, loading each config file in succession. */
      case 'c': if (o_config != NULL) { free(o_config); }
                o_config = strdup(optarg);
                E(lbm_config(o_config));
                break;
      case 'f': o_fast = 1; break;
      case 's': SAFE_ATOI(optarg, o_spin_cnt); break;
      case 't': free(o_topic); o_topic = strdup(optarg); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  if (optind != argc) { usage("Extra parameter(s)"); }
}  /* get_my_opts */


/* This is global because both "rcv_callback()" and "fast_rcv_callback()" need to
 * access it. */
static uint64_t num_rcv_msgs;
/* This "spinner" variable is made global to force the optimizer to update it. */
int global_spinner;
int rcv_callback(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
  static uint64_t min_latency;
  static uint64_t max_latency;
  static uint64_t sum_latencies;  /* For calculating average latencies. */
  static uint64_t num_timestamps; /* For calculating average latencies. */
  cpu_set_t cpuset;

  switch (msg->type) {
  case LBM_MSG_BOS:
    /* Assume SMX receive thread is calling this; pin the time-critical thread
     * to the requested CPU. */
    CPU_ZERO(&cpuset);
    CPU_SET(o_affinity_cpu, &cpuset);
    errno = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (errno != 0) { PERRNO("pthread_setaffinity_np"); }

    num_rcv_msgs = 0;
    min_latency = (uint64_t)-1;  /* max int */
    max_latency = 0;
    sum_latencies = 0;
    num_timestamps = 0;
    printf("rcv event BOS, topic_name='%s', source=%s, \n",
      msg->topic_name, msg->source);
    fflush(stdout);
    break;

  case LBM_MSG_EOS:
    if (num_timestamps > 0) {
      printf("rcv event EOS, '%s', %s, num_rcv_msgs=%"PRIu64", min_latency=%"PRIu64", max_latency=%"PRIu64", average latency=%"PRIu64", \n",
          msg->topic_name, msg->source, num_rcv_msgs,
          min_latency, max_latency, sum_latencies / num_timestamps);
    } else {
      printf("rcv event EOS, '%s', %s, num_rcv_msgs=%"PRIu64", \n",
          msg->topic_name, msg->source, num_rcv_msgs);
    }
    fflush(stdout);
    break;

  case LBM_MSG_DATA:
  {
    perf_msg_t *perf_msg = (perf_msg_t *)msg->data;
 
    if ((perf_msg->flags & FLAGS_TIMESTAMP) == FLAGS_TIMESTAMP) {
      struct timespec cur_ts;
      uint64_t diff_ns;

      /* Calculate one-way latency for this message. */
      clock_gettime(CLOCK_MONOTONIC, &cur_ts);
      DIFF_TS(diff_ns, cur_ts, perf_msg->send_ts);

      if (diff_ns < min_latency) min_latency = diff_ns;
      if (diff_ns > max_latency) max_latency = diff_ns;
      sum_latencies += diff_ns;
      num_timestamps++;
    }
    num_rcv_msgs++;
    /* This "spinner" loop is to introduce short delays into the receiver. */
    for (global_spinner = 0; global_spinner < o_spin_cnt; global_spinner++) {
    }
    break;
  }

  default:
    printf("rcv event %d, topic_name='%s', source=%s, \n", msg->type, msg->topic_name, msg->source);
  }  /* switch msg->type */

  return 0;
}  /* rcv_callback */


/* Set this as the receiver callback when you want absolute minimal work done
 * for received messages. */
int fast_rcv_callback(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
  if (msg->type == LBM_MSG_DATA) {
    num_rcv_msgs++;
  }
  else {
    /* Process BOS, EOS, etc. */
    rcv_callback(rcv, msg, clientd);
  }

  return 0;
}  /* fast_rcv_callback */


int main(int argc, char **argv)
{
  lbm_context_t *ctx;
  lbm_topic_t *topic_obj;
  lbm_rcv_t *rcv;

  get_my_opts(argc, argv);

  printf("o_affinity_cpu=%d, o_config=%s, o_fast=%d, o_spin_cnt=%d, o_topic='%s', \n",
      o_affinity_cpu, o_config, o_fast, o_spin_cnt, o_topic);

  /* Create UM context. */
  E(lbm_context_create(&ctx, NULL, NULL, NULL));

  /* Rest of publisher's UM objects. */
  E(lbm_rcv_topic_lookup(&topic_obj, ctx, o_topic, NULL));
  if (o_fast) {
    E(lbm_rcv_create(&rcv, ctx, topic_obj, fast_rcv_callback, NULL, NULL));
  } else {
    E(lbm_rcv_create(&rcv, ctx, topic_obj, rcv_callback, NULL, NULL));
  }

  /* The subscriber must be "kill"ed externally. */
  sleep(2000000000);  /* 23+ centries. */

  E(lbm_rcv_delete(rcv));
  E(lbm_context_delete(ctx));
}  /* main */
