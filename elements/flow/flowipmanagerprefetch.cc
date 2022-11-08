/*
 * flowipmanagerimp.{cc,hh}
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/ipflowid.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include "flowipmanagerprefetch.hh"
#include <rte_hash.h>
#include <click/dpdk_glue.hh>
#include <rte_ethdev.h>
#include <rte_errno.h>
#include <clicknet/ether.h>

CLICK_DECLS

FlowIPManagerPrefetch::FlowIPManagerPrefetch() : _verbose(1), _flags(0), _timer(this), _task(this), _tables(0), _cache(true), Router::ChildrenFuture(this) {
}

FlowIPManagerPrefetch::~FlowIPManagerPrefetch()
{
}

int
FlowIPManagerPrefetch::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
        .CLICK_NEVER_REPLACE(read_or_set_p)("CAPACITY", _table_size, 65536)
        .CLICK_NEVER_REPLACE(read_or_set)("RESERVE", _reserve, 0)
        .read_or_set("TIMEOUT", _timeout, -1)
        .read_or_set("CACHE", _cache, true)
        .read_or_set("VERBOSE", _verbose, true)
	.read_or_set("PREFETCH", _prefetch, 0)
	.read_or_set("PREFETCH_SEC", _prefetch_secondary, 0)
	.read_or_set("PREFETCH_LVL", _prefetch_lvl, 0)
	.read_or_set("MAXSEQINDEX", _maxIndex, 32)
	.read_or_set("BULK_SEARCH", _bulk_search, false)
	.read_or_set("PREFETCH_BATCH", _prefetch_batch, false)
        .complete() < 0)
        return -1;

    if (_timeout > 0) {
        return errh->error("Timeout unsupported!");
    }

    errh->warning("This element does not support timeout");

    find_children(_verbose);

    router()->get_root_init_future()->postOnce(&_fcb_builded_init_future);

    _fcb_builded_init_future.post(this);

    _reserve += sizeof(uint32_t);
    _currentIndex = 0;

    return 0;
}

int FlowIPManagerPrefetch::solve_initialize(ErrorHandler *errh)
{
    struct rte_hash_parameters hash_params = {0};
    char buf[64];
    hash_params.name = buf;
    auto passing = get_passing_threads();
    _tables_count = passing.size();
    _table_size = next_pow2(_table_size/passing.weight());
    click_chatter("Real capacity for each table will be %d", _table_size);
    hash_params.entries = _table_size;
    hash_params.key_len = sizeof(uint32_t);
    hash_params.hash_func = ipv4_hash_crc_tagged;
    hash_params.hash_func_init_val = 0;
    hash_params.extra_flag = _flags;

    _flow_state_size_full = sizeof(FlowControlBlock) + _reserve;

    _tables = CLICK_ALIGNED_NEW(gtable, _tables_count);
    CLICK_ASSERT_ALIGNED(_tables);

    for (int i = 0; i < _tables_count; i++) {
        if (!passing[i])
            continue;
        sprintf(buf, "%d-%s",i,name().c_str());
        _tables[i].hash = rte_hash_create(&hash_params);
        if (!_tables[i].hash)
            return errh->error("Could not init flow table %d : error %d (%s)!", i, rte_errno, rte_strerror(rte_errno));

        _tables[i].fcbs =  (FlowControlBlock*)CLICK_ALIGNED_ALLOC(_flow_state_size_full * _table_size);
        CLICK_ASSERT_ALIGNED(_tables[i].fcbs);
        bzero(_tables[i].fcbs,_flow_state_size_full * _table_size);
        if (!_tables[i].fcbs)
            return errh->error("Could not init data table %d!", i);
    }

    for (int i=0; i<32; i++){
        _prefetched_keys[i]=0;
    }
    _currentIndex=0;

    if (_timeout > 0) {
        _timer_wheel.initialize(_timeout);
    }

    /*
    _timer.initialize(this);
    _timer.schedule_after(Timestamp::make_sec(1));*/
    _task.initialize(this, false);

    return Router::InitFuture::solve_initialize(errh);
}


bool FlowIPManagerPrefetch::run_task(Task* t)
{
    /*
     Not working : the timerwheel must be per-thread too
     Timestamp recent = Timestamp::recent_steady();
    _timer_wheel.run_timers([this,recent](FlowControlBlock* prev) -> FlowControlBlock*{
        FlowControlBlock* next = *((FlowControlBlock**)&prev->data_32[2]);
        int old = (recent - prev->lastseen).sec();
        if (old > _timeout) {
            //click_chatter("Release %p as it is expired since %d", prev, old);
            //expire
            rte_hash_free_key_with_position(vhash[click_current_cpu_id()], prev->data_32[0]);//depreciated
        } else {
            //click_chatter("Cascade %p", prev);
            //No need for lock as we'll be the only one to enqueue there
            _timer_wheel.schedule_after(prev, _timeout - (recent - prev->lastseen).sec(),setter);
        }
        return next;
    });
    return true;*/
    return false;
}

void FlowIPManagerPrefetch::run_timer(Timer* t)
{
    //_task.reschedule();
   // t->reschedule_after(Timestamp::make_sec(1));
}

void FlowIPManagerPrefetch::cleanup(CleanupStage stage)
{
    click_chatter("Cleanup the table");
    if (_tables) {
        for(int i =0; i<click_max_cpu_ids(); i++) {
           if (_tables[i].hash)
               rte_hash_free(_tables[i].hash);

           if (_tables[i].fcbs)
                delete _tables[i].fcbs;
        }

        delete _tables;
    }
}

void FlowIPManagerPrefetch::process(Packet* p, BatchBuilder& b, const Timestamp& recent, int bulk_ret)
{
//    IPFlow5ID fid = IPFlow5ID(p);
//    const click_ether *ethh = (const click_ether *)p->data();
//    uint32_t key = *((uint32_t*)(ethh->ether_shost + 1));
    uint32_t key = AGGREGATE_ANNO(p);
//    printf("key is: %d \n", key);
//    if (_cache && fid == b.last_id) {
//        b.append(p);
//        return;
//    }
    auto& tab = _tables[click_current_cpu_id()];
    rte_hash* table = tab.hash;

    FlowControlBlock* fcb;
    

    if (_prefetch > 0) {
//        uint32_t predicted_key = *((uint32_t*)(ethh->ether_dhost + 1));
//	void * bucket = rte_hash_prefetch(table, &predicted_key, _prefetch_secondary, _prefetch_lvl);
//	_prefetched_keys[(_currentIndex+(_maxIndex-1))%_maxIndex] = bucket;

//	if(_prefetched_keys[_currentIndex] != 0){
//            rte_hash_prefetch_bucket(table, _prefetched_keys[_currentIndex]);
//	}
//        printf("prefetched key: %p , prefetched bucket: %p \n", bucket, _prefetched_keys[_currentIndex]);
//	if(_prefetched_keys[_currentIndex] != 0){
//            int idx = rte_hash_lookup(table, &(_prefetched_keys[_currentIndex]));
//	    if (idx>0)
        
//	int related_ret = predicted_key%_table_size;
//        rte_prefetch0( (FlowControlBlock*)((unsigned char*)tab.fcbs + (_flow_state_size_full * related_ret)) );
//	}
//	_currentIndex = (_currentIndex+1)%_maxIndex;
    }
//    int ret = key%_table_size;
//    fcb = (FlowControlBlock*)((unsigned char*)tab.fcbs + (_flow_state_size_full * ret));

    
    int ret;
    
    if(bulk_ret != 0)
        ret = bulk_ret;
    else {
        ret = rte_hash_lookup(table, &key);
    }

//    _currentIndex = (_currentIndex + 61) % _maxIndex;
//    int ret = _currentIndex;
//    int ret = click_random(1, _maxIndex);
    if (ret < 0) { //new flow
        ret = rte_hash_add_key(table, &key);
        if (unlikely(ret < 0)) {
            if (unlikely(_verbose > 0)) {
                click_chatter("Cannot add key (have %d items. Error %d)!", rte_hash_count(table), ret);
            }
            p->kill();
            return;
        }
//        fcb = (FlowControlBlock*)((unsigned char*)tab.fcbs + (_flow_state_size_full * ret));
        //We remember the index in the first 4 reserved bytes
//        fcb->data_32[0] = ret;
        if (_timeout > 0) {
            if (_flags) {
                _timer_wheel.schedule_after_mp(fcb, _timeout, fim_prefetch_setter);
            } else {
                _timer_wheel.schedule_after(fcb, _timeout, fim_prefetch_setter);
            }
        }
    } else { //existing flow
//        fcb = (FlowControlBlock*)((unsigned char*)tab.fcbs + (_flow_state_size_full * ret));
    }
    fcb = (FlowControlBlock*)((unsigned char*)tab.fcbs + (_flow_state_size_full));

//    if (_prefetch > 0){
//	int predicted = ((_prefetch*51) + _currentIndex) % _maxIndex;
//        FlowControlBlock* predictedFCB = (FlowControlBlock*)((unsigned char*)tab.fcbs + (_flow_state_size_full * predicted));
//	rte_prefetch0(predictedFCB);
//    }

    if (b.last == ret) {
        b.append(p);
    } else {
        PacketBatch* batch;
        batch = b.finish();
        if (batch) {
            fcb_stack->lastseen = recent;
#if HAVE_FLOW_DYNAMIC
	    fcb_stack->acquire(batch->count());
#endif
            output_push_batch(0, batch);
        }
        fcb_stack = fcb;
        b.init();
        b.append(p);
        b.last = ret;
//        if (_cache)
//            b.last_id = fid;
    }

}

void FlowIPManagerPrefetch::push_batch(int, PacketBatch* batch)
{
    BatchBuilder b;
    Timestamp recent = Timestamp::recent_steady();
    
    if (_prefetch_batch){
        auto& tab = _tables[click_current_cpu_id()];
        rte_hash* table = tab.hash;
        FOR_EACH_PACKET(batch, pt) {
            const click_ether *ethh = (const click_ether *)pt->data();
            //uint32_t key = *((uint32_t*)(ethh->ether_shost + 1));
	    uint32_t key = AGGREGATE_ANNO(pt);
	    rte_hash_prefetch(table, &key, _prefetch_secondary, _prefetch_lvl);
//	rte_prefetch0( (FlowControlBlock*)((unsigned char*)tab.fcbs + (_flow_state_size_full * ret)) );
  
        }
    }

    if(_bulk_search){

	int32_t ret[batch->count()];
        const void *key_array[batch->count()];
	const uint32_t *int_array[batch->count()];

        int index = 0;
        FOR_EACH_PACKET(batch, p){
            const click_ether *ethh = (const click_ether *)p->data();
	    // uint32_t* key = (uint32_t*)(ethh->ether_shost + 1);
            const uint32_t* key = p->anno_u32_ptr(20);
	    int_array[index] = key;    
	    key_array[index] = key;
	    index++;
        }

	auto& tab = _tables[click_current_cpu_id()];
	rte_hash* table = tab.hash;
	rte_hash_lookup_bulk(table, &key_array[0], batch->count(), ret);

	index = 0;
	FOR_EACH_PACKET_SAFE(batch, pt) {
            process(pt, b, recent, ret[index]);
	    index++;
	}
    }

    else {
        FOR_EACH_PACKET_SAFE(batch, p) {
            process(p, b, recent, 0);
        }
    }

    batch = b.finish();
    if (batch) {
        fcb_stack->lastseen = recent;
#if HAVE_FLOW_DYNAMIC
	fcb_stack->acquire(batch->count());
#endif
        output_push_batch(0, batch);
    }

    fcb_stack = 0;
}

enum {h_count, h_secondary};
String FlowIPManagerPrefetch::read_handler(Element* e, void* thunk)
{
    FlowIPManagerPrefetch* fc = static_cast<FlowIPManagerPrefetch*>(e);
    switch ((intptr_t)thunk) {
    case h_count:
    {
        int count = 0;
        for(int i=0; i< fc->_tables_count; i++)
        {
        gtable * t = (fc->_tables)+i;
        rte_hash* table = t->hash;
        if(table)
            count+=rte_hash_count(table);
        }
        return String(count);
    }
    case h_secondary:
    {
	int sec = 0;
	for(int i=0; i< fc->_tables_count; i++)
	{
	gtable * t = (fc->_tables)+i;
	rte_hash* table = t->hash;
	if(table)
	    sec+=rte_hash_secondary_count(table);
	}
	return String(sec);
    }
    default:
        return "<error>";
    }
};

void FlowIPManagerPrefetch::add_handlers()
{
    add_read_handler("count", read_handler, h_count);
    add_read_handler("secondary_count", read_handler, h_secondary);
}

int FlowIPManagerPrefetch::capacity()
{
    return 0;
}

int FlowIPManagerPrefetch::count()
{
    return 0;
}

CLICK_ENDDECLS

ELEMENT_REQUIRES(dpdk dpdk19)
EXPORT_ELEMENT(FlowIPManagerPrefetch)
ELEMENT_MT_SAFE(FlowIPManagerPrefetch)
