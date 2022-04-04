/*
 * FlowIPManagerFuzzy.{cc,hh} - Flow classification for the flow subsystem
 *
 * Copyright (c) 2019-2020 Tom Barbette, KTH Royal Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/ipflowid.hh>
#include <click/routervisitor.hh>
#include <click/straccum.hh>
#if HAVE_NUMA
#include <click/numa.hh>
#endif
#include "flowipmanagerfuzzy.hh"

CLICK_DECLS

FlowIPManagerFuzzy::FlowIPManagerFuzzy() : _verbose(1), _tables(0), Router::ChildrenFuture(this) {

}

FlowIPManagerFuzzy::~FlowIPManagerFuzzy() {

}

int
FlowIPManagerFuzzy::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
		    .read_or_set_p("CAPACITY", _table_size, 65536)
            .read_or_set("RESERVE", _reserve, 0)
#if HAVE_DPDK && HAVE_NUMA
            .read_or_set("DPDK_NUMA", _dpdk_numa, true)
#endif
            .complete() < 0)
        return -1;

    find_children(_verbose);

    router()->get_root_init_future()->postOnce(&_fcb_builded_init_future);
    _fcb_builded_init_future.post(this);

    return 0;
}

int FlowIPManagerFuzzy::solve_initialize(ErrorHandler *errh) {


    _flow_state_size_full = sizeof(FlowControlBlock) + _reserve;

    auto passing = get_passing_threads();


    _tables = CLICK_ALIGNED_NEW(gtable, passing.size());

    CLICK_ASSERT_ALIGNED(_tables);

    for (int i = 0; i < passing.size(); i++) {
        if (!passing[i])
            continue;

        if (_dpdk_numa)
            _tables[i].fcbs = (FlowControlBlock*)rte_malloc_socket(0, _flow_state_size_full * _table_size, CLICK_CACHE_LINE_SIZE, Numa::get_numa_node_of_cpu(i));
        else
            _tables[i].fcbs =  (FlowControlBlock*)CLICK_ALIGNED_ALLOC(_flow_state_size_full * _table_size);
        click_chatter("Allocating %d", i);
        bzero(_tables[i].fcbs,_flow_state_size_full * _table_size);
        CLICK_ASSERT_ALIGNED(_tables[i].fcbs);
        if (!_tables[i].fcbs)
            return errh->error("Could not init data table %d!", i);

    }
    click_chatter("%p{element} initialized withs", this);

    return Router::ChildrenFuture::solve_initialize(errh);
}

void FlowIPManagerFuzzy::cleanup(CleanupStage stage) {

}


/**
 * Classify a packet in the given group
 * FCB may change!
 */
void FlowIPManagerFuzzy::process(int groupid, Packet* p, BatchBuilder& b) {


    gtable& t = _tables[groupid];


    int ret = AGGREGATE_ANNO(p) % _table_size;

    if (b.last == ret) {
        b.append(p);
    } else {
        PacketBatch* batch;
        batch = b.finish();
        if (batch) {
            fcb_stack->acquire(batch->count());

            assert(fcb_stack);
            output_push_batch(0, batch);
        }
        fcb_stack = (FlowControlBlock*)((unsigned char*)t.fcbs + _flow_state_size_full * ret);
        b.init();
        b.append(p);
    }
}

void FlowIPManagerFuzzy::push_batch(int, PacketBatch* batch) {
    BatchBuilder b;
    int groupid = click_current_cpu_id();
    FOR_EACH_PACKET_SAFE(batch, p) {
        process(groupid, p, b);
    }

    batch = b.finish();
    if (batch) {
        assert(fcb_stack);
        fcb_stack->acquire(batch->count());
        output_push_batch(0, batch);
    }

}

enum {h_count};
String FlowIPManagerFuzzy::read_handler(Element* e, void* thunk) {
   FlowIPManagerFuzzy* fc = static_cast<FlowIPManagerFuzzy*>(e);

   switch ((intptr_t)thunk) {
       case h_count: {
        StringAccum acc;

        return acc.take_string(); }
       default:
        return "<error>";

   };
}

void FlowIPManagerFuzzy::add_handlers() {
    add_read_handler("count", read_handler, h_count);
}


int
FlowIPManagerFuzzy::count()
{
    int total = 0;
    for (int i = 0; i < 32; i++) {
        total += 0;
    }
    return total;
}

int
FlowIPManagerFuzzy::capacity()
{
    return _table_size;
}
/*
int
FlowIPManagerFuzzy::total_capacity()
{
    int total = 0;
    for (int i = 0; i < _groups; i++) {
        total += _table_size;
    }
    return total;
}
*/
CLICK_ENDDECLS

EXPORT_ELEMENT(FlowIPManagerFuzzy)
ELEMENT_MT_SAFE(FlowIPManagerFuzzy)
