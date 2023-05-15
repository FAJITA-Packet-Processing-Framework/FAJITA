/*
 * flowrandload.{cc,hh} -- Element artificially creates CPU burden per flow
 *
 * Tom Barbette
 *
 * Copyright (c) 2020 KTH Royal Institute of Technology
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

#include "flowlargefcb.hh"

#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <click/flow/flow.hh>

CLICK_DECLS

FlowLargeFCB::FlowLargeFCB() {

};

FlowLargeFCB::~FlowLargeFCB() {

}

int
FlowLargeFCB::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int size_in_bytes = 0;
    int size_in_cachelines = 0;
    if (Args(conf, this, errh)
            .read_or_set("SIZE", _size, 1)
            .read_or_set("PREFETCH", _prefetch, 0)
            .read_or_set("SIZEB", size_in_bytes, 0)
            .read_or_set("SIZECL", size_in_cachelines, 0)
            .complete() < 0)
        return -1;

    if (size_in_bytes > 0)
        _size = size_in_bytes / sizeof(int);
    if (size_in_cachelines > 0)
        _size = size_in_cachelines * (CLICK_CACHE_LINE_SIZE / sizeof(int));

    return 0;
}


int FlowLargeFCB::initialize(ErrorHandler *errh) {
    return 0;
}

bool FlowLargeFCB::new_flow(LargeFCBState* flowdata, Packet* p)
{

    flowdata->iarray = (int *) CLICK_ALIGNED_ALLOC(_size*sizeof(int));
    bzero(flowdata->iarray,_size * sizeof(int));

    return true;
}

#if FLOW_PUSH_BATCH
void FlowLargeFCB::prefetch_fcb(int line_idx, LargeFCBState* flowdata){
    if (!_prefetch)
        return;

    for (int i = 0; i < _size; i+=32){
        rte_prefetch0((flowdata->iarray)+i);
    }
/*
    for (int i = 0; i< ((_size/16)+1) && i < _prefetch; i++){
        rte_prefetch0((flowdata->iarray)+(i*64));
    }
*/
}

void FlowLargeFCB::push_flow_batch(int, LargeFCBState** flowdata, PacketBatch *head){
    

    FOR_EACH_PACKET_SAFE(head, p) {
        int *iarray = flowdata[FLOW_ID_ANNO(p)]->iarray;        
        int a = 0;
        for (int i = 0; i < _size; i+=32){
            a += iarray[i];
        }

        if (a > 10)
            iarray[0]+=a%10;
    }

}
#endif

void FlowLargeFCB::push_flow(int port, LargeFCBState* flowdata, PacketBatch* batch) {
    int *iarray = flowdata->iarray;
    int a = 0;
    if (_size < 10){
        a += flowdata->inplace_array[0];
    }
    else {
        for (int i = 0; i < _size; i+=32){
            a += iarray[i];
        }
    }

    if (a > 10){
        iarray[0]+=a%10;
        click_chatter("never should reach here!");        
    }
    output_push_batch(0, batch);
}


CLICK_ENDDECLS

ELEMENT_REQUIRES(flow)
EXPORT_ELEMENT(FlowLargeFCB)
ELEMENT_MT_SAFE(FlowLargeFCB)