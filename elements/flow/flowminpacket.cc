/*
 * FlowMinPacket.{cc,hh} -- remove insults in web pages
 * Tom Barbette
 */

#include <click/config.h>
#include <click/router.hh>
#include <click/args.hh>
#include <click/error.hh>
#include "flowminpacket.hh"

CLICK_DECLS

FlowMinPacket::FlowMinPacket()
{

}

int FlowMinPacket::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if(Args(conf, this, errh)
    .read_mp("MIN", _min)
    .complete() < 0)
        return -1;

    return 0;
}

void FlowMinPacket::push_flow(int, FMPState* fcb, PacketBatch* flow)
{
    fcb->count += flow->count();
    if (fcb->count >= _min) {
        if (fcb->p) {
            fcb->p->append_batch(flow);
            fcb->p = 0;
        }
        output_push_batch(0, flow);
    } else {
        if (fcb->p) {
            fcb->p->append_batch(flow);
        } else {
            fcb->p = flow;
        }
    }
    
}


String
FlowMinPacket::read_handler(Element *e, void *thunk)
{
    FlowMinPacket *fd = static_cast<FlowMinPacket *>(e);
    switch ((intptr_t)thunk) {
      default:
          return "<error>";
    }
}

int
FlowMinPacket::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    return -1;
}

void
FlowMinPacket::add_handlers() {

}

CLICK_ENDDECLS
EXPORT_ELEMENT(FlowMinPacket)
ELEMENT_MT_SAFE(FlowMinPacket)
