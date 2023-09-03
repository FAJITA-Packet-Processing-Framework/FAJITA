/*
 * EtherRewrite.{cc,hh} -- encapsulates packet in Ethernet header
 * Tom Barbette
 *
 * Batching support from Georgios Katsikas
 *
 * Copyright (c) 2015 University of Liege
 * Copyright (c) 2017 Georgios Katsikas, RISE SICS
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
#include "simplecache.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

SimpleCache::SimpleCache()
{
}

SimpleCache::~SimpleCache()
{
}

int
SimpleCache::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
        .read_or_set_p("ROWS", _rows_number, 65536)
        .complete() < 0)
       return -1;

    _rows_number = next_pow2(_rows_number);
    _row_mask = _rows_number - 1;

    _rows = (Row*)CLICK_ALIGNED_ALLOC(sizeof(Row) * _rows_number);
    bzero(_rows,sizeof(Row) * _rows_number);
    
    return 0;
}

inline Packet *
SimpleCache::smaction(Packet *p)
{
    uint32_t hash = p->anno_u32(20);
    uint32_t cache_row = hash & _row_mask;

    for (int i = 0; i < SETS_NUM; i++){
        if (_rows[cache_row].offset[i] == hash){
            _successful_searches++;
            _rows[cache_row].replacement_candidate = 1 - i;
            return p;
        }
    }

    _failed_searches++;
    uint32_t replacement_idx = click_random()%SETS_NUM;
//    uint32_t replacement_idx = _rows[cache_row].replacement_candidate;
    _rows[cache_row].offset[replacement_idx] = hash;
    _rows[cache_row].replacement_candidate = 1 - replacement_idx;

    return p;
}

inline void
SimpleCache::push(int, Packet *p)
{
    
    smaction(p);
    output(0).push(p);
}

#if HAVE_BATCH
void
SimpleCache::push_batch(int, PacketBatch *batch)
{
    FOR_EACH_PACKET(batch, p){
        smaction(p);
    }

    output(0).push_batch(batch);
}

#endif

enum {
        h_failed_searches,
        h_successful_searches,
        h_success_rate
    };

String
SimpleCache::read_handler(Element *e, void *thunk)
{
    SimpleCache *c = reinterpret_cast<SimpleCache *>(e);

    switch ((intptr_t)thunk) {
        case h_failed_searches: {
            return String(c->_failed_searches);
        }
        case h_successful_searches: {
            return String(c->_successful_searches);
        }
        case h_success_rate: {
            return String((double) c->_successful_searches / (double) (c->_successful_searches + c->_failed_searches));
        }
        default: {
            return String("<error>");
        }
    }
}


void
SimpleCache::add_handlers()
{
    add_read_handler("failed_searches", read_handler, h_failed_searches);
    add_read_handler("successful_searches", read_handler, h_successful_searches);
    add_read_handler("success_rate", read_handler, h_success_rate);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SimpleCache)
