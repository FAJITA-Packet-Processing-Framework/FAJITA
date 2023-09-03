/**
 * flowipmanagerdpdk.{cc,hh}
 */

#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/ipflowid.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include "flowipmanagerhashanalysis.hh"
#include <rte_hash.h>
#include <click/dpdk_glue.hh>
#include <rte_ethdev.h>
#include <rte_errno.h>
#include <iostream>
#include <random>
#include <set>


CLICK_DECLS

FlowIPManager_HASHANALYSIS::FlowIPManager_HASHANALYSIS() {

}

FlowIPManager_HASHANALYSIS::~FlowIPManager_HASHANALYSIS() {

}

int FlowIPManager_HASHANALYSIS::configure(Vector<String> &conf, ErrorHandler *errh) {
    Args args(conf, this, errh);

    if (parse(&args) || args
                .read_or_set("VERBOSE", _verbose, true)
                .read_or_set("RANDOM_BYTES", _random_bytes, 13)
                .complete())
        return errh->error("Error while parsing arguments!");

    find_children(_verbose);
    router()->get_root_init_future()->postOnce(&_fcb_builded_init_future);
    _fcb_builded_init_future.post(this);

    _reserve += reserve_size();

    return 0;
}

int
FlowIPManager_HASHANALYSIS::alloc(FlowIPManager_HashState & table, int core, ErrorHandler* errh)
{
    struct rte_hash_parameters hash_params = {0};

    unsigned char test[13] = {172,217,21,170,1,187,172,217,21,170,1,187,6};
    uint32_t hash = rte_hash_crc_1byte(1,4294967295);
//    uint32_t hash = ipv4_hash_crc4(test,8,0);
    click_chatter("hex: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x ", 
    test[0], test[1], test[2], test[3], test[4], test[5], test[6], test[7], test[8], test[9], test[10], 
    test[11], test[12] );
    click_chatter("result is : %d", hash);


    char buf[64];
    sprintf(buf, "%i-%s", core, name().c_str());

    hash_params.name = buf;
    hash_params.entries = _capacity;

//    hash_params.key_len = sizeof(IPFlow5ID);
    hash_params.key_len = sizeof(uint32_t);
    hash_params.hash_func = ipv4_hash_crc2;
    hash_params.hash_func_init_val = 0;
    hash_params.extra_flag = 0;

    sprintf(buf, "%d-%s",core ,name().c_str());
    table.hash = rte_hash_create(&hash_params);
    _hash_size = 4194304;
    _current_id_num = 0;
    bzero(table.inplace_array,sizeof(int) * _hash_size);
    if (!table.hash)
        return errh->error("Could not init flow table %d : error %d (%s)!", core, rte_errno, rte_strerror(rte_errno));


    int numBits = 22;
    int numNumbers = 4000000;
    const int maxValue = (1 << numBits) - 1;  // Maximum value for the given number of bits

    std::random_device rd;  // Random device to seed the random number generator
    std::mt19937 gen(rd());  // Mersenne Twister random number engine
    std::uniform_int_distribution<int> dist(0, maxValue);  // Uniform distribution between 0 and maxValue

    std::set<int> generatedNumbers;  // Set to store unique generated numbers

    while (generatedNumbers.size() < numNumbers) {
        int randomNumber = dist(gen);
        generatedNumbers.insert(randomNumber);
    }

    int index = 0;
    for (int randomNumber : generatedNumbers) {
        table._generated_numbers[index++] = randomNumber;
    }

    return 0;
}

void
FlowIPManager_HASHANALYSIS::find_bulk(PacketBatch *batch, int* positions)
{   
    auto *table = 	reinterpret_cast<rte_hash*>(_tables->hash);

    int bitmask = _hash_size - 1;
    int idx = 0;
    int google_ip_port_proto[7] = {172,217,21,170,1,187,6};
    int client_msb_ip[3] = {170,16,10};

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<unsigned char> distribution(0, 255);
    
    FOR_EACH_PACKET(batch, p){
        uint32_t key = AGGREGATE_ANNO(p);
        int ret = rte_hash_lookup(table, &key);
        positions[idx] = ret;
        idx++;
        if (ret <0){
//            IPFlow5ID id = IPFlow5ID(p);
//            uint32_t hash = ipv4_hash_crc3(&id,0,0);
            
            unsigned char randomData[13];
            for (int i = 0; i < 13; ++i) {
                if (i < 3)
                    randomData[i] = client_msb_ip[i];
//                else if (i == 2)
//                    randomData[i] = (_tables->_generated_numbers[_current_id_num] >> 24) & 255; 
                else if (i == 3)
                    randomData[i] = (_tables->_generated_numbers[_current_id_num] >> 16) & 255; 
                else if (i >= 4 && i <= 7 )
                    randomData[i] = google_ip_port_proto[i-4];
                else if (i == 8)
                    randomData[i] = (_tables->_generated_numbers[_current_id_num] >> 8) & 255; 
                else if (i == 9)
                    randomData[i] = _tables->_generated_numbers[_current_id_num] & 255; 
//                else if (i >= 3 && i <= 5)
//                    randomData[i] = distribution(generator);
                else
                    randomData[i] = google_ip_port_proto[i-6];

//                if (i >= i >= _random_bytes)
//                    randomData[i] = 0;
//                else
//                    randomData[i] = distribution(generator);
            }
            uint32_t hash = ipv4_hash_crc4(randomData,13,0);

            int primary = (hash >> 10) & bitmask;
//            int primary = p->anno_u32(20) & bitmask;
            _tables->inplace_array[primary]++;
            _current_id_num++;
        }
    }
}

int
FlowIPManager_HASHANALYSIS::find(IPFlow5ID &f)
{
    auto *table = 	reinterpret_cast<rte_hash*>(_tables->hash);
    
    int ret = rte_hash_lookup(table, &f);

    return ret;
}


int
FlowIPManager_HASHANALYSIS::count()
{
    return 0;
}

int
FlowIPManager_HASHANALYSIS::insert(IPFlow5ID &f, int)
{
    auto *table = reinterpret_cast<rte_hash *> (_tables->hash);
    int ret = rte_hash_add_key(table, &f);

    return ret;
}

int
FlowIPManager_HASHANALYSIS::insert2(Packet *p, int)
{
    auto *table = reinterpret_cast<rte_hash *> (_tables->hash);
    uint32_t key = AGGREGATE_ANNO(p);
    int ret = rte_hash_add_key(table, &key);

    return ret;
}

int
FlowIPManager_HASHANALYSIS::remove(IPFlow5ID &f)
{
    auto *table = reinterpret_cast<rte_hash *> (_tables->hash);
    int ret = rte_hash_del_key(table, &f);
    return ret;
}

void FlowIPManager_HASHANALYSIS::cleanup(CleanupStage stage)
{
}

CLICK_ENDDECLS

ELEMENT_REQUIRES(dpdk dpdk19 cxx17)
EXPORT_ELEMENT(FlowIPManager_HASHANALYSIS)
ELEMENT_MT_SAFE(FlowIPManager_HASHANALYSIS)
