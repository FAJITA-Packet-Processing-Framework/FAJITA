/*
 * RTPapi.{cc,hh} -- measures round trip cycles on a push or pull path.
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */
#include <click/config.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/args.hh>
#include "rtpapi.hh"

RTPapi::RTPapi() : _state()
{
}

RTPapi::~RTPapi()
{
}

int
RTPapi::configure(Vector<String> &conf, ErrorHandler *errh) {


    if (Args(conf, this, errh)
        .read_all("EVENT", _events)
        .complete() < 0)
        return -1;
      if (_events.size() == 0)
        _events.push_back("PAPI_TOT_CYC");

    return 0;
}

int
RTPapi::initialize(ErrorHandler* errh) {

  int retval = 0;
    click_chatter("Initializing PAPI...");
    retval=PAPI_library_init(PAPI_VER_CURRENT);
    if (retval!=PAPI_VER_CURRENT) {
            fprintf(stderr,"Error initializing PAPI! %s\n",
                    PAPI_strerror(retval));
            return 0;
    }

    retval = PAPI_thread_init(pthread_self);
    if (retval != PAPI_OK)
      return errh->error("Could not init PAPI thread: %s",PAPI_strerror(retval));

    eventset = PAPI_NULL ;

  retval=PAPI_create_eventset(&eventset);
  if (retval!=PAPI_OK) {
    fprintf(stderr,"Error creating eventset! %s\n",
          PAPI_strerror(retval));
  }

  for (int i =0; i < _events.size(); i++) {
    String event = _events[i];
    retval=PAPI_add_named_event(eventset,event.c_str());
    if (retval!=PAPI_OK) {
      fprintf(stderr,"Error adding %s: %s\n", event.c_str(),
        PAPI_strerror(retval));
    }
  }
  return retval;

}

void
RTPapi::start_count() {
  int retval;
  PAPI_reset(eventset);
  retval=PAPI_start(eventset);
  if (retval!=PAPI_OK) {
    fprintf(stderr,"Error starting CUDA: %s\n",
      PAPI_strerror(retval));
  }
}

click_cycles_t
RTPapi::stop_count() {
  int retval;
  long long int count;
  retval=PAPI_stop(eventset,&count);
  if (retval!=PAPI_OK) {
    fprintf(stderr,"Error stopping:  %s\n",
                          PAPI_strerror(retval));
  }
  else {
       return count;
  }
}

void
RTPapi::push(int, Packet *p)
{
  start_count();
  output(0).push(p);
  _state->accum += stop_count();
  _state->npackets++;
}

Packet *
RTPapi::pull(int)
{
  start_count();
  Packet *p = input(0).pull();
  _state->accum += stop_count();
  if (p) _state->npackets++;
  return(p);
}

#if HAVE_BATCH
void
RTPapi::push_batch(int, PacketBatch *p)
{
  start_count();
  int count = p->count();
  output(0).push_batch(p);
  _state->accum += stop_count();
  _state->npackets += count;
}

PacketBatch *
RTPapi::pull_batch(int,unsigned max)
{
  start_count();
  PacketBatch *p = input(0).pull_batch(max);
  _state->accum += stop_count();
  if (p) _state->npackets += p->count();
      return(p);
}
#endif

void
RTPapi::cleanup(CleanupStage) {
         PAPI_cleanup_eventset(eventset);
        PAPI_destroy_eventset(&eventset);
}

String
RTPapi::read_handler(Element *e, void *thunk)
{
	RTPapi *cca = static_cast<RTPapi *>(e);
    switch ((uintptr_t)thunk) {
      case 0: {
	  PER_THREAD_MEMBER_SUM(uint64_t,count,cca->_state,npackets);
	  return String(count); }
      case 1: {
	  PER_THREAD_MEMBER_SUM(uint64_t,accum,cca->_state,accum);
	  return String(accum); }
      default:
	  return String();
    }
}

int
RTPapi::reset_handler(const String &, Element *e, void *, ErrorHandler *)
{
	RTPapi *cca = static_cast<RTPapi *>(e);
    PER_THREAD_MEMBER_SET(cca->_state,accum,0);
    PER_THREAD_MEMBER_SET(cca->_state,npackets,0);
    return 0;
}

void
RTPapi::add_handlers()
{
    add_read_handler("packets", read_handler, 0);
    add_read_handler("count", read_handler, 0);
    add_read_handler("cycles", read_handler, 1);
    add_write_handler("reset_counts", reset_handler, 0, Handler::BUTTON);
}


ELEMENT_REQUIRES(int64 papi)
EXPORT_ELEMENT(RTPapi)
