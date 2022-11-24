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
#include <algorithm>

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
    
    // For now, we assume no unsplitted packet ends up in this batch!
    uint32_t max_split_count = (_max_packet_len - _len) / _len;
    uint32_t original_batch_count = batch->count();
    uint32_t split_count = std::min(original_batch_count - 1, max_split_count);
    
    int bytes_to_remove_from_head = batch->first()->length() - _len;
    if (bytes_to_remove_from_head < 0)
        click_chatter("EXCEPTION!!!!!!!");
    
    int pad_size = ( _len * split_count ) - bytes_to_remove_from_head;
    
    click_chatter("pad_size: %d, bytes_to_remove_from_head: %d, original_batch_count: %d, split_count: %d", pad_size, bytes_to_remove_from_head, original_batch_count, split_count);

    WritablePacket* iterate = batch->first()->put(pad_size);
    batch = batch->pop_front();
    PacketBatch* resultBatch = PacketBatch::make_from_packet(iterate);
      
    int i = 1;
    uint8_t next_head_index = i + split_count;
    uint8_t remained_packets = original_batch_count - 1;
    FOR_EACH_PACKET_SAFE(batch, p) {
	if (i == next_head_index){ // Create a new MergedPacket
	    // click_chatter("Miad Enja!");
	    remained_packets = original_batch_count - i;
	    if (remained_packets < split_count + 1){
                pad_size = (_len * (remained_packets - 1)) - bytes_to_remove_from_head;
	    }
	    if (pad_size < 0){
                p->take(bytes_to_remove_from_head);
		iterate = p->uniqueify();
            }
            else
                iterate = p->put(pad_size);
	    iterate->set_next(0);
	    resultBatch->append_packet(iterate);
	    i++;
	    next_head_index = i + split_count;
	    continue;
        }
        
        memcpy(iterate->data() + ((i%(split_count+1))*_len) , p->mac_header(), _len);
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
