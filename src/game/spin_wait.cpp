// Busy-wait loops (tools/find_spin_loops.py lists them). The game polls a
// few flags in loops with no OS call:
//   func_80025CA4 0x80025D30  the idle thread, waiting for a callback
//   func_80025E44 0x80025FB0  the main thread, waiting on D_80068A95
//   func_8004D7B0 0x8004D7B0  waiting for the job thread to finish its jobs
// On the N64, interrupts preempt them whenever an event wakes another
// thread. The runtime's scheduler only switches threads inside OS calls, and
// only delivers VI and other events when a game thread waits for one, so a
// spinning thread kept every other thread waiting forever. The hooks in
// snp.jp.toml call snp_spin_yield on each pass instead.
#include <cstdint>

#include "ultramodern/ultramodern.hpp"

#include "snp_recomp_hooks.h"

// ultramodern/src/mesgqueue.cpp, not declared in a header.
void dequeue_external_messages(uint8_t* rdram);

extern "C" int32_t osGetThreadPri(uint8_t* rdram, int32_t t);
extern "C" void osSetThreadPri(uint8_t* rdram, int32_t t, int32_t pri);

extern "C" void snp_spin_yield(uint8_t* rdram) {
    // Wait up to 1 ms for an event (so the loop doesn't spin a host core),
    // then deliver every pending one: the runtime's own delivery takes one
    // at a time and can fall behind.
    ultramodern::wait_for_external_message_timed(rdram, 1);
    dequeue_external_messages(rdram);
    ultramodern::check_running_queue(rdram);

    // That only switches to a strictly higher-priority thread, but the thread
    // a loop waits on can have a lower priority than the spinning one (the
    // job thread, for the main thread). Dropping to priority 0 for a moment
    // lets every ready thread run first. (The idle thread is already at 0.)
    int32_t pri = osGetThreadPri(rdram, 0);
    if (pri != 0) {
        osSetThreadPri(rdram, 0, 0);
        osSetThreadPri(rdram, 0, pri);
    }
}
