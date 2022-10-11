#ifndef TORVARS_CLIENT_H
#define TORVARS_CLIENT_H

#include "TorPT.hh"
#include "../common/Common.hh"
#include "../controller/ControllerClient.hh"


//Tor Vars
#define PT_CLIENT_TRANSPORTS "TOR_PT_CLIENT_TRANSPORTS"
#define PT_PROXY             "TOR_PT_PROXY"

class TorPTClient : public TorPT {

    public:

        TorPTClient(){};

        void initialize(ControllerClient *controller, int run_mode);
        
    protected:

        inline void checkIfVarsAreSet();

        void parseVariables();

        void notifyParentAboutProxy(short proxy_status);

    private:

        /*
            "TOR_PT_CLIENT_TRANSPORTS"

            Specifies the PT protocols the client proxy should initialize,
            as a comma separated list of PT names.

            PTs SHOULD ignore PT names that it does not recognize.

            Parent processes MUST set this environment variable when
            launching a client-side PT proxy instance.

            Example:

                TOR_PT_CLIENT_TRANSPORTS=obfs2,obfs3,obfs4
        */
        std::vector<std::string> pt_transports;

        /*
            "TOR_PT_PROXY"

            Specifies an upstream proxy that the PT MUST use when making
            outgoing network connections.  It is a URI [RFC3986] of the
            format:

                <proxy_type>://[<user_name>[:<password>][@]<ip>:<port>.

            The "TOR_PT_PROXY" environment variable is OPTIONAL and
            MUST be omitted if there is no need to connect via an
            upstream proxy.

                Examples:

                TOR_PT_PROXY=socks5://tor:test1234@198.51.100.1:8000
                TOR_PT_PROXY=socks4a://198.51.100.2:8001
                TOR_PT_PROXY=http://198.51.100.3:443
        */
        std::string pt_proxy;

        ControllerClient *_controller;
};

#endif // TORVARS_CLIENT_H