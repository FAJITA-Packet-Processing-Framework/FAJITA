#ifndef CLICK_FLOWIPMANAGERHASHANALYSIS_HH
#define CLICK_FLOWIPMANAGERHASHANALYSIS_HH
#include <click/config.h>
#include <click/string.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include <click/multithread.hh>
#include <click/pair.hh>
#include <click/flow/common.hh>
#include <click/flow/virtualflowmanager.hh>
#include <click/batchbuilder.hh>
#include <click/timerwheel.hh>

CLICK_DECLS

class DPDKDevice;
struct rte_hash;

class FlowIPManager_HashState: public FlowManagerIMPStateNoFID { public:
    void *hash = 0;
    int inplace_array [4194304];
    int _generated_numbers [4194304];
} CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);

/**
 * =c
 * FlowIPManager_DPDK(CAPACITY [, RESERVE])
 *
 * =s flow
 *  FCB packet classifier - cuckoo per-thread
 *
 * =d
 *
 * Initialize the FCB stack for every packets passing by.
 * The classification is done using a per-core cuckoo hash table.
 *
 * This element does not find automatically the FCB layout for FlowElement,
 * neither set the offsets for placement in the FCB automatically. Look at
 * the middleclick branch for alternatives.
 *
 * =a FlowIPManger
 *
 */
class FlowIPManager_HASHANALYSIS: public VirtualFlowManagerIMP<FlowIPManager_HASHANALYSIS, FlowIPManager_HashState>
{
    public:
        FlowIPManager_HASHANALYSIS() CLICK_COLD;
        ~FlowIPManager_HASHANALYSIS() CLICK_COLD;

        const char *class_name() const override { return "FlowIPManager_HASHANALYSIS"; }
        const char *port_count() const override { return "1/1"; }

        const char *processing() const override { return PUSH; }

        int configure(Vector<String> &, ErrorHandler *) override CLICK_COLD;
        void cleanup(CleanupStage stage) override CLICK_COLD;

    protected:

    int _hash_size;
    int _random_bytes;
    int _current_id_num;

    //Implemented for VirtualFlowManagerIMP. It is using CRTP so no override.
    inline int alloc(FlowIPManager_HashState& table, int core, ErrorHandler* errh);
    inline void find_bulk(PacketBatch *batch, int32_t* positions);
	inline int find(IPFlow5ID &f);
	inline int insert(IPFlow5ID &f, int);
	inline int insert2(Packet *p, int);
    inline int remove(IPFlow5ID &f);
    inline int count();


    enum {
        h_dump,
        h_count,
        h_conflict
    };

    static String read_handler(Element *e, void *thunk) {
        FlowIPManager_HASHANALYSIS *f = static_cast<FlowIPManager_HASHANALYSIS *>(e);

        intptr_t cnt = (intptr_t)thunk;
        switch (cnt) {
        case h_dump: {
            StringAccum s;
            for(int i=0; i<256; i++){
                int a = f->_tables.get_value(0).inplace_array[i];
                s << i << ", " << a << "\n ";
            }
            return s.take_string();
        }
        case h_count: {
            int sum = 0;
            for(int i=0; i<f->_hash_size; i++){
                sum+=f->_tables.get_value(0).inplace_array[i];
            }
            return String(sum);
        }
        case h_conflict: {
            int* conf = new int[6];
            for (int i = 0; i< 6; i++){
                conf[i] = 0;
            } 
            for(int i=0; i<f->_hash_size; i++){
                int num =f->_tables.get_value(0).inplace_array[i];
                if (num > 4) num = 5;
                conf[num]++;
            }
            StringAccum s;
            s << "\n";
            for(int i=0; i<6; i++){
                s << i << " " << conf[i] << "\n ";
            }
            return s.take_string();

        }
        default:
            return "<error>";
        }
    }

    void add_handlers() {
        add_read_handler("dump", read_handler, h_dump);
        add_read_handler("count", read_handler, h_count);
        add_read_handler("conflict", read_handler, h_conflict);
    }

    friend class VirtualFlowManagerIMP;
};

CLICK_ENDDECLS

#endif

