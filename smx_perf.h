/* smx_perf.h */
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

#ifndef SMX_PERF_H
#define SMX_PERF_H

#ifdef __cplusplus
extern "C" {
#endif

/* This is needed for affinity setting. */
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>

/* Simple error handling macros:
 * E() - for UM API calls. Checks for error (return != 0). If error, prints
 *     file:line and error message, exits with bad status (1).
 * PERRNO() - prints file:line and error message associated with "errno",
 *     exits with bad status (1).
 */

#define E(lbm_funct_call_) do { \
  int e_ = (lbm_funct_call_); \
  if (e_ != LBM_OK) { \
    fprintf(stderr, "ERROR (%s:%d): %s failed: '%s'\n", \
       __FILE__, __LINE__, #lbm_funct_call_, lbm_errmsg()); \
    fflush(stderr); \
    exit(1); \
  } \
} while (0)

/* Macro to print errno in human-readable form and exit(1). */
#define PERRNO(perrno_in_str_) do { \
  char perrno_errno_ = errno; \
  char perrno_errstr_[2048]; \
  snprintf(perrno_errstr_, sizeof(perrno_errstr_), "ERROR (%s:%d): %s: errno=%u", \
      __FILE__, __LINE__, perrno_in_str_, perrno_errno_); \
  errno = perrno_errno_; \
  perror(perrno_errstr_); \
  fflush(stderr); \
  exit(1); \
} while (0)

/* See https://github.com/fordsfords/safe_atoi */
#define SAFE_ATOI(a_,r_) do { \
  char *in_a_ = (a_); \
  int new_errno_; \
  unsigned long long fs_[9] = {  /* All '1's by variable size. */ \
    0, 0xff, 0xffff, 0, 0xffffffff, 0, 0, 0, 0xffffffffffffffff }; \
  (r_) = fs_[sizeof(r_)]; \
  if ((r_) < 0) { /* Is result a signed value? */ \
    char *temp_ = NULL;  long long llresult_; \
    if (strlen(in_a_) > 2 && in_a_[0] == '0' && (in_a_[1] == 'x' || in_a_[1] == 'X')) { \
      in_a_ += 2;  /* Skip past '0x'. */ \
      errno = 0; \
      llresult_ = strtoll(in_a_, &temp_, 16); \
      new_errno_ = errno; \
    } else { \
      errno = 0; \
      llresult_ = strtoll(in_a_, &temp_, 10); \
      new_errno_ = errno; \
    } \
    if (new_errno_ != 0 || temp_ == in_a_ || temp_ == NULL || *temp_ != '\0') { \
      if (new_errno_ == 0) { \
        new_errno_ = EINVAL; \
      } \
      fprintf(stderr, "%s:%d, Error, invalid number for %s: '%s'\n", \
         __FILE__, __LINE__, #r_, in_a_); \
    } else { /* strtol thinks success; check for overflow. */ \
      (r_) = llresult_; /* "return" value of macro */ \
      if ((r_) != llresult_) { \
        fprintf(stderr, "%s:%d, %s over/under flow: '%s'\n", \
           __FILE__, __LINE__, #r_, in_a_); \
        new_errno_ = ERANGE; \
      } \
    } \
  } else { \
    char *temp_ = NULL;  unsigned long long llresult_; \
    if (strlen(in_a_) > 2 && in_a_[0] == '0' && (in_a_[1] == 'x' || in_a_[1] == 'X')) { \
      in_a_ += 2;  /* Skip past '0x'. */ \
      errno = 0; \
      llresult_ = strtoull(in_a_, &temp_, 16); \
      new_errno_ = errno; \
    } else { \
      errno = 0; \
      llresult_ = strtoull(in_a_, &temp_, 10); \
      new_errno_ = errno; \
    } \
    if (new_errno_ != 0 || temp_ == in_a_ || temp_ == NULL || *temp_ != '\0') { \
      if (new_errno_ == 0) { \
        new_errno_ = EINVAL; \
      } \
      fprintf(stderr, "%s:%d, Error, invalid number for %s: '%s'\n", \
         __FILE__, __LINE__, #r_, in_a_); \
    } else { /* strtol thinks success; check for overflow. */ \
      (r_) = llresult_; /* "return" value of macro */ \
      if ((r_) != llresult_) { \
        fprintf(stderr, "%s:%d, %s over/under flow: '%s'\n", \
           __FILE__, __LINE__, #r_, in_a_); \
        new_errno_ = ERANGE; \
      } \
    } \
  } \
  errno = new_errno_; \
  if (errno != 0) { PERRNO("SAFE_ATOI"); }; \
} while (0)

/* Compute number of nanoseconds between two "struct timespec" values. */
#define DIFF_TS(diff_ts_result_ns_, diff_ts_end_ts_, diff_ts_start_ts_) do { \
  (diff_ts_result_ns_) = (((uint64_t)diff_ts_end_ts_.tv_sec \
                           - (uint64_t)diff_ts_start_ts_.tv_sec) * 1000000000 \
                          + (uint64_t)diff_ts_end_ts_.tv_nsec) \
                         - (uint64_t)diff_ts_start_ts_.tv_nsec; \
} while (0)


#define FLAGS_TIMESTAMP    0x01
#define FLAGS_NON_BLOCKING 0x02
#define FLAGS_GENERIC_SRC  0x04

struct perf_msg_s {
  uint64_t flags;
  uint64_t msg_num;
  struct timespec send_ts;
};
typedef struct perf_msg_s perf_msg_t;

#if defined(__cplusplus)
}
#endif

#endif  /* SMX_PERF_H */
