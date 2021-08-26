# smx_perf - test programs to measure the performance of Ultra Messaging's SMX transport.

## Copyright and License

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

## MEASUREMENT OUTLIERS

The SMX transport code is written to provide very constant execution time.
Dynamic memory (malloc/free) is not used during message transfer.
User data is not copied between buffers.
There is no significant source for measurement variance in the SMX code itself.

However, the measurements made at Informatica show significant outliers.
These outliers are caused by two environmental factors:
* Interruptions.
* Memory contention and cache invalidation.

### Interruptions

There are many sources of interruptions on a CPU core running a typical OS (Linux or Windows).
Some of them are actual hardware interrupts, like page faults,
disk controllers, NICs, and timers.
Others are soft, like virtual memory maintenance (e.g. page faults),
scheduler-related operations, and potentially even system
or user processes "stealing" time from the main application.
It is possible to reduce or eliminate many of these sources of interrupt
by modifying the host's configuration, both in its BIOS and the kernel.
For example, see:
* https://lwn.net/Articles/549580/
* https://lwn.net/Articles/659490/
* https://www.kernel.org/doc/Documentation/timers/NO_HZ.txt

There are many other kernel tweaks and optimizations that can be made
to prevent interruptions and allow the application to minimize instances
of giving up the CPU.
However, Informatica does not know of any way to completely eliminate
all interruptions.

Without doing these optimizations,
the test results are extremely susceptible to interruptions.

The "smx_perf_pub.c" program contains a test function named "jitter_loop()"
which simply calls "clock_gettime()" twice in and subtracts the two times.
It does this in a tight loop and keeps track of the minimum and maximum
time differences.
The minimum time difference essentially documents the execution time of
"clock_gettime()", and is 12 ns on our test hardware.
But much more interesting are the maximum times.
It is common to see time differences in the hundreds of microseconds
(e.g. 384 for one of our test runs).
These outliers are caused by interruptions.

## Memory Contention and Cache Invalidation

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

## Code Notes

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

