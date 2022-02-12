#ifndef CLICK_FlowIPManagerFuzzy_HH
#define CLICK_FlowIPManagerFuzzy_HH
#include <click/config.h>
#include <click/string.hh>
#include <click/vector.hh>
#include <click/multithread.hh>
#include <click/batchelement.hh>
#include <click/pair.hh>
#include <click/batchbuilder.hh>
#include <click/flow/common.hh>
#include <click/flow/flowelement.hh>
#include <utility>

CLICK_DECLS

/**
 * FCB packet classifier
 *
 * Simply assign a bucket according to the hash of the packet
 */
class FlowIPManagerFuzzy: public VirtualFlowManager, public Router::ChildrenFuture {
public:

    FlowIPManagerFuzzy() CLICK_COLD;
	~FlowIPManagerFuzzy() CLICK_COLD;

    const char *class_name() const		{ return "FlowIPManagerFuzzy"; }
    const char *port_count() const		{ return "1/1"; }

    const char *processing() const		{ return PUSH; }
    int configure_phase() const     { return CONFIGURE_PHASE_PRIVILEGED + 1; }

    int configure(Vector<String> &, ErrorHandler *) override CLICK_COLD;
    int solve_initialize(ErrorHandler *errh) override CLICK_COLD;
    void cleanup(CleanupStage stage) CLICK_COLD;

    void push_batch(int, PacketBatch* batch) override;
    virtual int capacity();
    virtual int count();

    void add_handlers();

    static String read_handler(Element* e, void* thunk);
private:


    struct gtable {
        gtable() {
        }
        FlowControlBlock *fcbs;
    } CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);


    int _table_size;
    int _flow_state_size_full;
    int _verbose;

    gtable* _tables;

    inline void process(int groupid, Packet* p, BatchBuilder& b);

};

CLICK_ENDDECLS
#endif
