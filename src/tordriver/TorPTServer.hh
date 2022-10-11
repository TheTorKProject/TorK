#ifndef TORPTSERVER_H
#define TORPTSERVER_H

#include "TorPT.hh"
#include "../common/Common.hh"
#include "../controller/ControllerServer.hh"

//Tor Vars
#define PT_SERVER_TRANSPORTS     "TOR_PT_SERVER_TRANSPORTS"
#define PT_SERVER_TRANSPORTS_OPT "TOR_PT_SERVER_TRANSPORT_OPTIONS"
#define PT_SERVER_BINDADDR       "TOR_PT_SERVER_BINDADDR"
#define PT_ORPORT                "TOR_PT_ORPORT"
#define PT_EXT_SERVER_PORT       "TOR_PT_EXTENDED_SERVER_PORT"
#define PT_AUTH_COOKIE           "TOR_PT_AUTH_COOKIE_FILE"


class TorPTServer : public TorPT {

  public:

    TorPTServer() {
        pt_bindaddr = std::map<std::string, std::string>();
    }

    void initialize(ControllerServer *controller, int run_mode);

    std::vector<std::string>           getTransports()        {return pt_transports;}

    std::map<std::string, std::string> getTransportsOptions() {return pt_transport_options;}

    std::map<std::string, std::string> getBindAddresses()     {return pt_bindaddr;}

    std::string                        getOnionAddress()      {return pt_addr_orport;}

    std::string                        getOnionPort()         {return pt_orport;}

    std::string                        getExtendedOnionPort() {return pt_extented_port;}

    std::string                        getAuthCookie()        {return pt_auth_cookie;}

    std::string                        getBindAddressPT(std::string pt);


  protected:

    inline void checkIfVarsAreSet();

    void parseVariables();


  private:
  
    /*
    "TOR_PT_SERVER_TRANSPORTS"

      Specifies the PT protocols the server proxy should initialize,
      as a comma separated list of PT names.

      PTs SHOULD ignore PT names that it does not recognize.

      Parent processes MUST set this environment variable when
      launching a server-side PT reverse proxy instance.

      Example:

        TOR_PT_SERVER_TRANSPORTS=obfs3,scramblesuit
    */
    std::vector<std::string> pt_transports;

    /*
    "TOR_PT_SERVER_TRANSPORT_OPTIONS"

      Specifies per-PT protocol configuration directives, as a
      semicolon-separated list of <key>:<value> pairs, where <key>
      is a PT name and <value> is a k=v string value with options
      that are to be passed to the transport.

      Colons, semicolons, and backslashes MUST be
      escaped with a backslash.

      If there are no arguments that need to be passed to any of
      PT transport protocols, "TOR_PT_SERVER_TRANSPORT_OPTIONS"
      MAY be omitted.

      Example:

        TOR_PT_SERVER_TRANSPORT_OPTIONS=scramblesuit:key=banana;automata:rule=110;automata:depth=3

        Will pass to 'scramblesuit' the parameter 'key=banana' and to
        'automata' the arguments 'rule=110' and 'depth=3'.
    */
  std::map<std::string, std::string> pt_transport_options;

  /*
  "TOR_PT_SERVER_BINDADDR"

      A comma separated list of <key>-<value> pairs, where <key> is
      a PT name and <value> is the <address>:<port> on which it
      should listen for incoming client connections.

      The keys holding transport names MUST be in the same order as
      they appear in "TOR_PT_SERVER_TRANSPORTS".

      The <address> MAY be a locally scoped address as long as port
      forwarding is done externally.

      The <address>:<port> combination MUST be an IP address
      supported by `bind()`, and MUST NOT be a host name.

      Applications MUST NOT set more than one <address>:<port> pair
      per PT name.

      If there is no specific <address>:<port> combination to be
      configured for any transports, "TOR_PT_SERVER_BINDADDR" MAY
      be omitted.

      Example:

          TOR_PT_SERVER_BINDADDR=obfs3-198.51.100.1:1984,scramblesuit-127.0.0.1:4891
    */
  std::map<std::string, std::string> pt_bindaddr;

  /*
  "TOR_PT_ORPORT"

      Specifies the destination that the PT reverse proxy should forward
      traffic to after transforming it as appropriate, as an
      <address>:<port>.

      Connections to the destination specified via "TOR_PT_ORPORT"
      MUST only contain application payload.  If the parent process
      requires the actual source IP address of client connections
      (or other metadata), it should set "TOR_PT_EXTENDED_SERVER_PORT"
      instead.

      Example:

        TOR_PT_ORPORT=127.0.0.1:9001
    */
  std::string pt_addr_orport;
  std::string pt_orport;

  /*
  "TOR_PT_EXTENDED_SERVER_PORT"

      Specifies the destination that the PT reverse proxy should
      forward traffic to, via the Extended ORPort protocol [EXTORPORT]
      as an <address>:<port>.

      The Extended ORPort protocol allows the PT reverse proxy to
      communicate per-connection metadata such as the PT name and
      client IP address/port to the parent process.

      If the parent process does not support the ExtORPort protocol,
      it MUST set "TOR_PT_EXTENDED_SERVER_PORT" to an empty string.

      Example:

        TOR_PT_EXTENDED_SERVER_PORT=127.0.0.1:4200
    */
  std::string pt_extended_addr;
  std::string pt_extented_port;

  /*
  "TOR_PT_AUTH_COOKIE_FILE"

      Specifies an absolute filesystem path to the Extended ORPort
      authentication cookie, required to communicate with the
      Extended ORPort specified via "TOR_PT_EXTENDED_SERVER_PORT".

      If the parent process is not using the ExtORPort protocol for
      incoming traffic, "TOR_PT_AUTH_COOKIE_FILE" MUST be omitted.

      Example:

        TOR_PT_AUTH_COOKIE_FILE=/var/lib/tor/extended_orport_auth_cookie
    */
  std::string pt_auth_cookie;

  ControllerServer *_controller;
};


#endif // TORPTSERVER_H