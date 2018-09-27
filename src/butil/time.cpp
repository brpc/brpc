// Copyright (c) 2010 Baidu, Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Ge,Jun (gejun@baidu.com)
// Date: Fri Aug 29 15:01:15 CST 2014

#include <unistd.h>                          // close
#include <sys/types.h>                       // open
#include <sys/stat.h>                        // ^
#include <fcntl.h>                           // ^

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <string.h>                          // memmem
#undef _GNU_SOURCE

#include "butil/time.h"

#if defined(NO_CLOCK_GETTIME_IN_MAC)
#include <mach/clock.h>                      // mach_absolute_time
#include <mach/mach_time.h>                  // mach_timebase_info
#include <pthread.h>                         // pthread_once
#include <stdlib.h>                          // exit

static mach_timebase_info_data_t s_timebase;
static timespec s_init_time;
static uint64_t s_init_ticks;
static pthread_once_t s_init_clock_once = PTHREAD_ONCE_INIT;

static void InitClock() {
    if (mach_timebase_info(&s_timebase) != 0) {
        exit(1);
    }
    timeval now;
    if (gettimeofday(&now, NULL) != 0) {
        exit(1);
    }
    s_init_time.tv_sec = now.tv_sec;
    s_init_time.tv_nsec = now.tv_usec * 1000L;
    s_init_ticks = mach_absolute_time();
}

int clock_gettime(clockid_t id, timespec* time) {
    if (pthread_once(&s_init_clock_once, InitClock) != 0) {
        exit(1);
    }
    uint64_t clock = mach_absolute_time() - s_init_ticks;
    uint64_t elapsed = clock * (uint64_t)s_timebase.numer / (uint64_t)s_timebase.denom;
    *time = s_init_time;
    time->tv_sec += elapsed / 1000000000L;
    time->tv_nsec += elapsed % 1000000000L;
    time->tv_sec += time->tv_nsec / 1000000000L;
    time->tv_nsec = time->tv_nsec % 1000000000L;
    return 0;
}

#endif

namespace butil {

int64_t monotonic_time_ns() {
    // MONOTONIC_RAW is slower than MONOTONIC in linux 2.6.32, trying to
    // use the RAW version does not make sense anymore.
    // NOTE: Not inline to keep ABI-compatible with previous versions.
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000000000L + now.tv_nsec;
}

namespace detail {

static double read_info_from_buf(char* buf, int n, const char* param) {
    char *pos = static_cast<char*>(memmem(buf, n, param, strlen(param)));
    double result = 0;
    if (pos != NULL) {
        char *endp = buf + n;
        int seen_decpoint = 0;
        int ndigits = 0;

        /* Search for the beginning of the string.  */
        while (pos < endp && (*pos < '0' || *pos > '9') && *pos != '\n') {
            ++pos;
        }
        while (pos < endp && *pos != '\n') {
            if (*pos >= '0' && *pos <= '9') {
                result *= 10;
                result += *pos - '0';
                if (seen_decpoint)
                    ++ndigits;
            } else if (*pos == '.') {
                seen_decpoint = 1;
            }
            ++pos;
        }
        while (ndigits --) {
            result /= 10;
        }
    }
    return result;
}

// read_cpu_frequency() is modified from source code of glibc.
int64_t read_cpu_frequency(bool* invariant_tsc) {
    /*
     * We use this formula to get the "constant" frequency of cpu:
     * freq_Mhz = bogomips / hyper-threading_num
    */

    const int fd = open("/proc/cpuinfo", O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    int64_t result = 0;
    char buf[4096];  // should be enough
    const ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
        double bogomips = read_info_from_buf(buf, n, "bogomips"); 
        double core_num = read_info_from_buf(buf, n, "cpu cores");
        double siblings = read_info_from_buf(buf, n, "siblings");
        if (siblings > 0) {
            result = static_cast<int64_t>(1000000 * bogomips * core_num / siblings);
        }

        if (invariant_tsc) {
            char* flags_pos = static_cast<char*>(memmem(buf, n, "flags", 5));
            *invariant_tsc = 
                (flags_pos &&
                 memmem(flags_pos, buf + n - flags_pos, "constant_tsc", 12) &&
                 memmem(flags_pos, buf + n - flags_pos, "nonstop_tsc", 11));
        }
    }
    close (fd);
    return result;
}

// Return value must be >= 0
int64_t read_invariant_cpu_frequency() {
    bool invariant_tsc = false;
    const int64_t freq = read_cpu_frequency(&invariant_tsc);
    if (!invariant_tsc || freq < 0) {
        return 0;
    }
    return freq;
}

int64_t invariant_cpu_freq = -1;
}  // namespace detail

}  // namespace butil
