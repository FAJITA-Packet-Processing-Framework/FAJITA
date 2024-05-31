/*
 * flowcounter.{cc,hh} -- remove insults in web pages
 * Tom Barbette
 */

#include <click/config.h>
#include <click/router.hh>
#include <click/args.hh>
#include <click/error.hh>
#include "flowacl.hh"

CLICK_DECLS

FlowACL::FlowACL()
{

}

int FlowACL::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if(Args(conf, this, errh)
    .complete() < 0)
        return -1;

    return 0;
}

void FlowACL::push_flow(int, int* fcb, PacketBatch* flow)
{
    output_push_batch(0, flow);
}

#if FLOW_PUSH_BATCH
inline void FlowACL::push_flow_batch(int port, int** fcb, PacketBatch *head) 
{
}
#endif

enum { h_count };

String
FlowACL::read_handler(Element *e, void *thunk)
{
    return "<error>";
}

int
FlowACL::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    return -1;
}

void
FlowACL::add_handlers() {
    add_read_handler("count", read_handler, h_count);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FlowACL)
ELEMENT_MT_SAFE(FlowACL)
