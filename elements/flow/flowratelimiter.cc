/*
 * flowratelimiter.{cc,hh} -- limit the packet rate per flow
 * Tom Barbette
 */

#include <click/config.h>
#include <click/router.hh>
#include <click/args.hh>
#include <click/error.hh>
#include "flowratelimiter.hh"

CLICK_DECLS

FlowRateLimiter::FlowRateLimiter()
{

}

int FlowRateLimiter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if(Args(conf, this, errh)
    .complete() < 0)
        return -1;
    return configure_helper(&_tb,this,conf,errh);
}

int
FlowRateLimiter::configure_helper(TokenBucket *tb, Element *elt, Vector<String> &conf, ErrorHandler *errh)
{
    unsigned r;
    unsigned dur_msec = 20;
    unsigned tokens;
    bool dur_specified, tokens_specified;
    const char *burst_size = "BURST_SIZE";

    Args args(conf, elt, errh);
	args.read_mp("RATE", r);
    if (args.read("BURST_DURATION", SecondsArg(3), dur_msec).read_status(dur_specified)
	.read(burst_size, tokens).read_status(tokens_specified)
	.complete() < 0)
	return -1;

    if (dur_specified && tokens_specified)
	    return errh->error("cannot specify both BURST_DURATION and BURST_SIZE");
    else if (!tokens_specified) {
        bigint::limb_type res[2];
        bigint::multiply(res[1], res[0], r, dur_msec);
        bigint::divide(res, res, 2, 1000);
        tokens = res[1] ? UINT_MAX : res[0];
    }

    tb->assign(r, tokens ? tokens : 1);
    return 0;
}


void FlowRateLimiter::push_flow(int, FRLState* fcb, PacketBatch* flow)
{
    fcb->tb.refill();
    if (fcb->tb.contains(flow->count())) {
        fcb->tb.remove(flow->count());
        output_push_batch(0, flow);
    } else {
        int n = min(fcb->tb.capacity(), flow->count());
        flow->split(n)->kill();
        output_push_batch(0, flow);
        fcb->tb.remove(n);
    }
    
}


String
FlowRateLimiter::read_handler(Element *e, void *thunk)
{
    FlowRateLimiter *fd = static_cast<FlowRateLimiter *>(e);
    switch ((intptr_t)thunk) {
      default:
          return "<error>";
    }
}

int
FlowRateLimiter::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    return -1;
}

void
FlowRateLimiter::add_handlers() {

}

CLICK_ENDDECLS
EXPORT_ELEMENT(FlowRateLimiter)
ELEMENT_MT_SAFE(FlowRateLimiter)
