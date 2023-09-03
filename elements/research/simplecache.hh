#ifndef CLICK_SIMPLECACHE_HH
#define CLICK_SIMPLECACHE_HH
#include <click/batchelement.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

EtherRewrite(SRC, DST)

=s ethernet

rewrite source and destination Ethernet address

=d

Rewrite the source and the destination address in the Ethernet header.
The ETHERTYPE is left untouched.

=e

Ensure that source address of all packets passing by is 1:1:1:1:1:1, and
the destination address is 2:2:2:2:2:2:

  EtherRewrite(1:1:1:1:1:1, 2:2:2:2:2:2)

=n

This element is useful if there is only one possible nexthop on a given link
(such as for a network-layer transparent firewall), meaning that source and
destination ethernet address will always be the same for a given output.

=h src read/write

Return or set the SRC parameter.

=h dst read/write

Return or set the DST parameter.

=a

EtherEncap, EtherVLANEncap, ARPQuerier, EnsureEther, StoreEtherAddress */


class SimpleCache : public BatchElement { public:

    SimpleCache() CLICK_COLD;
    ~SimpleCache() CLICK_COLD;

    const char *class_name() const override	{ return "SimpleCache"; }
    const char *port_count() const override	{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;
    static String read_handler(Element *e, void *thunk) CLICK_COLD;

    inline Packet *smaction(Packet *);
    void push(int, Packet *) override;

#if HAVE_BATCH
    void push_batch(int, PacketBatch *) override;
#endif

  private:
    #define SETS_NUM 2

    struct Row {
      uint32_t offset[SETS_NUM];
      uint32_t value[SETS_NUM];
      uint32_t replacement_candidate;
    };

    Row *_rows;
    uint32_t _rows_number;
    uint32_t _row_mask;

    uint32_t _successful_searches;
    uint32_t _failed_searches;

};

CLICK_ENDDECLS
#endif
