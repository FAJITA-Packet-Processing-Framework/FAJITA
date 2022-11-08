#ifndef CLICK_BATCHMERGE_HH
#define CLICK_BATCHMERGE_HH

#include <click/config.h>
#include <click/batchelement.hh>
#include <click/glue.hh>
#include <click/gaprate.hh>
#include <click/packet.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>


CLICK_DECLS

/**
 * =c
 * Shifter(IP_OFFSET, PORT_OFFSET, I<keywords>)
 *
 * 
 * =s  nat
 * 
 * Shifts IP and ports.
 *
 * =d
 * Shifts IPs and ports by the given offset, modulo the size of the field.
 * Accepts TCP and UDP packets encapsulated in IPv4.
 * Other L4 packets will be emitted untouched.
 * 
 * Keywords:
 * =item IP_OFFSET
 * Shifting for the source IP
 * =item PORT_OFFSET
 * Shifting for the source port
 * =item IP_OFFSET_DST
 * Shifting for the destination IP
 * =item PORT_OFFSET_DST
 * Shifting for the destination port
 * 
 * =e
 * 
 * FromDump(trace.pcap)
 * -> Strip(14) -> CheckIPHeader(CHECKSUM false)
 * -> Shifter(1,1,1,1)
 * -> CheckIPHeader(CHECKSUM false) 
 * -> ToDump(trace_1.pcap)
 * 
 **/
class BatchMerge : public BatchElement {
  public:
    BatchMerge() CLICK_COLD;
    ~BatchMerge() CLICK_COLD;

    const char *class_name() const override { return "BatchMerge"; }
    const char *port_count() const override { return "1/1"; }
    const char *processing() const override { return PUSH; }
    int configure(Vector<String> &, ErrorHandler *) override CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    inline Packet *simple_action(Packet *p) override;

    void add_handlers() override CLICK_COLD;
    static String read_handler(Element* e, void* thunk);

#if HAVE_BATCH
    inline void push_batch(int, PacketBatch *batch) override;
#endif

  private:
    PacketBatch* process(PacketBatch *batch);

    unsigned _len;
    uint32_t _max_packet_len;

    uint32_t _input_count;
    uint32_t _output_count;

};

CLICK_ENDDECLS
#endif
