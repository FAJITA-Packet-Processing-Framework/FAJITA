#ifndef CLICK_FlowLargeFCB_HH
#define CLICK_FlowLargeFCB_HH
#include <click/config.h>
#include <click/multithread.hh>
#include <click/hashtablemp.hh>
#include <click/glue.hh>
#include <click/vector.hh>
#include <click/flow/flowelement.hh>
#include <random>

#define LB_FLOW_TIMEOUT 60 * 1000

CLICK_DECLS

struct LargeFCBState {

    LargeFCBState() {
        bzero(inplace_array, sizeof(int)*10);
    }

	int *iarray;
    int inplace_array [10];
};

/*
 * =c
 * FlowRandLoad([MIN, MAX])
 *
 * =s research
 * 
 * Artificial CPU load, randomly selected per-flow
 *
 * =d
 *
 * For each new flow (using the flow subsystem) this element will select
 * a random number between MIN and MAX, that designates how many
 * PRNG should be done per-packet. Hence, some flow will appear heavy,
 * and some light. One PRNG is around 8 CPU cycles.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item MIN
 *
 * Integer. Minimal number of PRNG to run for each packets. Default is 1.
 *
 * =item MAX
 *
 * Integer. Maximal number of PRNG to run for each packets. Default is 100.
 *
 * =e
 *  FlowIPManager->FlowRandLoad(MIN 1, MAX 100).
 *
 * =a
 * RandLoad, WorkPackage
 */
class FlowLargeFCB : public FlowStateElement<FlowLargeFCB,LargeFCBState> {

public:

    FlowLargeFCB() CLICK_COLD;
    ~FlowLargeFCB() CLICK_COLD;

    const char *class_name() const override		{ return "FlowLargeFCB"; }
    const char *port_count() const override		{ return "1/1"; }
    const char *processing() const override		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *) override CLICK_COLD;
    int initialize(ErrorHandler *errh) override CLICK_COLD;

    static const int timeout = LB_FLOW_TIMEOUT;
    bool new_flow(LargeFCBState*, Packet*);
    void release_flow(LargeFCBState*) {};
    void push_flow(int, LargeFCBState*, PacketBatch *);
#if FLOW_PUSH_BATCH
    void push_flow_batch(int, LargeFCBState**, PacketBatch *);
    void prefetch_fcb(int, LargeFCBState*);
    bool is_fcb_large(){return 1;}
#endif

private:
    int _size;
    int _prefetch;
};





CLICK_ENDDECLS
#endif
