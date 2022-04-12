#ifndef CLICK_FlowMinPacket_HH
#define CLICK_FlowMinPacket_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/multithread.hh>
#include <click/flow/flowelement.hh>

CLICK_DECLS


/*
=c

FlowMinPacket()

=s flow

Counts the number of packets per flow and let packet through only if it has at least N packets.

 */
    
struct FMPState {
    long count;
    PacketBatch* p;
};

class FlowMinPacket : public FlowStateElement<FlowMinPacket,FMPState>
{
public:
    /** @brief Construct an FlowMinPacket element
     */
    FlowMinPacket() CLICK_COLD;

    const char *class_name() const override        { return "FlowMinPacket"; }
    const char *port_count() const override        { return PORTS_1_1; }
    const char *processing() const override        { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *) override CLICK_COLD;

    void release_flow(FMPState* fcb) {

    }

    const static int timeout = 15000;

    void push_flow(int port, FMPState* fcb, PacketBatch*);

    inline bool new_flow(FMPState*, Packet*) {
        return true;
    }

    void add_handlers() override CLICK_COLD;

    int _min;

protected:


    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;
};

CLICK_ENDDECLS
#endif
