# smx_perf - test programs to measure the performance of Ultra Messaging's SMX transport.

## COPYRIGHT AND LICENSE

All of the documentation and software included in this and any
other Informatica Ultra Messaging GitHub repository
Copyright (C) Informatica. All rights reserved.

Permission is granted to licensees to use
or alter this software for any purpose, including commercial applications,
according to the terms laid out in the Software License Agreement.

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

## REPOSITORY

See https://github.com/UltraMessaging/smx_perf for code and documentation.

## RESULTS

Informatica used the tools in this repository to measure the
performance of the SMX transport.

In the results below, "K" represents 1,000. "M" represents 1,000,000.

The SMX transport's maximum sustainable message rate (throughput) speed is
very dependent on the speed of the CPU and the bandwidth to and from memory.
Note that the highest performance can often be seen on hosts with
a single CPU chip and a single NUMA node.
However, we feel that this is not representative of the types of hosts
our users have access to,
so we chose a host with two CPU chips and two NUMA nodes.
See [Informatica Test Hardware](#informatica-test-hardware) for detailed
specs on our test host.

Maximum Sustainable Message Rate:
* 14M msgs/sec.
* We tested with a variety of message sizes, from 100 bytes to 1500 bytes.
The results were mostly independent of messages size.
Under special circumstances, much higher throughput can be measured
(e.g. over 100M msgs/sec).
However, this cannot be counted on; see
[Measurement Outliers](#measurement-outliers).

Latencies:
* 70-100 ns
* We tested with a variety of message sizes, from 100 bytes to 1500 bytes.
The results were mostly independent of messages size.
* We tested with a range of message rates, from 250K msgs/sec to
10M msgs/sec.
The results were mostly independent of message rate.

Cross-NUMA access is MUCH slower.
Keep senders and receivers in the same NUMA node if possible.
* Max sustainable message rate about a third: ~6M msgs/sec.
* Latencies about double: 170-360 ns.

Slow receivers slow down the source to match the receiver speed.
* When SMX buffer fills,
the next buffer acquisition will busy-loop until the receiver consumes.
* If multiple receivers, *all* must consume before the sender can resume.
All receivers get all messages.

Multiple receivers slow SMX down
* Maximum sustainable message rate with two or three receivers: 11M msgs/sec.
* As above, mostly independent of message size.
* Latencies only slightly elevated.
* As above, mostly independent of message size and message rate.

## REPRODUCE RESULTS

It is assumed in all of these steps that the "LD_LIBRARY_PATH" environment
variable includes the path to the Ultra Messaging version 6.14
library directory.
For example:
````
LBM=$HOME/UMP_6.14/Linux-glibc-2.17-x86_64  # Modify according to your needs.

export LD_LIBRARY_PATH=$LBM/lib
````

### Requirements

1. Linux-based server (X86, 84-bit, 4 cores or more, hyperthreading turned off,
8 gigabytes or more memory.
2. C compiler (gcc) and related tools.
3. Ultra Messaging version 6.14, including development files (lbm.h,
libraries, etc.).

See [Test Hardware](#informatica-test-hardware) for details of Informatica's
test host.

### Choose CPUs

Different hardware systems assign CPU numbers to NUMA nodes differently.
Some do even/odd assignments; others do first-half/second-half.

Enter the Linux command "lscpu". Here is an excerpt of its output:
````
$ lscpu
Architecture:        x86_64
CPU op-mode(s):      32-bit, 64-bit
Byte Order:          Little Endian
CPU(s):              12
On-line CPU(s) list: 0-11
Thread(s) per core:  1
Core(s) per socket:  6
Socket(s):           2
NUMA node(s):        2
...
NUMA node0 CPU(s):   0,2,4,6,8,10
NUMA node1 CPU(s):   1,3,5,7,9,11
...
````

Choose two CPU numbers on the same NUMA node.
We usually stay away from CPU numbers 0 and 1.

For our testing, we chose CPUs 5 and 7, which are on NUMA node 1.

### Build Test Tools

The standard UM example applications were not designed to measure the maximum
sustainable message rate for SMX, or message latency under load.

A new pair of tools were written:
* smx_perf_pub - publisher (source).
* smx_perf_sub - subscriber (receiver).

The source code for these tools can be found in the GitHub repository
"smx_perf" at: https://github.com/UltraMessaging/smx_perf
The files can be obtained by cloning the repository using "git" or
[GitHub Desktop](https://desktop.github.com), or by browsing to
https://github.com/UltraMessaging/smx_perf and clicking the green "Code"
button (select "Download ZIP").

To build the tools, the shell script "bld.sh" should be modified.
Here is an excerpt:
````
#!/bin/sh
# bld.sh - build the programs on Linux.

LBM=$HOME/UMP_6.14/Linux-glibc-2.17-x86_64  # Modify according to your needs.
...
````
The assignment to the shell variable "LBM" should be changed to the location
of your UM installation.

### Update Configuration File

The file "smx.cfg" should be modified.
Here is an excerpt:
````
# smx.cfg

context resolver_multicast_address 239.101.3.1
...
````

Contact your network administration group and request a multicast group
that you can use exclusively (for topic resolution).
You don't want your testing to interfere with others,
and you don't want others' activities to interfere with your test.

We used group "239.101.3.1".
Change that to the group provided by your network admins.

WARNING:
The "smx.cfg" configuration is a minimal setup for this test,
and is not suitable for production.
It uses multicast topic resolution for ease of setting up the test,
even though we typically recommend the use of
[TCP-based topic resolution](https://ultramessaging.github.io/currdoc/doc/Design/topicresolutiondescription.html#tcptr).
It also does not contain proper tunings for many other options.
We recommend conducting a configuration workshop with Informatica.

### Measure Maximum Sustainable Message Rate

Open three "terminal" windows to your test host.

***Window 1***: run "top -d 1" to continuously display system usage statistics.
When "top" is running, press the "1" key.
This displays per-CPU statistics.
It may be helpful to expand this window vertically to maximize the number
of lines displayed.

***Window 2***: run "./smx_perf_sub -a 5 -c smx.cfg -f".
Substitute the "5" with the CPU number you previously chose for the message
publisher.
For example:
````
./smx_perf_sub -a 5 -c smx.cfg -f
Core-7911-1: Onload extensions API has been dynamically loaded
o_affinity_cpu=5, o_config=smx.cfg, o_fast=1, o_spin_cnt=0, o_topic='smx_perf', 
Core-9401-4: WARNING: default_interface for a context should be set to a valid network interface.
Core-5688-1833: WARNING: Host has multiple multicast-capable interfaces; going to use [enp5s0f1np1][10.29.4.52].
Core-10403-150: Context (0x33d6a00) created with ContextID (1209888241) and ContextName [(NULL)]
````

***Window 3***: run "./smx_perf_pub -a 7 -c smx.cfg -l 2000 -m 64 -n 100000000 -r 999999999".
Substitute the "7" with the CPU number you previously chose for the message
publisher.
For example:
````
$ ./smx_perf_pub -a 7 -c smx.cfg -l 2000 -m 64 -n 100000000 -r 999999999
Core-7911-1: Onload extensions API has been dynamically loaded
o_affinity_cpu=7, o_config=smx.cfg, o_flags=0x00, o_jitter_loops=0, o_linger_ms=2000, o_msg_len=64, o_num_msgs=100000000, o_rate=999999999, o_topic='smx_perf', o_warmup_loops=10000, 
Core-9401-4: WARNING: default_interface for a context should be set to a valid network interface.
Core-5688-1833: WARNING: Host has multiple multicast-capable interfaces; going to use [enp5s0f1np1][10.29.4.52].
Core-10403-150: Context (0x282da00) created with ContextID (977241557) and ContextName [(NULL)]
actual_sends=100000000, duration_ns=5713119332, result_rate=17503572.775014, 

````
This run took 5.7 seconds to send 100M 64-byte message for a resulting message
rate of 17.5M msgs/sec.

Wait 5 seconds after the publisher completes, and Window 2 will display:
````
rcv event EOS, 'smx_perf', LBT-SMX:ff311d1b:12001[1015551347], num_rcv_msgs=100010000, 
````
Note that the total messages are equal to the requested messages (o_num_msgs)
plus the warmup messages (o_warmup_loops).

Also, note that the "top" command running in Window 1 shows CPU 5 running at
100%, even though the publisher is not running.
The SMX receiver thread does not block or sleep.

#### Other Message Sizes

Here are the maximum sustainable rates measured at Informatica for a variety
of message sizes:

-m msg_len | Maximum Sustainable Message Rate
---------- | --------------------------------
25 | 19,723,235
64 | 17,542,852
65 | 24,179,168
100 | 33,196,206
112 | 32,955,059
113 | 75,687,141
128 | 75,718,195
129 | 18,629,398
200 | 15,916,652
500 | 14,650,158
1500 | 14,720,429

Note the discontinuities at message sizes 65, 113, and 129.
There are other discontinuities,
although they may depend on the hardware used.
We believe these discontinuities result from alignment to
cache lines, affecting when and how data moves from main memory
to the CPU cache.

The higher numbers may not be reliable, especially as application complexity
is added, which will affect the CPU cache. 
This is why we claim throughput of 14M msgs/sec - this seems to be a
reliable performance measure that does not rely on luck.

#### Receiver Workload

The ["-s spin_cnt"](#spin-count) option can be used to add a little bit of
extra work to the subscriber's receiver callback function.
This can be used to illustrate another counter-intuitive effect related
to ["memory contention"](#memory-contention-and-cache-invalidation).

In window 2, restart the subscriber, replacing the "-f" option with "-s 3".
For example:
````
./smx_perf_sub -a 5 -c smx.cfg -s 4
Core-7911-1: Onload extensions API has been dynamically loaded
o_affinity_cpu=5, o_config=smx.cfg, o_fast=1, o_spin_cnt=3, o_topic='smx_perf', 
...
````

Then in window 3, re-run the publisher with 54-byte messages.
For example:
````
./smx_perf_pub -a 7 -c smx.cfg -l 2000 -m 64 -n 100000000 -r 999999999
Core-7911-1: Onload extensions API has been dynamically loaded
o_affinity_cpu=7, o_config=smx.cfg, o_flags=0x00, o_jitter_loops=0, o_linger_ms=2000, o_msg_len=64, o_num_msgs=100000000, o_rate=999999999, o_topic='smx_perf', o_warmup_loops=10000, 
Core-9401-4: WARNING: default_interface for a context should be set to a valid network interface.
Core-5688-1833: WARNING: Host has multiple multicast-capable interfaces; going to use [enp5s0f1np1][10.29.4.52].
Core-10403-150: Context (0x23dba00) created with ContextID (749998171) and ContextName [(NULL)]
actual_sends=100000000, duration_ns=1251633649, result_rate=79895582.928675, 
````

The previous measurement where the receiver used "-f" gave a result rate
of 17.5M msgs/sec.
But by adding four busy-loops inside the subscriber's receiver callback,
the result rate jumps to 79.9M msgs/sec - a 400% speed increase.
We believe this to be due to 
["memory contention"](#memory-contention-and-cache-invalidation);
with the very fast receiver, the publisher always collides with the subscriber
accessing shared memory.
By adding just a small amount of extra work to the receiver callback,
the publisher and subscriber "just miss" each other,
resulting in greatly increased throughput.

The higher numbers may not be reliable, especially as application complexity
is added, which will affect the CPU cache and timing in unpredictable ways. 
This is why we claim a throughput of 14M msgs/sec - this seems to be a
reliable performance measure that does not rely on luck.

## TOOL USAGE NOTES

### smx_perf_pub

````
Usage: smx_perf_pub [-h] [-a affinity_cpu] [-c config] [-f flags]
    [-j jitter_loops] [-l linger_ms] [-m msg_len] [-n num_msgs]
    [-r rate] [-t topic] [-w warmup_loops]
where:
  -h : print help
  -a affinity_cpu : bitmap for CPU affinity for send thread [%d]
  -c config : configuration file; can be repeated [%s]
  -f flags : bitmap [0x%x]: 0x01: FLAGS_TIMESTAMP (to measure latency)
                           0x02: FLAGS_NON_BLOCKING
                           0x04: FLAGS_GENERIC_SRC
  -j jitter_loops : jitter measurement loops [%d]
  -l linger_ms : linger time before source delete [%d]
  -m msg_len : message length [%d]
  -n num_msgs : number of messages to send [%d]
  -r rate : messages per second to send [%d]
  -t topic : topic string [\"%s\"]
  -w warmup_loops : messages to send before measurement loop [%d]
````

### Affinity

The "-a" command-line option is used to specify the CPU core number
to use for the time-critical thread.

For the publisher (smx_perf_pub.c),
the time-critical thread is the "main" thread,
since that is the thread that sends the messages.
The publisher program is typically started with affinity set to a
non-critical CPU core, typically 1, using the "taskset" command.
The publisher creates its context, which creates the context thread,
inheriting the CPU affinity to core 1.
Then, before it starts sending messages,
it sets affinity to the CPU core number specified with the "-a"
command-line option.

For the subscriber (smx_perf_sub.c),
the time-critical thread is the SMX receiver thread.
As with the publisher program,
the subscriber program is typically started with affinity set to a
non-critical CPU core, typically 1, using the "taskset" command.
The publisher creates its context, which creates the context thread,
inheriting the CPU affinity to core 1.
However, at this point, the SMX receiver thread is not created.
That thread is not created by the UM code when a receiver is resolved
to an SMX source for the first time.
The SMX receiver thread sets its affinity during the processing of the
"Beginning Of Stream" (LBM_MSG_BOS) receiver event.

Note that the publisher does not create a receiver object,
and therefore never has an SMX receiver thread.

#### Jitter Measurement

The "-j jitter_loops" command-line option changes smx_perf_sub's function.
It does not send any messages.

Instead, it produces a rough measure of system-induced outliers (jitter).
It does this by simply taking two high-resolution timestamps in a row
using [clock_gettime()](https://linux.die.net/man/3/clock_gettime),
and calculating the difference in nanoseconds.
This measurement is repeated a very large number of times,
keeping track of the minimum and maximum times.

The "minimum" time represents the execution time of a single call to
clock_gettime() (12-13 nanoseconds on our test hosts).
The "maximum" time represents the longest time that Linux interrupted the
execution of the loop.
See [Measurement Outliers](measurement-outliers) for information about
execution interruptions.

We commonly measure interruptions above 100 microseconds,
sometimes above 300 microseconds.

#### Linger Time

The "-l linger_ms" command-line option introduces a delay between the
last message sent and deletion of the UM source.
Informatica generally recommends a delay before deleting the source to
allow any receivers to get caught up.
Once the source is deleted, a receiver that is behind might experience
a type of unrecoverable loss called
"[tail loss](https://ultramessaging.github.io/currdoc/doc/Design/fundamentalconcepts.html#tailloss)".

In the case of SMX,
if the publisher deletes the source object before the subscriber is
able to read all messages from it, the subscriber can experience
tail loss.

#### Warmup

When measuring performance, we recommended performing a number
of "warmup" loops of the time-critical code.
This is to get code and data pages loaded into physical memory, and to
the CPU caches loaded.

The execution of those initial warmup loops is not included in the
performance measurements.

### smx_perf_sub

````
Usage: smx_perf_sub [-h] [-a affinity_cpu] [-c config] [-f]
    [-s spin_cnt] [-t topic]
where:
  -a affinity_cpu : CPU number (0..N-1) for SMX receive thread [%d]
  -c config : configuration file; can be repeated [%s]
  -f : fast - minimal processing in message receive (no latency calcs)
  -s spin_cnt : empty loop inside receiver callback [%d]
  -t topic : topic string [%s]
````

#### Fast

The "-f" command-line option causes the subscriber to use a very minimal
receiver callback that has the lowest-possible per-message execution time.
It is used when measuring the maximum sustainable message rate.

Without "-f", the receiver callback contains code that can sometimes add
enough execution time to mask the effects of
[memory contention](#memory-contention-and-cache-invalidation)
on performance.

#### Spin Count

There is an empty "for" loop inside the receiver callback:
````C
for (global_counter = 0; global_counter < o_spin_cnt; global_counter++) {
}
````
This loop is used to add a small amount of per-message "work" to the subscriber.
Note that the "global_counter" variable is made global so the compiler
won't optimize the loop away.

This is used to explore the effects of
[memory contention](#memory-contention-and-cache-invalidation)
on performance.

## MEASUREMENT OUTLIERS

The SMX transport code is written to provide a very constant execution time.
Dynamic memory (malloc/free) is not used during message transfer.
User data is not copied between buffers.
There is no significant source for measurement outliers
(jitter) in the SMX code itself.

However, the measurements made at Informatica show significant outliers.
Two environmental factors cause these outliers:
* Interruptions.
* Memory contention and cache invalidation.

### Interruptions

There are many sources of execution interruptions on a CPU core running
a typical OS (Linux or Windows).
Some of them are actual hardware interrupts, like page faults,
disk controllers, NICs, and timers.
Others are soft, like virtual memory maintenance,
scheduler-related operations, and potentially even system
or user processes "stealing" time from the main application.
It is possible to eliminate or at least reduce many of these sources of
interrupt by modifying the host's configuration,
both in its BIOS and the kernel.
For example, see:
* https://lwn.net/Articles/549580/
* https://lwn.net/Articles/659490/
* https://www.kernel.org/doc/Documentation/timers/NO_HZ.txt

There are many other kernel tweaks and optimizations that can be made
to prevent interruptions and allow the application to minimize instances
of giving up the CPU.
Informatica recommends that users work with an experienced Linux performance
engineer to understand the tradeoffs of potential optimizations.
However, Informatica does not know of any way to eliminate
all interruptions.

Without doing these optimizations,
the test results are highly susceptible to interruptions.

See [Jitter Measurement](#jitter-measurement) for a method to measure
these interruptions.

### Memory Contention and Cache Invalidation

There are two places where memory contention plays a significant role
in varying the measured performance of SMX:
when the shared memory is empty of messages and the receiver is waiting for one,
and when the shared memory is full, and the publisher is waiting for
an opening to be made.
In both cases, one CPU is continuously reading a word of shared memory,
waiting for the other CPU to write something into it.
When the "writer" CPU is ready to write the value that the "reader" CPU
is waiting for, the hardware must serialize the accesses and maintain
cache coherency, which introduces wait states for both CPUs.

Take the case where the memory segment is empty, and the subscriber is
waiting for the publisher to send a message.
The SMX receiver code is tightly polling the "head index",
waiting for the publisher to move it. The memory contention associated with
the "send" function significantly slows down updating the
head index.
If we assume that the receiver code is faster than the sender code,
the receiver will quickly process the message and go back to tightly
reading the head index.
Thus, the publisher will encounter this contention on every single message sent,
resulting in a baseline throughput.

Now let's modify the situation.
Let's add a little bit of work in the subscriber's receiver callback.
In the "smx_perf_sub" program, this is simply an empty "for" loop that
spins a requested number of times (see the
["-s spin_cnt"](#spin-count) command-line option).
If the receiver spins only three times for each received message,
this allows the sender to update the shared memory before the receiver
has finished.
This greatly increases the speed of that send operation,
and also increases the speed of the subsequent read operation.
Informatica has seen throughput increase by over 400% by
simply adding that little bit of work to the receiver.
The memory accesses just miss each other instead of colliding.

This effect can be demonstrated in [Receiver Workload](#receiver-workload).
Note that the effect is more pronounced with smaller messages sizes.

In a real-world environment where traffic bursts intensely,
followed by periods of idleness, it frequently happens that the first
message of a burst will have the contention,
but many of the subsequent messages in the burst can avoid the contention.
However, this is typically not something that you can count on.
So for this report, the "fully-contended" throughput is the one we emphasize.
If, in practice, the sender and receiver sometimes synchronize in a way that
improves throughput, that's a nice benefit, but not a guarantee.

## CODE NOTES

We attempt to explain some of the "why"s of non-obvious parts of the code.

### Error Handling

Informatica strongly advises users to check return status for errors after
every UM API call.
As this can clutter the source code, making it harder to read,
the "smx_perf_pub.c" and "smx_perf_sub.c" programs use a code macro called
"E()" to make the handling of UM API errors uniform and non-intrusive.
For example:
````C
E(lbm_config(o_config));
````

The "E()" code macro checks for error, prints the source code file name,
line number, and the UM error message, and then calls "exit(1)",
terminating the program with bad status.

In a production program, users typically have their own well-defined
error handling conventions which typically includes logging messages to
a file and alerting operations staff of the exceptional condition.
Informatica does not recommend the use of this "E()" macro in production
programs, at least not as it is implemented here.

Another simple shortcut macro is "PERRNO()" which prints the source code
file name, line number, and the error message associated with the contents
of "errno", then calls "exit(1)", terminating the program with bad status.
This is useful if a system library function fails.

### SAFE_ATOI

This is a helper macro that is similar to the system function "atoi()"
with three improvements:
* Automatically conforms to different integer types
(8-bit, 16-bit, 32-bit, 64-bit, signed or unsigned).
* Treats numbers with the "0x" prefix as hexadecimal.
* Adds significant error checking and reporting.

As with the "E()" macro, it accomplishes these goals without code clutter.

### DIFF_TS

This is a helper macro that subtracts two "struct timespec" values, as
returned by [clock_gettime()](https://linux.die.net/man/3/clock_gettime),
and puts the difference into a uint64_t variable
as the number of nanoseconds between the two timestamps.

For example:
````C
struct timespec start_ts;
struct timespec end_ts;
uint64 duration_ns;  /* In nanoseconds. */

clock_gettime(&start_ts);
... /* code to be timed */
clock_gettime(&end_ts);
DIFF_TS(duration_ns, end_ts, start_ts);
````

### send_loop()

The "send_loop()" function in "smx_perf_pub.c" does the work of
sending messages at the desired rate.
It is designed to "busy-loop" between sends so that the time spacing between
messages is as constant and uniform as possible.
The message traffic is not subject to bursts.

This approach is not intended to model the behavior of a real-life trading system,
where message traffic is highly subject to intense bursts.
Generating bursty traffic is very important when testing trading system
designs,
but is not desired when measuring maximum sustainable throughput and
latency under load.

Maximum sustainable throughput is the message rate at which the subscriber
can just barely keep up.
Sending a burst of traffic at a higher rate can be accommodated
temporarily by buffering the excess messages until the burst is over.
After the burst, the send rate needs to drop below the maximum sustainable
message rate so that the subscriber can empty the buffer and get caught up.
But none of this is useful in measuring the maximum sustainable throughput.
Instead, evenly-spaced messages should be sent to get an accurate measurement
of the maximum sustainable throughput.
This gives you a baseline for calculating the size of the buffer required to
handle bursts of a maximum intensity and duration.

Similarly, sending bursts of traffic that exceed the maximum
sustainable throughput is not desired for measuring the latency under load
of the underlying messaging system.
Again, a burst can be accommodated temporarily by buffering the excess messages,
but this simply adds buffering latency,
which is not the latency that we are trying to measure.

When running at or near the maximum sustainable throughput,
some amount of buffering latency is inevitable due to the subscriber being
susceptible to [execution interruptions](#interruptions).
This contributes to latency variation since messages after a
subscriber interruption can experience buffering latency if the subscriber
hasn't yet gotten caught up.

## INFORMATICA TEST HARDWARE

Here are command excerpts that document the host used to
perform the in-house measurements.

````
$ lscpu
Architecture:        x86_64
CPU op-mode(s):      32-bit, 64-bit
Byte Order:          Little Endian
CPU(s):              12
On-line CPU(s) list: 0-11
Thread(s) per core:  1
Core(s) per socket:  6
Socket(s):           2
NUMA node(s):        2
Vendor ID:           GenuineIntel
CPU family:          6
Model:               79
Model name:          Intel(R) Xeon(R) CPU E5-2643 v4 @ 3.40GHz
Stepping:            1
CPU MHz:             1507.237
CPU max MHz:         3700.0000
CPU min MHz:         1200.0000
BogoMIPS:            6800.30
Virtualization:      VT-x
L1d cache:           32K
L1i cache:           32K
L2 cache:            256K
L3 cache:            20480K
NUMA node0 CPU(s):   0,2,4,6,8,10
NUMA node1 CPU(s):   1,3,5,7,9,11
...

$ vmstat -s
     65608176 K total memory
...

$ cat /etc/centos-release
CentOS Linux release 8.0.1905 (Core)
````
