#ifndef RTPapi_HH
#define RTPapi_HH
#include <click/batchelement.hh>
#include <papi.h>

/*
 * =c
 * RoundTripCycleCount()
 *
 * =s counters
 * measures round trip cycles on a push or pull path
 *
 * =d
 *
 * Measures the number of CPU cycles it takes for a push or pull to come back
 * to the element. This is a good indication of how much CPU is spent on the
 * Click path after or before this element.
 *
 * =h packets read-only
 * Returns the number of packets that have passed.
 *
 * =h cycles read-only
 * Returns the accumulated round-trip cycles for all passing packets.
 *
 * =h reset_counts write-only
 * Resets C<packets> and C<cycles> counters to zero when written.
 *
 * =a SetCycleCount, CycleCountAccum, SetPerfCount, PerfCountAccum
 */

class RTPapi : public BatchElement { public:

  RTPapi() CLICK_COLD;
  ~RTPapi() CLICK_COLD;

  const char *class_name() const override	{ return "RoundTripPapiCount"; }
  const char *port_count() const override	{ return PORTS_1_1; }

  int configure(Vector<String> &conf, ErrorHandler *errh);
  int initialize(ErrorHandler* errh);
  void cleanup(CleanupStage);

  void push(int, Packet *p);
  Packet *pull(int);
#if HAVE_BATCH
  void push_batch(int, PacketBatch *b) override;
  PacketBatch *pull_batch(int,unsigned) override;
#endif
  void add_handlers() CLICK_COLD;

  struct state {
	  state() : accum(0), npackets(0) {

	  }
    click_cycles_t accum;
    uint64_t npackets;

  };
  per_thread<state> _state;
    Vector<String> _events;
  int eventset;




void start_count();
click_cycles_t stop_count();



  static String read_handler(Element *, void *) CLICK_COLD;
  static int reset_handler(const String &, Element*, void*, ErrorHandler*);

};

#endif
