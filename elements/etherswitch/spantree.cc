/*
 * spantree.{cc,hh} -- element implementing IEEE 802.1d protocol for
 * Ethernet bridges
 * John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "spantree.hh"
#include "confparse.hh"
#include "elements/standard/suppressor.hh"
#include "error.hh"

EtherSpanTree::EtherSpanTree()
  : _input_sup(0), _output_sup(0), _topology_change(0),
    _bridge_priority(0xdead),	// Make it very unlikely to be root
    _long_cache_timeout(5*60),
    _hello_timer(hello_hook, (unsigned long)this)
{
}

EtherSpanTree::~EtherSpanTree()
{
}

EtherSpanTree*
EtherSpanTree::clone() const
{
  return new EtherSpanTree;
}

void
EtherSpanTree::notify_ninputs(int n) {
  set_ninputs(n);
  // the rest is redundant with notify_noutputs below
  _port.resize(n);
}

void
EtherSpanTree::notify_noutputs(int n) {
  set_noutputs(n);
  // the rest is redundant with notify_ninputs above
  _port.resize(n);
}

int
EtherSpanTree::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  Element* in;
  Element* out;
  Element* sw;

  if (cp_va_parse(conf, this, errh,
		  cpEthernetAddress, "bridge address", _addr,
		  cpElement, "Suppressor to control input", &in,
		  cpElement, "Suppressor to control output", &out,
		  cpElement, "EtherSwitch", &sw,
		  cpEnd) < 0)
    return -1;
  
  if (!in || !in->cast("Suppressor"))
    return errh->error("EtherSpanTree needs an input Suppressor");
  if (!out || !out->cast("Suppressor"))
    return errh->error("EtherSpanTree needs an output Suppressor");
  if (!sw || !sw->cast("EtherSwitch"))
    return errh->error("EtherSpanTree needs an EtherSwitch");
  
  _input_sup = (Suppressor*)in;
  _output_sup = (Suppressor*)out;
  _switch = (EtherSwitch*)sw;
  memcpy(&_bridge_id, _addr, 6);
  return 0;
}

int
EtherSpanTree::initialize(ErrorHandler *)
{
  for (int i = 0; i < _port.size(); i++) {
    set_state(i, FORWARD);
  }
  _best.reset(((u_int64_t)_bridge_priority << 48) | _bridge_id);
  _hello_timer.attach(this);
  _hello_timer.schedule_after_ms(_best._hello_time * 1000);
  return 0;
}

void
EtherSpanTree::uninitialize()
{
  _hello_timer.unschedule();
}

String
EtherSpanTree::read_msgs(Element* f, void *) {
  EtherSpanTree* sw = (EtherSpanTree*)f;
  String s;
  for (int i = 0; i < sw->_port.size(); i++) {
    s += sw->_port[i].msg.s() + "\n";
  }
  s += sw->_best.s() + "\n";
  return s;
}

void
EtherSpanTree::add_handlers()
{
  add_read_handler("msgs", read_msgs, 0);
}

void
EtherSpanTree::periodic() {
  // Push LISTEN and LEARN ports forward.
  timeval now;
  timeval cutoff;
  click_gettimeofday(&now);
  cutoff = now;
  cutoff.tv_sec -= _best._forward_delay;

  for (int i = 0; i < _port.size(); i++) {
    if (_port[i].state == LISTEN || _port[i].state == LEARN)
      if (timercmp(&_port[i].since, &cutoff, <)) {
	set_state(i, FORWARD);
      }
  }

  expire();
  find_tree();
}

bool
EtherSpanTree::expire() {
  timeval t;
  click_gettimeofday(&t);
  t.tv_sec -= _best._max_age;

  bool expired = false;
  for (int i = 0; i < _port.size(); i++) {
    if (_port[i].msg.expire(&t)) {
      expired = true;
      click_chatter("Expiring message on port %d", i);
    }
  }
  return expired;
}


void
EtherSpanTree::find_tree() {
  // First, determine _best, which will either be the bridge's own
  // message or the best message received on one of its ports.
  int root_port = -1;
  u_int64_t my_id = ((u_int64_t)_bridge_priority << 48) | _bridge_id;
  _best.reset(my_id);
  for (int i = 0; i < _port.size(); i++) {
    // Temporarily inc cost
    _port[i].msg._cost++;	// Make configurable JJJ
    if (_best.compare(&_port[i].msg) < 0) {
      _best = _port[i].msg;
      root_port = i;
    }
    _port[i].msg._cost--;	// Make configurable JJJ
  }



  for (int i = 0; i < _port.size(); i++) {
    // The tree includes the port on which _best came in, and ports on
    // which we are the designated bridge.
    if (i == root_port) {
      if (_port[i].state == BLOCK)
	set_state(i, FORWARD);
      continue;
    }

    // We are the designated bridge on a given port if we could spit
    // out a message on that port that is better than the one we have
    // received.
    int cmp = _best.compare(&_port[i].msg, my_id, i);
    if (cmp < 0) // (_best.compare(&_port[i].msg, i) < 0)
      set_state(i, BLOCK);
    else if (_port[i].state == BLOCK)
      set_state(i, FORWARD);
  }
}


bool
EtherSpanTree::set_state(int i, PortState state)
{
  assert(state != LISTEN);
  assert(state != LEARN);

  if (_port[i].state == state)
    return false;

  if (state == FORWARD) {
    if (_port[i].state == BLOCK) {
      click_chatter("Setting send_tc_msg: BLOCK -> FORWARD on %d", i);
      _send_tc_msg = true;
    }
    // Can't go there directly, just increment
    state = (PortState)(_port[i].state+1);
  } else {
    // Since we already checked that _port[i].state != state, this
    // must be a topology change.
    click_chatter("Setting send_tc_msg: FORWARD -> BLOCK on %d", i);
    _send_tc_msg = true;
  }

  click_chatter("Changing port %d from %d to %d", i, _port[i].state, state);

  _port[i].state = state;
  click_gettimeofday(&_port[i].since);


  switch (state) {
  case BLOCK:
  case LISTEN:			// LISTEN == BLOCK except it timeouts to LEARN
    _input_sup->suppress(i);
    _output_sup->suppress(i);
    break;
  case LEARN:			// Port is "read-only" until timeout to FORWARD
    _input_sup->allow(i);
    _output_sup->suppress(i);
    break;
  case FORWARD:
    _input_sup->allow(i);
    _output_sup->allow(i);
    break;
  }

  return true;
}


void
EtherSpanTree::push(int source, Packet* p) {
  const BridgeMessage::wire* msg =
    reinterpret_cast<const BridgeMessage::wire*>(p->data());

  // Accept a message if it is better *or equal* to current message.
  // (if it is equal, we need it so that its timestamp is updated)
  int cmp = _port[source].msg.compare(msg);

  if (cmp <= 0) {
    _port[source].msg.from_wire(msg);
    _send_tc_msg &= !msg->tca; // Stop sending tc if this is an ack.
  }

  find_tree();

  p->kill();
}

void
EtherSpanTree::hello_hook(unsigned long v)
{
  EtherSpanTree *e = (EtherSpanTree *)v;
  e->periodic();
  for (int i = 0; i < e->noutputs(); i++) {
    Packet* p = e->generate_packet(i);
    if (p) e->output(i).push(p);
  }
  e->_hello_timer.schedule_after_ms(e->_best._hello_time * 1000);
}

Packet*
EtherSpanTree::generate_packet(int output)
{
  // Return without doing anything unless we are the "designated
  // bridge" for the lan on this port.  This is only if we can send
  // out a better advertisement than we have received.
  int cmp = _best.compare(&_port[output].msg);

  if (cmp < 0) {
    // This is a quite rare case.  It occurs when two nearly identical
    // messages on received on two different ports.  Then, the
    // increase caused by incrementing cost can cause my best message
    // to be worse than the other message which was very close to the
    // one I based _best on.
    return 0;
  }

  if (cmp == 0 && !_send_tc_msg) {
    return 0;
  }
  
  // _best is better (or we need send topology change)
  WritablePacket* p = Packet::make(sizeof(BridgeMessage::wire));
  BridgeMessage::wire* msg = reinterpret_cast<BridgeMessage::wire*>(p->data());
  
  if (cmp == 0) {
    // Root port, send topology change message
    BridgeMessage::fill_tcm(msg);
  } else {
    // We are designated bridge for this port, send _best.
    _best.to_wire(msg);
    msg->bridge_id = htonq(((u_int64_t)_bridge_priority << 48) | _bridge_id);
    msg->port_id = htons(output);
    if (_topology_change) {
      timeval cutoff;
      click_gettimeofday(&cutoff);
      cutoff.tv_sec -= _best._forward_delay + _best._max_age;
      if (timercmp(_topology_change, &cutoff, <)) {
	delete _topology_change;
	_topology_change = 0;
      } else {
	msg->tc = 1;
      }
    }
    if (_port[output].needs_tca) {
      _port[output].needs_tca = false;
      msg->tca = 1;
    }
  }
  memcpy(msg->src, _addr, 6);
  return p;
}

EXPORT_ELEMENT(EtherSpanTree)
ELEMENT_REQUIRES(Suppressor EtherSwitchBridgeMessage)

// generate Vector template instance
#include "vector.cc"
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<EtherSpanTree::PortInfo>;
#endif
