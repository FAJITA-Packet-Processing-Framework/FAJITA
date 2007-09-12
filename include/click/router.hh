// -*- c-basic-offset: 4; related-file-name: "../../lib/router.cc" -*-
#ifndef CLICK_ROUTER_HH
#define CLICK_ROUTER_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/sync.hh>
#include <click/task.hh>
#include <click/standard/threadsched.hh>
#if CLICK_NS
# include <click/simclick.h>
#endif
CLICK_DECLS
class Master;
class ElementFilter;
class RouterThread;
class HashMap_ArenaFactory;
class NotifierSignal;
class ThreadSched;
class Handler;
class NameInfo;

class Router { public:

    /** @name Public Member Functions */
    //@{
    // MASTER
    inline Master* master() const;

    // STATUS
    inline bool initialized() const;
    inline bool handlers_ready() const;
    inline bool running() const;

    // RUNCOUNT AND RUNCLASS
    enum { STOP_RUNCOUNT = -2147483647 - 1 };
    inline int32_t runcount() const;
    void adjust_runcount(int32_t delta);
    void set_runcount(int32_t rc);
    inline void please_stop_driver();

    // ELEMENTS
    inline const Vector<Element*>& elements() const;
    inline int nelements() const;
    // eindex -1 returns root_element(), other out-of-range indexes return 0
    inline Element* element(int eindex) const;
    inline Element* root_element() const;
    static Element* element(const Router *router, int eindex);
  
    Element* find(const String& name, ErrorHandler* errh = 0) const;
    Element* find(const String& name, String context, ErrorHandler* errh = 0) const;
    Element* find(const String& name, Element* context, ErrorHandler* errh = 0) const;

    int downstream_elements(Element* e, int port, ElementFilter* filter, Vector<Element*>& result);
    int upstream_elements(Element* e, int port, ElementFilter* filter, Vector<Element*>& result);
  
    const String& ename(int eindex) const;
    const String& elandmark(int eindex) const;
    const String& econfiguration(int eindex) const;
    void set_econfiguration(int eindex, const String& conf);
  
    // HANDLERS
    enum { FIRST_GLOBAL_HANDLER = 0x40000000 };
    static int hindex(const Element* e, const String& hname);
    static void element_hindexes(const Element* e, Vector<int>& result);

    // 'const Handler*' results last until that element/handlername modified
    static const Handler* handler(const Router* router, int hindex);
    static const Handler* handler(const Element* e, const String& hname);

    static void add_read_handler(const Element* e, const String& hname, ReadHandlerHook hook, void* thunk);
    static void add_write_handler(const Element* e, const String& hname, WriteHandlerHook hook, void* thunk);
    static void set_handler(const Element* e, const String& hname, int mask, HandlerHook hook, void* thunk1 = 0, void* thunk2 = 0);
    static int change_handler_flags(const Element* e, const String& hname, uint32_t clear_flags, uint32_t set_flags);

    // ATTACHMENTS AND REQUIREMENTS
    void* attachment(const String& aname) const;
    void*& force_attachment(const String& aname);
    void* set_attachment(const String& aname, void* value);
    
    ErrorHandler* chatter_channel(const String& channel_name) const;
    HashMap_ArenaFactory* arena_factory() const;

    inline ThreadSched* thread_sched() const;
    inline void set_thread_sched(ThreadSched* scheduler);
    inline int initial_home_thread_id(Task* task, bool scheduled) const;

    inline NameInfo* name_info() const;
    NameInfo* force_name_info();

    // UNPARSING
    inline const String& configuration_string() const;
    void unparse(StringAccum& sa, const String& indent = String()) const;
    void unparse_requirements(StringAccum& sa, const String& indent = String()) const;
    void unparse_classes(StringAccum& sa, const String& indent = String()) const;
    void unparse_declarations(StringAccum& sa, const String& indent = String()) const;
    void unparse_connections(StringAccum& sa, const String& indent = String()) const;

    String element_ports_string(int eindex) const;
    //@}
  
    // INITIALIZATION
    /** @name Internal Functions */
    //@{
    Router(const String& configuration, Master* master);
    ~Router();

    static void static_initialize();
    static void static_cleanup();

    inline void use();
    void unuse();

    inline const Vector<String>& requirements() const;
    void add_requirement(const String& requirement);
    int add_element(Element* e, const String& name, const String& conf, const String& landmark);
    int add_connection(int from_idx, int from_port, int to_idx, int to_port);
#if CLICK_LINUXMODULE
    int add_module_ref(struct module* module);
#endif

    inline Router* hotswap_router() const;
    void set_hotswap_router(Router* router);

    int initialize(ErrorHandler* errh);
    void activate(bool foreground, ErrorHandler* errh);
    inline void activate(ErrorHandler* errh);

    int new_notifier_signal(NotifierSignal& signal);
    //@}

    /** @cond never */
    // Needs to be public for Lexer, etc., but not useful outside
    struct Hookup {
	int idx;
	int port;
	Hookup()				: idx(-1) { }
	Hookup(int i, int p)			: idx(i), port(p) { }
    };
    /** @endcond never */
      
#if CLICK_NS
    simclick_node_t *simnode() const;
    int sim_get_ifid(const char* ifname);
    int sim_listen(int ifid, int element);
    int sim_if_ready(int ifid);
    int sim_write(int ifid, int ptype, const unsigned char *, int len,
		  simclick_simpacketinfo *pinfo);
    int sim_incoming_packet(int ifid, int ptype, const unsigned char *,
			    int len, simclick_simpacketinfo* pinfo);
    void sim_trace(const char* event);
    int sim_get_node_id();
    int sim_get_next_pkt_id();

  protected:
    Vector<Vector<int> *> _listenvecs;
    Vector<int>* sim_listenvec(int ifid);
#endif
  
  private:

    enum {
	ROUTER_NEW, ROUTER_PRECONFIGURE, ROUTER_PREINITIALIZE,
	ROUTER_LIVE, ROUTER_DEAD		// order is important
    };
    enum {
	RUNNING_DEAD = -2, RUNNING_INACTIVE = -1, RUNNING_PREPARING = 0,
	RUNNING_BACKGROUND = 1, RUNNING_ACTIVE = 2
    };

    Master* _master;
    
    atomic_uint32_t _runcount;

    atomic_uint32_t _refcount;
  
    Vector<Element*> _elements;
    Vector<String> _element_names;
    Vector<String> _element_configurations;
    Vector<String> _element_landmarks;
    Vector<int> _element_configure_order;

    Vector<Hookup> _hookup_from;
    Vector<Hookup> _hookup_to;

    /** @cond never */
    struct Gport {
	Vector<int> e2g;
	Vector<int> g2e;
	int size() const			{ return g2e.size(); }
    };
    /** @endcond never */
    Gport _gports[2];
  
    Vector<int> _hookup_gports[2];

    Vector<String> _requirements;

    volatile int _state;
    bool _have_connections : 1;
    volatile int _running;
  
    Vector<int> _ehandler_first_by_element;
    Vector<int> _ehandler_to_handler;
    Vector<int> _ehandler_next;

    Vector<int> _handler_first_by_name;

    enum { HANDLER_BUFSIZ = 256 };
    Handler** _handler_bufs;
    int _nhandlers_bufs;
    int _free_handler;

    Vector<String> _attachment_names;
    Vector<void*> _attachments;
  
    Element* _root_element;
    String _configuration;

    enum { NOTIFIER_SIGNALS_CAPACITY = 4096 };
    atomic_uint32_t* _notifier_signals;
    int _n_notifier_signals;
    HashMap_ArenaFactory* _arena_factory;
    Router* _hotswap_router;
    ThreadSched* _thread_sched;
    mutable NameInfo* _name_info;

    Router* _next_router;

#if CLICK_LINUXMODULE
    Vector<struct module*> _modules;
#endif
    
    Router(const Router&);
    Router& operator=(const Router&);
  
    void remove_hookup(int);
    void hookup_error(const Hookup&, bool, const char*, ErrorHandler*);
    int check_hookup_elements(ErrorHandler*);
    int check_hookup_range(ErrorHandler*);
    int check_hookup_completeness(ErrorHandler*);
  
    int processing_error(const Hookup&, const Hookup&, bool, int, ErrorHandler*);
    int check_push_and_pull(ErrorHandler*);
    
    void make_gports();
    int ngports(bool isout) const	{ return _gports[isout].g2e.size(); }
    inline int gport(bool isoutput, const Hookup&) const;
    inline Hookup gport_hookup(bool isoutput, int) const;
    void gport_list_elements(bool, const Bitvector&, Vector<Element*>&) const;
    void make_hookup_gports();
  
    void set_connections();
  
    String context_message(int element_no, const char*) const;
    int element_lerror(ErrorHandler*, Element*, const char*, ...) const;

    // private handler methods
    void initialize_handlers(bool, bool);
    inline Handler* xhandler(int) const;
    int find_ehandler(int, const String&, bool allow_star) const;
    static inline Handler fetch_handler(const Element*, const String&);
    void store_local_handler(int, const Handler&);
    static void store_global_handler(const Handler&);
    static inline void store_handler(const Element*, const Handler&);

    int global_port_flow(bool forward, Element* first_element, int first_port, ElementFilter* stop_filter, Bitvector& results);

    // global handlers
    static String router_read_handler(Element*, void*);

    /** @cond never */
    friend class Master;
    friend class Task;
    friend int Element::set_nports(int, int);
    /** @endcond never */
  
};


class Handler { public:

    enum Flags {
	OP_READ = 0x001, OP_WRITE = 0x002,
	READ_PARAM = 0x004, ONE_HOOK = 0x008,
	SPECIAL_FLAGS = OP_READ | OP_WRITE | READ_PARAM | ONE_HOOK,
	EXCLUSIVE = 0x010, RAW = 0x020,
	DRIVER_FLAG_0 = 0x040, DRIVER_FLAG_1 = 0x080,
	DRIVER_FLAG_2 = 0x100, DRIVER_FLAG_3 = 0x200,
	USER_FLAG_SHIFT = 10, USER_FLAG_0 = 1 << USER_FLAG_SHIFT
    };

    inline const String &name() const;
    inline uint32_t flags() const;
    inline void *thunk1() const;
    inline void *thunk2() const;

    inline bool readable() const;
    inline bool read_param() const;
    inline bool read_visible() const;
    inline bool writable() const;
    inline bool write_visible() const;
    inline bool visible() const;
    inline bool exclusive() const;
    inline bool raw() const;

    inline String call_read(Element *e, ErrorHandler *errh = 0) const;
    String call_read(Element *e, const String &param, bool raw,
		     ErrorHandler *errh) const;
    int call_write(const String &value, Element *e, bool raw,
		   ErrorHandler *errh) const;
  
    String unparse_name(Element *e) const;
    static String unparse_name(Element *e, const String &hname);

    static inline const Handler *blank_handler();
    
  private:

    String _name;
    union {
	HandlerHook h;
	struct {
	    ReadHandlerHook r;
	    WriteHandlerHook w;
	} rw;
    } _hook;
    void *_thunk1;
    void *_thunk2;
    uint32_t _flags;
    int _use_count;
    int _next_by_name;

    static const Handler *the_blank_handler;
    
    Handler(const String & = String());

    bool compatible(const Handler &) const;
  
    friend class Router;
  
};

/* The largest size a write handler is allowed to have. */
#define LARGEST_HANDLER_WRITE 65536


inline bool
operator==(const Router::Hookup& a, const Router::Hookup& b)
{
    return a.idx == b.idx && a.port == b.port;
}

inline bool
operator!=(const Router::Hookup& a, const Router::Hookup& b)
{
    return a.idx != b.idx || a.port != b.port;
}

/** @brief  Increment the router's reference count.
 *
 *  Routers are reference counted objects.  A Router is created with one
 *  reference, which is held by its Master object.  Normally the Router and
 *  all its elements will be deleted when the Master drops this reference, but
 *  you can preserve the Router for longer by adding a reference yourself. */
inline void
Router::use()
{
    _refcount++;
}

/** @brief  Return true iff the router is currently running.
 *
 *  A running router has been successfully initialized (so running() implies
 *  initialized()), and has not stopped yet. */
inline bool
Router::running() const
{
    return _running > 0;
}

/** @brief  Return true iff the router has been successfully initialized. */
inline bool
Router::initialized() const
{
    return _state == ROUTER_LIVE;
}

/** @brief  Return true iff the router's handlers have been initialized.
 *
 *  handlers_ready() returns false until each element's
 *  Element::add_handlers() method has been called.  This happens after
 *  Element::configure(), but before Element::initialize(). */
inline bool
Router::handlers_ready() const
{
    return _state > ROUTER_PRECONFIGURE;
}

/** @brief  Returns a vector containing all the router's elements.
 *  @invariant  elements()[i] == element(i) for all i in range. */
inline const Vector<Element*>&
Router::elements() const
{
    return _elements;
}

/** @brief  Returns the number of elements in the router. */
inline int
Router::nelements() const
{
    return _elements.size();
}

/** @brief  Returns the element with index @a eindex.
 *  @param  eindex  element index, or -1 for root_element()
 *  @invariant  If eindex(i) isn't null, then eindex(i)->@link Element::eindex eindex@endlink() == i.
 *
 *  This function returns the element with index @a eindex.  If @a eindex ==
 *  -1, returns root_element().  If @a eindex is otherwise out of range,
 *  returns null. */
inline Element*
Router::element(int eindex) const
{
    return element(this, eindex);
}

/** @brief  Returns this router's root element.
 *
 *  Every router has a root Element.  This element has Element::eindex -1 and
 *  name "".  It is not configured or initialized, and doesn't appear in the
 *  configuration; it exists only for convenience, when other Click code needs
 *  to refer to some arbitrary element at the top level of the compound
 *  element hierarchy. */
inline Element*
Router::root_element() const
{
    return _root_element;
}

inline const Vector<String>&
Router::requirements() const
{
    return _requirements;
}

inline ThreadSched*
Router::thread_sched() const
{
    return _thread_sched;
}

inline void
Router::set_thread_sched(ThreadSched* ts)
{
    _thread_sched = ts;
}

inline int
Router::initial_home_thread_id(Task* t, bool scheduled) const
{
    if (!_thread_sched)
	return ThreadSched::THREAD_UNKNOWN;
    else
	return _thread_sched->initial_home_thread_id(t, scheduled);
}

inline NameInfo*
Router::name_info() const
{
    return _name_info;
}

/** @brief  Returns the Master object for this router. */
inline Master*
Router::master() const
{
    return _master;
}

/** @brief  Return the router's runcount.
 *
 *  The runcount is an integer that determines whether the router is running.
 *  A running router has positive runcount.  Decrementing the router's runcount
 *  to zero or below will cause the router to stop, although elements like
 *  DriverManager can intercept the stop request and continue processing.
 *
 *  Elements request that the router stop its processing by calling
 *  adjust_runcount() or please_stop_driver(). */
inline int32_t
Router::runcount() const
{
    return _runcount.value();
}

/** @brief  Request a driver stop by adjusting the runcount by -1.
 *  @note   Equivalent to adjust_runcount(-1). */
inline void
Router::please_stop_driver()
{
    adjust_runcount(-1);
}

/** @brief  Returns the router's initial configuration string.
 *  @return The configuration string specified to the constructor. */
inline const String&
Router::configuration_string() const
{
    return _configuration;
}

inline void
Router::activate(ErrorHandler* errh)
{
    activate(true, errh);
}

/** @brief  Finds an element named @a name.
 *  @param  name     element name
 *  @param  errh     optional error handler
 *
 *  Returns the unique element named @a name, if any.  If no element named @a
 *  name is found, reports an error to @a errh and returns null.  The error is
 *  "<tt>no element named 'name'</tt>".  If @a errh is null, no error is
 *  reported.
 *
 *  This function is equivalent to find(const String&, String, ErrorHandler*)
 *  with a context argument of the empty string. */
inline Element *
Router::find(const String& name, ErrorHandler *errh) const
{
    return find(name, "", errh);
}

inline HashMap_ArenaFactory*
Router::arena_factory() const
{
    return _arena_factory;
}

/** @brief Returns the currently-installed router this router will eventually
 * replace.
 *
 * This function is only meaningful during a router's initialization.  If this
 * router was installed with the hotswap option, then hotswap_router() will
 * return the currently-installed router that this router will eventually
 * replace (assuming error-free initialization).  Otherwise, hotswap_router()
 * will return 0.
 */
inline Router*
Router::hotswap_router() const
{
    return _hotswap_router;
}

inline
Handler::Handler(const String &name)
    : _name(name), _thunk1(0), _thunk2(0), _flags(0), _use_count(0),
      _next_by_name(-1)
{
    _hook.rw.r = 0;
    _hook.rw.w = 0;
}

inline bool
Handler::compatible(const Handler& o) const
{
    return (_hook.rw.r == o._hook.rw.r && _hook.rw.w == o._hook.rw.w
	    && _thunk1 == o._thunk1 && _thunk2 == o._thunk2
	    && _flags == o._flags);
}

/** @brief Returns this handler's name. */
inline const String&
Handler::name() const
{
    return _name;
}

/** @brief Returns this handler's flags.

    The result is a bitwise-or of flags from the Flags enumeration type. */
inline uint32_t
Handler::flags() const
{
    return _flags;
}

/** @brief Returns this handler's first callback data. */
inline void*
Handler::thunk1() const
{
    return _thunk1;
}

/** @brief Returns this handler's second callback data. */
inline void*
Handler::thunk2() const
{
    return _thunk2;
}

/** @brief Returns true iff this is a valid read handler. */
inline bool
Handler::readable() const
{
    return _flags & OP_READ;
}

/** @brief Returns true iff this is a valid read handler that may accept
    parameters. */
inline bool
Handler::read_param() const
{
    return _flags & READ_PARAM;
}

/** @brief Returns true iff this is a valid visible read handler.

    Only visible handlers may be called from outside the router
    configuration. */
inline bool
Handler::read_visible() const
{
    return _flags & OP_READ;
}

/** @brief Returns true iff this is a valid write handler. */
inline bool
Handler::writable() const
{
    return _flags & OP_WRITE;
}

/** @brief Returns true iff this is a valid visible write handler.

    Only visible handlers may be called from outside the router
    configuration. */
inline bool
Handler::write_visible() const
{
    return _flags & OP_WRITE;
}

/** @brief Returns true iff this handler is visible. */
inline bool
Handler::visible() const
{
    return _flags & (OP_READ | OP_WRITE);
}

/** @brief Returns true iff this handler is exclusive.

    Exclusive means mutually exclusive with all other router processing.  In
    the Linux kernel module driver, reading or writing an exclusive handler
    using the Click filesystem will first lock all router threads and
    handlers. */
inline bool
Handler::exclusive() const
{
    return _flags & EXCLUSIVE;
}

/** @brief Returns true iff quotes should be removed when calling this
    handler.

    A raw handler expects and returns raw text.  Click will unquote quoted
    text before passing it to a raw handler, and (in the Linux kernel module)
    will not add a courtesy newline to the end of a raw handler's value. */
inline bool
Handler::raw() const
{
    return _flags & RAW;
}

/** @brief Call a read handler without parameters.
    @param e element on which to call the handler
    @param errh error handler

    The element must be nonnull; to call a global handler, pass the relevant
    router's Router::root_element().  @a errh may be null, in which case
    errors are ignored. */
inline String
Handler::call_read(Element* e, ErrorHandler* errh) const
{
    return call_read(e, String(), false, errh);
}

/** @brief Returns a handler incapable of doing anything.
 *
 *  The returned handler returns false for readable() and writable()
 *  and has flags() of zero. */
inline const Handler *
Handler::blank_handler()
{
    return the_blank_handler;
}

CLICK_ENDDECLS
#endif
