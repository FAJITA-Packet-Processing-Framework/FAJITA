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
            .read_mp("SRCETH", EtherAddressArg(), _ethh.ether_shost)
	    .read_mp("DSTETH", EtherAddressArg(), _ethh.ether_dhost)
	    .read_mp("SRCIP", _sipaddr)
	    .read_mp("DSTIP", _dipaddr)
	    .read_or_set("LENGTH", _len, 1024)
//            .read_or_set_p("PORT_OFFSET_DST", _portoffset_dst, 0)
            .complete() < 0)
        return -1;

    _ethh.ether_type = htons(0x0800);

    return 0;
}

int BatchMerge::initialize(ErrorHandler * errh) {
    WritablePacket *q = Packet::make(_len);
    if (unlikely(!q)){
        return errh->error("Could not initialize packet, out of memory?");
    }
    
    resultp = q;

    memcpy(q->data(), &_ethh, 14);
    click_ip *ip = reinterpret_cast<click_ip *>(q->data()+14);
    click_udp *udp = reinterpret_cast<click_udp *>(ip + 1);

    ip->ip_v = 4;
    ip->ip_hl = sizeof(click_ip) >> 2;
    ip->ip_len = htons(_len-14);
    ip->ip_id = 0;
    ip->ip_p = IP_PROTO_UDP;
    ip->ip_src = _sipaddr;
    ip->ip_dst = _dipaddr;
    ip->ip_tos = 0;
    ip->ip_off = 0;
    ip->ip_ttl = 250;
    ip->ip_sum = 0;
    ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
    
    resultp->set_dst_ip_anno(IPAddress(_dipaddr));
    resultp->set_ip_header(ip, sizeof(click_ip));

    udp->uh_sport = (click_random() >> 2) % 0xFFFF;
    udp->uh_dport = (click_random() >> 2) % 0xFFFF;
    udp->uh_sum = 0;
    unsigned short len = _len-14-sizeof(click_ip);
    udp->uh_ulen = htons(len);
    udp->uh_sum = 0;
    click_chatter("Template Packet Generated!");
    return 0;
}

inline Packet* BatchMerge::process(PacketBatch *batch) {
    
    Packet *packet = resultp->clone();
    WritablePacket* q = packet->uniqueify();
    click_ip *resultp_ip = q->ip_header();
    int off = q->transport_header_offset() + sizeof(click_udp);
    int i = 0;

    FOR_EACH_PACKET_SAFE(batch, p) {
	WritablePacket *processingp = p->uniqueify();
        click_ip *processing_ip = reinterpret_cast<click_ip *>(processingp->data()+14);
        memcpy(q->data() + off + (i*sizeof(click_ip)) , processing_ip, sizeof(click_ip));
        i++;
    }
    
    return q;
}

#if HAVE_BATCH
void BatchMerge::push_batch(int, PacketBatch *batch) {
    Packet *result = process(batch);
    batch->kill();batch = 0;
    output_push_batch(0, PacketBatch::make_from_packet(result));

//    output(0).push_batch(batch);
//    output_push_batch(0, batch);
}
#endif

Packet *BatchMerge::simple_action(Packet *p) {
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BatchMerge)
ELEMENT_MT_SAFE(BatchMerge)
