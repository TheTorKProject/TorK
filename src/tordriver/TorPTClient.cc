#include <assert.h>

#include "TorPTClient.hh"
#include "../common/Common.hh"



void TorPTClient::initialize(ControllerClient *controller, int run_mode)
{
    assert(controller != NULL);
    _controller = controller;

    assert(run_mode == RUN_FOREGROUND);

    display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"Parsing vars...\"");
    parseVariables();
    display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"Starting Proxy...\"");

    display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"Proxy is running...\"");
    notifyParentAboutProxy(0);

}


void TorPTClient::checkIfVarsAreSet() {
    if (std::getenv(PT_CLIENT_TRANSPORTS) == NULL) {
        display(PT_ENV_ERROR, ENV_VAR_NSET, PT_CLIENT_TRANSPORTS);
        exit(1);
    }
}


void TorPTClient::parseVariables() {
    display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"Starting checking if vars are set\"");
    //No need to parse if mandatory vars are not set
    checkIfVarsAreSet();
    display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"Parsing common vars\"");
    //Parse the common vars
    TorPT::parseCommonVars();
    display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"Finished Parsing common vars\"");

    if (std::getenv(PT_PROXY) != NULL) {
        this->pt_proxy = std::getenv(PT_PROXY);
        display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"PT_PROXY is set\"");
        
        if (validURI(this->pt_proxy)) {
            display(PT_PRX_DONE);
        }
        else {
            display(PT_PRX_ERROR);
        }
    }

}


/**
 * Notifies the Parent Proxy that the SOCKS Proxy is alive and well to start
 * receiving requests OR in case of failure
 */
void TorPTClient::notifyParentAboutProxy(short proxy_status) {

    if (proxy_status == -1) {
        display(PT_C_METHOD_ERROR, TRANSPORT_NAME, "Proxy has crashed!");
        return;
    }

    display(PT_STATUS, "TRANSPORT=tork", "DEBUG=PT_CLIENT_TRANSPORTS");
    char* env = std::getenv(PT_CLIENT_TRANSPORTS);
    this->pt_transports = boost::split(this->pt_transports, env, boost::is_any_of(","));

    //For each transport initialized, the PT proxy reports the listener status
    //back to the parent via messages to stdout.
    std::vector<std::string>::const_iterator it = std::find(this->pt_transports.begin(), this->pt_transports.end(), TRANSPORT_NAME);

    //Parent process recognize our transport name as a transport
    if (it != this->pt_transports.end()) {
        std::vector<std::string> params = std::vector<std::string>();
        params.push_back(TRANSPORT_NAME);
        params.push_back(SOCKS_VER);
        params.push_back(CLI_SOCKS_ADDR_PORT);
        //display(PT_STATUS, "TRANSPORT=tork", "DEBUG=CMETHOD set");
        display(PT_C_METHOD, params);

    } else {
        display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"CMETHOD not set\"");
        //there are no pt compatible with the client
        display(PT_C_METHOD_ERROR, TRANSPORT_NAME, "No tork available");
    }

    display(PT_C_METHOD_DONE);
}


