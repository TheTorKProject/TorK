#ifndef TORPT_H
#define TORPT_H

#include <bits/stdc++.h> 
#include <iostream> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <algorithm>
#include <regex>
#include <boost/asio.hpp>


#define TRANSPORT_NAME "tork"
#define SOCKS_VER      "socks5"
#define CLI_SOCKS_ADDR_PORT "127.0.0.1:1088"
#define SERVER_LISTEN4CLIREQS_PORT "127.0.0.1:1089"
#define SER_ARGS             ""

/* The size of temporary buffer for forwarding data */
#define PT_BUF_SIZE 4096


//Pluggable Transports Output Standards
const std::string PT_ENV_ERROR = "ENV-ERROR";
const std::string PT_VER_ERROR = "VERSION-ERROR";
const std::string PT_VER_INFO  = "VERSION";

const std::string PT_PRX_DONE   = "PROXY DONE";
const std::string PT_PRX_ERROR  = "PROXY-ERROR";

const std::string PT_C_METHOD         = "CMETHOD";
const std::string PT_C_METHOD_ERROR   = "CMETHOD-ERROR";
const std::string PT_C_METHOD_DONE    = "CMETHODS DONE";

const std::string PT_S_METHOD         = "SMETHOD";
const std::string PT_S_METHOD_ERROR   = "SMETHOD-ERROR";
const std::string PT_S_METHOD_DONE    = "SMETHODS DONE";

const std::string PT_STATUS        = "STATUS";
const std::string LOG              = "LOG";
const std::string LOG_SEVERITY     = "SEVERITY";
const std::string LOG_MESSAGE      = "MESSAGE";


#define TORVARS_VERSION "1"

//Tor Vars
#define PT_STATE_LOC "TOR_PT_STATE_LOCATION"
#define PT_MAN_TRVER "TOR_PT_MANAGED_TRANSPORT_VER"
#define PT_STDIN_CLOSE "TOR_PT_EXIT_ON_STDIN_CLOSE"

//Tor ENV_ERR Descriptions
const std::string ENV_VAR_NSET = "The following var is not set:";
const std::string ENV_VAR_SET  = "The following var is set when it shouldn't:";


class TorPT {

    public:
        bool exitOnStdinClose() {
            return pt_exit_on_stdin_close;
        }

        int waitUntilStdinClose();

    protected:

        inline void checkIfCommonVarsAreSet();

        void parseCommonVars();

        void display(std::string message);

        void display(std::string prefix, std::string message);

        void display(std::string prefix, std::vector<std::string> messages);

        void display(std::string prefix, std::string message,
                     std::string env_var);

        void display(std::string prefix, std::map<std::string,
                     std::string> messages);

        bool validURI(std::string url);


    private:
    
    /*
        "TOR_PT_STATE_LOCATION" -- A filesystem directory path where the
        PT is allowed to store permanent state if required. This
        directory is not required to exist, but the proxy SHOULD be able
        to create it if it does not. The proxy MUST NOT store state
        elsewhere.
        Example: TOR_PT_STATE_LOCATION=/var/lib/tor/pt_state/
    */
    std::string pt_state_location;

    /*
        "TOR_PT_MANAGED_TRANSPORT_VER" -- Used to tell the proxy which
        versions of this configuration protocol Tor supports. Clients
        MUST accept comma-separated lists containing any version that
        they recognise, and MUST work correctly even if some of the
        versions they do not recognise are non-numeric. Valid version
        characters are non-space, non-comma printable ASCII characters.
        Example: TOR_PT_MANAGED_TRANSPORT_VER=1,1a,2,4B
    */
    std::vector<std::string> pt_versions_supported;

    /*
        Specifies that the parent process will close the PT proxy's
        standard input (stdin) stream to indicate that the PT proxy
        should gracefully exit.

        PTs MUST NOT treat a closed stdin as a signal to terminate
        unless this environment variable is set to "1".

        PTs SHOULD treat stdin being closed as a signal to gracefully
        terminate if this environment variable is set to "1".
    */
    bool pt_exit_on_stdin_close;
};


#endif // TORPT_H