// -*- mode: c++; c-basic-offset: 4 -*-

#ifndef CLICK_METRON_HH
#define CLICK_METRON_HH

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include <click/hashmap.hh>

#include "../json/json.hh"

CLICK_DECLS

class Metron;

class CPU {
    public:
        CPU(int id, String vendor, long _frequency)
            : _id(id), _vendor(vendor), _frequency(_frequency) {
        }

        int getId();
        String getVendor();
        long getFrequency();

        Json toJSON();

        static const int MEGA_HZ = 1000000;

    private:
        int _id;
        String _vendor;
        long _frequency;
};

class NIC {
    public:
        Element *element;

        String getId() {
            return element->name();
        }

        String getDeviceId();

        Json toJSON(bool stats = false);

        int queuePerPool() {
            return atoi(
                callRead("nb_rx_queues").c_str()) /
                atoi(callRead("nb_vf_pools").c_str()
            );
        }

        int cpuToQueue(int id) {
            return id * (queuePerPool());
        }

        String callRead(String h);
        String callTxRead(String h);
};

class ServiceChain {
    public:

        class RxFilter {
            public:

                RxFilter(ServiceChain *sc) : _sc(sc) {

                }

                ~RxFilter() {

                }

                String method;
                ServiceChain *_sc;

                static RxFilter *fromJSON(
                    Json j, ServiceChain *sc, ErrorHandler *errh
                );
                Json toJSON();

                int cpuToQueue(NIC *nic, int cpuid) {
                    return nic->cpuToQueue(cpuid);
                }

                virtual int apply(NIC *nic, ErrorHandler *errh);

                Vector<Vector<String>> values;
        };

        enum ScStatus{
            SC_FAILED,
            SC_OK=1
        };

        String id;
        RxFilter *rxFilter;
        String config;

        enum ScStatus status;

        class Stat {
            public:
                long long useless;
                long long useful;
                long long count;
                float load;

                Stat() : useless(0), useful(0), count(0), load(0) {

                }
        };
        Vector<Stat> nic_stats;

        ServiceChain(Metron *m);
        ~ServiceChain();

        static ServiceChain *fromJSON(Json j, Metron *m, ErrorHandler *errh);
        int reconfigureFromJSON(Json j, Metron *m, ErrorHandler *errh);

        Json toJSON();
        Json statsToJSON();

        inline String getId() {
            return id;
        }

        inline int getUsedCpuNr() {
            return _used_cpu_nr;
        }

        inline int getMaxCpuNr() {
            return _max_cpu_nr;
        }


        inline int getCpuMap(int i) {
            return _cpus[i];
        }

        inline int getNICNr() {
            return _nics.size();
        }

        inline NIC *getNICById(String id) {
            for (NIC *nic : _nics) {
                if (nic->getId() == id)
                    return nic;
            }

            return 0;
        }

        inline int getNICIndex(NIC *nic) {
            for (int i = 0; i < _nics.size(); i++) {
                if (_nics[i] == nic)
                    return i;
            }

            return -1;
        }

        inline NIC *getNICByIndex(int i) {
            return _nics[i];
        }

        Bitvector assignedCpus();

        String generateConfig();
        String generateConfigSlaveFDName(int nic_index, int cpu_index) {
            return "slaveFD" + String(nic_index) + "C" + String(cpu_index);
        }

        Vector<String> buildCmdLine(int socketfd);

        void controlInit(int fd, int pid);

        int controlReadLine(String &line);

        void controlWriteLine(String cmd);

        String controlSendCommand(String cmd);

        void checkAlive();

        int call(
            String fnt, bool hasResponse, String handler,
            String &response, String params
        );
        String simpleCallRead(String handler);
        int callRead(String handler, String &response, String params = "");
        int callWrite(String handler, String &response, String params = "");

        Vector<int> &getCpuMapRef() {
            return _cpus;
        }

        struct timing_stats {
            Timestamp start, parse, launch;
            Json toJSON();
        };
        void setTimingStats(struct timing_stats ts) {
            _timing_stats = ts;
        }

        struct autoscale_timing_stats {
            Timestamp autoscale_start, autoscale_end;
            Json toJSON();
        };
        void setAutoscaleTimingStats(struct autoscale_timing_stats ts) {
            _as_timing_stats = ts;
        }

        void doAutoscale(int nCpuChange);

    private:

        Metron *_metron;
        Vector<int> _cpus;
        Vector<NIC *> _nics;
        Vector<float> _cpuload;
        float _total_cpuload;
        int _socket;
        int _pid;
        struct timing_stats _timing_stats;
        struct autoscale_timing_stats _as_timing_stats;
        int _used_cpu_nr;
        int _max_cpu_nr;
        bool _autoscale;
        Timestamp _last_autoscale;

        friend class Metron;
};

/*
=c

Metron */

class Metron : public Element {
    public:

        Metron() CLICK_COLD;
        ~Metron() CLICK_COLD;

        const char *class_name() const  { return "Metron"; }
        const char *port_count() const  { return PORTS_0_0; }

        int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
        int initialize(ErrorHandler *) CLICK_COLD;
        bool discover();
        void cleanup(CleanupStage) CLICK_COLD;

        void run_timer(Timer *t) override;

        void add_handlers() CLICK_COLD;
        static int param_handler(
            int operation, String &param, Element *e,
            const Handler *, ErrorHandler *errh
        ) CLICK_COLD;
        static String read_handler(Element *e, void *user_data) CLICK_COLD;
        static int write_handler(
            const String &data, Element *e, void *user_data,
            ErrorHandler *errh
        ) CLICK_COLD;

        void setHwInfo(Json &j);

        Json toJSON();
        Json statsToJSON();
        Json controllersToJSON();
        int  controllersFromJson(Json j);

        enum {
            h_discovered, h_controllers,
            h_resources,  h_stats,
            h_delete_chains, h_put_chains,
            h_chains, h_chains_stats, h_chains_proxy
        };

        ServiceChain *findChainById(String id);

        int instantiateChain(ServiceChain *sc, ErrorHandler *errh);
        int removeChain(ServiceChain *sc, ErrorHandler *errh);

        int getCpuNr() {
            return click_max_cpu_ids();
        }

        int getNbChains() {
            return _scs.size();
        }

        int getAssignedCpuNr();

        bool assignCpus(ServiceChain *sc, Vector<int> &map);
        void unassignCpus(ServiceChain *sc);

        const float CPU_OVERLOAD_LIMIT = (float) 0.7;
        const float CPU_UNERLOAD_LIMIT = (float) 0.4;

        /* Agent's default REST configuration */
        const int    DEF_AGENT_PORT  = 80;
        const String DEF_AGENT_PROTO = "http";

        /* Controller's default REST configuration */
        const int    DEF_DISCOVER_PORT      = 80;
        const int    DEF_DISCOVER_REST_PORT = 8181;
        const String DEF_DISCOVER_DRIVER    = "restServer";
        const String DEF_DISCOVER_USER      = "onos";
        const String DEF_DISCOVER_PATH      = "/onos/v1/network/configuration/";

        /* Bound the discovery process */
        const unsigned short DISCOVERY_ATTEMPTS = 3;

    private:
        String _id;
        Vector<String> _args;
        Vector<String> _dpdk_args;

        HashMap<String,NIC> _nics;
        HashMap<String,ServiceChain *> _scs;

        Vector<ServiceChain *> _cpu_map;

        String _cpu_vendor;
        String _hw;
        String _sw;
        String _serial;

        bool _timing_stats;

        /* Agent's (local) information */
        String _agent_ip;
        int    _agent_port;

        /* Controller's (remote) information */
        String _discover_ip;
        int    _discover_port;      // Port that talks to agent (Metron protocol)
        int    _discover_rest_port; // REST port
        String _discover_path;
        String _discover_user;
        String _discover_password;

        /* Discovery status */
        bool _discovered;

        int runChain(ServiceChain *sc, ErrorHandler *errh);

        Timer _timer;

        friend class ServiceChain;
};

CLICK_ENDDECLS

#endif