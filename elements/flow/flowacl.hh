#ifndef CLICK_FLOWACL_HH
#define CLICK_FLOWACL_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/multithread.hh>
#include <click/flow/flowelement.hh>

CLICK_DECLS


/*
=c

FlowCounter([CLOSECONNECTION])

=s flow

Counts all flows passing by, the number of active flows, and the number of 
packets per flow.

 */


class FlowACL : public FlowStateElement<FlowACL,int>
{
public:
    /** @brief Construct an FlowCounter element
     */
    FlowACL() CLICK_COLD;

    const char *class_name() const override        { return "FlowACL"; }
    const char *port_count() const override        { return PORTS_1_1; }
    const char *processing() const override        { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *) override CLICK_COLD;

    void release_flow(int* fcb) {
    }

    const static int timeout = 15000;

    void push_flow(int port, int* fcb, PacketBatch*);

#if FLOW_PUSH_BATCH
    void push_flow_batch(int port, int** fcb, PacketBatch *head);
#endif

    inline bool new_flow(int* state, Packet*) {
        *state = 1;
        return true;
    }

    void add_handlers() override CLICK_COLD;
protected:

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;
};

CLICK_ENDDECLS
#endif
