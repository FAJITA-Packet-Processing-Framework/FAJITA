/**
 * flowipmanagerdpdk.{cc,hh}
 */

#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/ipflowid.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include "flowipmanagerdpdk.hh"
#include <rte_hash.h>
#include <click/dpdk_glue.hh>
#include <rte_ethdev.h>
#include <rte_errno.h>

CLICK_DECLS

FlowIPManager_DPDK::FlowIPManager_DPDK() {

}

FlowIPManager_DPDK::~FlowIPManager_DPDK() {

}

int FlowIPManager_DPDK::configure(Vector<String> &conf, ErrorHandler *errh) {
    Args args(conf, this, errh);

    if (parse(&args) || args
                .read_or_set("VERBOSE", _verbose, false)
                .complete())
        return errh->error("Error while parsing arguments!");

    find_children(_verbose);
    router()->get_root_init_future()->postOnce(&_fcb_builded_init_future);
    _fcb_builded_init_future.post(this);

    _reserve += reserve_size();

    return 0;
}

int
FlowIPManager_DPDK::alloc(FlowIPManager_DPDKState & table, int core, ErrorHandler* errh)
{
    struct rte_hash_parameters hash_params = {0};

    char buf[64];
    sprintf(buf, "%i-%s", core, name().c_str());

    hash_params.name = buf;
    hash_params.entries = _capacity;

    hash_params.key_len = sizeof(IPFlow5ID);
    hash_params.hash_func = ipv4_hash_crc;
    hash_params.hash_func_init_val = 0;
    hash_params.extra_flag = 0x20;

    sprintf(buf, "%d-%s",core ,name().c_str());
    table.hash = rte_hash_create(&hash_params);
    if (!table.hash)
        return errh->error("Could not init flow table %d : error %d (%s)!", core, rte_errno, rte_strerror(rte_errno));
    return 0;
}

void
FlowIPManager_DPDK::find_bulk(PacketBatch *batch, int* positions)
{   
    int index = 0;

    FOR_EACH_PACKET(batch, p){
        _tables->flowIDs[index] = IPFlow5ID(p);
        _tables->key_array[index] = &(_tables->flowIDs[index]);
        index++;
    }

    auto *table = reinterpret_cast<rte_hash*>(_tables->hash);
    rte_hash_lookup_bulk(table, const_cast<const void **>(&(_tables->key_array[0])), index, positions);
}

int
FlowIPManager_DPDK::find(IPFlow5ID &f)
{
    auto *table = 	reinterpret_cast<rte_hash*>(_tables->hash);
    
    int ret = rte_hash_lookup(table, &f);

    return ret;
}


int
FlowIPManager_DPDK::count()
{
    int total = 0;
    for (int i = 0; i < _tables.weight(); i++) {
        auto *table = reinterpret_cast<rte_hash*>(_tables.get_value(i).hash);
        total += rte_hash_count(table);
    }

    return total;
}

int
FlowIPManager_DPDK::insert(IPFlow5ID &f, int)
{
    auto *table = reinterpret_cast<rte_hash *> (_tables->hash);
    int ret = rte_hash_add_key(table, &f);

    return ret;
}

int
FlowIPManager_DPDK::remove(IPFlow5ID &f)
{
    auto *table = reinterpret_cast<rte_hash *> (_tables->hash);
    int ret = rte_hash_del_key(table, &f);
    return ret;
}

void FlowIPManager_DPDK::cleanup(CleanupStage stage)
{
}

CLICK_ENDDECLS

ELEMENT_REQUIRES(dpdk dpdk19 cxx17)
EXPORT_ELEMENT(FlowIPManager_DPDK)
ELEMENT_MT_SAFE(FlowIPManager_DPDK)
