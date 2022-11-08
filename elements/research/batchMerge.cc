/*
 * shifter.{cc,hh} -- shift packets' 4-tuple fields by a given offset
 * Massimo Girondi
 *
 * Copyright (c) 2021 KTH Royal Institute of Technology
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
#include <clicknet/ip.h>
#include "batchMerge.hh"
#include <click/args.hh>
#include <click/etheraddress.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/glue.hh>
#include <click/router.hh>
#include <click/standard/alignmentinfo.hh>
#include <click/standard/scheduleinfo.hh>

CLICK_DECLS

BatchMerge::BatchMerge() {}
BatchMerge::~BatchMerge() {}

int BatchMerge::configure(Vector<String> &conf, ErrorHandler *errh) {
    if (Args(conf, this, errh)
	    .read_or_set("SPLITED_LEN", _len, 74)
	    .read_or_set("MAX_PKT_LEN", _max_packet_len, 1500)
//            .read_or_set_p("PORT_OFFSET_DST", _portoffset_dst, 0)
            .complete() < 0)
        return -1;

    return 0;
}

int BatchMerge::initialize(ErrorHandler * errh) {
    _input_count = 0;
    _output_count = 0;
    return 0;
}

inline PacketBatch* BatchMerge::process(PacketBatch *batch) {
    
    uint32_t pad_size = _max_packet_len - _len;
    uint32_t split_count = (_max_packet_len - _len) / _len;
  
    WritablePacket* iterate = batch->first()->put(pad_size);
    batch = batch->pop_front();
    PacketBatch* resultBatch = PacketBatch::make_from_packet(iterate);
      
    int i = 1;
 
    FOR_EACH_PACKET_SAFE(batch, p) {
	if (i%split_count == 0){ // Create a new MergedPacket
            iterate = p->put(pad_size);
	    iterate->set_next(0);
	    resultBatch->append_packet(iterate);
	    i++;
	    continue;
        }
        
	// WritablePacket *processingp = p->uniqueify();
        // click_ip *processing_ip = reinterpret_cast<click_ip *>(processingp->data());
        memcpy(iterate->data() + ((i%split_count)*_len) , p->mac_header(), _len);
	p->kill();
        i++;
    }
    batch=0;
    
    return resultBatch;
}

#if HAVE_BATCH
void BatchMerge::push_batch(int, PacketBatch *batch) {
    
    _input_count+=batch->count();

    PacketBatch *result = process(batch);

    _output_count+=result->count();

    output(0).push_batch(result);

//    output_push_batch(0, batch);
}
#endif

Packet *BatchMerge::simple_action(Packet *p) {
    return p;
}

enum {h_merge_size};

String BatchMerge::read_handler(Element* e, void* thunk)
{
    BatchMerge* fc = static_cast<BatchMerge*>(e);
    
    switch ((intptr_t)thunk) {
        case h_merge_size:
            return String((float) fc->_input_count / (float) fc->_output_count);
        default:
            return "<error>";
    }

}

void BatchMerge::add_handlers()
{
    add_read_handler("avg_merge_size", read_handler, h_merge_size);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(BatchMerge)
ELEMENT_MT_SAFE(BatchMerge)
