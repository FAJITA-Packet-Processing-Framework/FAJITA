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
    if (Args(conf, this, errh)
            .read_or_set("SIZE", _size, 1)
            .read_or_set("PREFETCH", _prefetch, 0)
            .complete() < 0)
        return -1;
    return 0;
}


int FlowLargeFCB::initialize(ErrorHandler *errh) {
    return 0;
}

bool FlowLargeFCB::new_flow(LargeFCBState* flowdata, Packet* p)
{
    /*
    flowdata->iarray = (int *) CLICK_ALIGNED_ALLOC(_size*sizeof(int));

    bzero(flowdata->iarray,_size * sizeof(int));
    */
    return true;
}

void FlowLargeFCB::prefetch_fcb(int line_idx, LargeFCBState* flowdata){
    if (!_prefetch)
        return;

//    rte_prefetch0((flowdata->iarray)+(line_idx*64));

/*
    for (int i = 0; i< ((_size/16)+1) && i < _prefetch; i++){
        rte_prefetch0((flowdata->iarray)+(i*64));
    }
*/
}

void FlowLargeFCB::push_flow_batch(int, LargeFCBState** flowdata, PacketBatch *head){
    

    int pkt_idx = 0;
    FOR_EACH_PACKET_SAFE(head, p) {

        int a = 0;
        for (int j = 0; j < _size; j++){
            a += flowdata[pkt_idx]->inplace_array[j];
        }
        if (a > 10)
            flowdata[pkt_idx]->inplace_array[0]+=a%10;
        pkt_idx++;
    }

}

void FlowLargeFCB::push_flow(int port, LargeFCBState* flowdata, PacketBatch* batch) {
    int *iarray = flowdata->iarray;

    for (int i = 0; i< 10; i++){
        iarray[i]++;
        iarray[i] = iarray[i]%500;
    }
        
    output_push_batch(0, batch);
}


CLICK_ENDDECLS

ELEMENT_REQUIRES(flow)
EXPORT_ELEMENT(FlowLargeFCB)
ELEMENT_MT_SAFE(FlowLargeFCB)