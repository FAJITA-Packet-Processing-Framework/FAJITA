#ifndef CLICK_SOURCECOUNTER_HH
#define CLICK_SOURCECOUNTER_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/multithread.hh>
#include <click/flow/flowelement.hh>
#include <click/atomic.hh>
#include <rte_hash.h>

CLICK_DECLS


/*
=c

FlowCounter([CLOSECONNECTION])

=s flow

Counts all flows passing by, the number of active flows, and the number of 
packets per flow.

 */
// alignas(CLICK_CACHE_LINE_SIZE)
struct SourceCounterState {
    atomic_uint32_t count;
//    void* pointer[8]; // extra useless cache line just to add enough space
};

class SourceCounter : public FlowStateElement<SourceCounter,int>
{
public:
    /** @brief Construct an FlowCounter element
     */
    SourceCounter() CLICK_COLD;

    const char *class_name() const override        { return "SourceCounter"; }
    const char *port_count() const override        { return PORTS_1_1; }
    const char *processing() const override        { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *) override CLICK_COLD;

    void release_flow(int* fcb) {
    }

    const static int timeout = 15000;

    void push_flow(int port, int* fcb, PacketBatch*);

#if FLOW_PUSH_BATCH
    inline void push_flow_batch(int port, int** fcb, PacketBatch *head);
#endif

    inline bool new_flow(int* state, Packet* p) {

        if (!_cache)
            return true;

        auto *table = reinterpret_cast<rte_hash *> (_table);
        local_flowID* ifid = (local_flowID*) (p->data() + _offset);
        if (_verbose)
            _insertions++;
        int fcb_idx = rte_hash_add_key(table, ifid);
        if (unlikely(fcb_idx < 0)){
            click_chatter("Problem with inserting data! %d", fcb_idx);
            return false;
        }
           
        *state = fcb_idx;
        return true;
    }

    void add_handlers() override CLICK_COLD;
protected:

    struct local_flowID {
        uint32_t ip_src;
    };

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

    uint32_t _capacity;
    uint8_t _is_src;
    uint8_t _cache;
    uint32_t _offset;
    SourceCounterState *_local_fcbs_struct;
    atomic_uint32_t *_local_fcbs;
    atomic_uint32_t _insertions;
    uint32_t _verbose;

    void *_table;
};

CLICK_ENDDECLS
#endif
