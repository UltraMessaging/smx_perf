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

## USAGE NOTES

### smx_perf_pub

````
Usage: smx_perf_pub [-h] [-a affinity_cpu] [-c config] [-f flags] [-j jitter_loops] [-l linger_ms] [-m msg_len] [-n num_msgs] [-r rate] [-t topic] [-w warmup_loops]
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
clock_gettime() (12-13 nanoseconds on our test machines).
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

### smx_perf_sub

````
Usage: smx_perf_sub [-h] [-a affinity_cpu] [-c config] [-f] [-s spin_cnt] [-t topic]
where:
  -a affinity_cpu : CPU number (0..N-1) for SMX receive thread [%d]
  -c config : configuration file; can be repeated [%s]
  -f : fast - minimal processing in message receive (no latency calcs)
  -s spin_cnt : empty loop inside receiver callback [%d]
  -t topic : topic string [%s]
````

## MEASUREMENT OUTLIERS

The SMX transport code is written to provide very constant execution time.
Dynamic memory (malloc/free) is not used during message transfer.
User data is not copied between buffers.
There is no significant source for measurement outliers
(jitter) in the SMX code itself.

However, the measurements made at Informatica show significant outliers.
These outliers are caused by two environmental factors:
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
However, Informatica does not know of any way to completely eliminate
all interruptions.

Without doing these optimizations,
the test results are extremely susceptible to interruptions.

See [Jitter Measurement](#jitter-measurement) for a method to measure
these interruptions.

### Memory Contention and Cache Invalidation

There are two places where memory contention plays a significant role
in varying the measured performance of SMX:
when the shared memory is empty of messages and the receiver is waiting for one,
and when the shared memory is full and the publisher is waiting for
an opening to be made.
In both cases, one CPU is continuously reading a word of shared memory,
waiting for the other CPU to write something into it.
When the "writer" CPU is ready to write the value that the "reader" CPU
is waiting for, the hardware must serialize the accesses and maintain
cache coherency, which introduces wait states for both CPUs.

Take the case where the memory segment is empty and the subscriber is
waiting for the publisher to send a message.
The SMX receiver code is tightly polling the "head index",
waiting for the publisher to move it. The memory contention associated with
the "send" function significantly slows down the process of updating the
head index.
If we assume that the receiver code is faster than the sender code,
the receiver will quickly process the message and go back to tightly
reading the head index.
Thus, the publisher will encounter this contention on every single message sent,
resulting in a baseline throughput.

Now let's modify the situation.
Let's add a little bit of work in the subscriber's receiver callback.
In the "smx_perf_sub" program, this is simply an empty "for" loop that
spins a requested number of times.
If the receiver spins only 3 times for each received message,
this allows the sender to update the shared memory before the receiver
has finished.
This greatly increases the speed of that send operation,
and it also increases the speed of the subsequent read operation.
Informatica has seen throughput increase by a factor of more than 4 by
simply adding that little bit of work to the receiver.
The memory accesses just miss each other instead of colliding.

This effect can be seen in the section "Detailed results" below.
Also note that the effect is more-pronounced with smaller messages sizes.
The exact reasons are not well-understood.

In a real-world environment where traffic bursts intensely,
followed by periods of idleness, it frequently happens that the first
message of a burst will have the contention,
but many of the subsequent messages in the burst can avoid the contention.
However, this is typically not something that can be counted on.
So for this report, the "fully-contended" throughput is the one we emphasize.
If in practice the sender and receiver sometimes synchronize in a way that
improves throughput, that's a nice benefit, but not a guarantee.

## CODE NOTES

This section explains various potentially non-obvious implementation details
of the smx_perf_pub.c and smx_perf_sub.c programs.

We attempted to write the test programs with a minimum of features and
complexity so that the basic algorithms can be seen clearly.
However, some explanation can help the user to understand the code more quickly.

### Thread Affinity

The "-a" command-line option is used to specify the CPU core number
to use for the time-critical thread.

For the publisher (smx_perf_pub.c),
the time critical thread is the "main" thread,
since that is the thread which sends the messages.
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

### Error Handling

Informatica strongly advises users to check return status for errors after
every UM API call.
As this can clutter the source code, making it harder to read,
the "smx_perf_pub.c" and "smx_perf_sub.c" programs use a code macro called
"E()" to make the handling of errors uniform and non-intrusive.
For example:
````
E(lbm_config(o_config));
````

The "E()" code macro checks for error, prints the source code file name,
line number, and the UM error message, and then calls "exit(1)",
terminating the program with bad status.

In a production program, users typically have their own well-defined
error handling conventions which typically includes logging messages to
a file and alerting operations staff of the exceptional condition.
Informatica does not recommend the use of the "E()" macro in production
programs, at least as it is implemented in these test programs.

Another simple shortcut macro is "PERRNO()" which prints the source code
file name, line number, and the error message associated with the contents
of "errno", then calls "exit(1)", terminating the program with bad status.
This is useful if a system library function fails.

### SAFE_ATOI

This is a helper macro that is similar to the system function "atoi()"
with three improvements:
* Automatically conforms to different integer types
(8-bit, 16-bit, 32-bit, 64-bit, signed or unsigned).
* Adds significant error checking and reporting.
* Treats numbers with the "0x" prefix as hexadecimal.

### DIFF_TS

This is a helper macro that subtracts two "struct timespec" values, as
returned by "clock_gettime()", and puts the difference into a uint64_t variable
as the number of nanoseconds between the two timespecs.

### send_loop()

The "send_loop()" function in "smx_perf_pub.c" does the work of
sending messages at a desired rate.
It is designed to "busy loop" between sends so that the time spacing between
messages is as constant and uniform as possible.
Which is to say, the message traffic is not subject to bursts.

This is not intended to model the behavior of a real-life trading system,
where message traffic is highly subject to intense bursts.
Generating bursty traffic is very important when testing trading system
designs,
but is not desired when measuring maximum sustainable throughput and
latency under load.

Maximum sustainable throughput is the message rate at which the subscriber
can just barely keep up.
Sending a burst of traffic at higher than that rate can be accommodated
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
This contributes to latency variation, since messages subsequent to a
subscriber interruption can experience buffering latency if the subscriber
hasn't yet gotten caught up.
