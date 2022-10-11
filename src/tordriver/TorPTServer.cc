#include "TorPTServer.hh"


void TorPTServer::initialize(ControllerServer *controller, int run_mode)
{
    assert(controller != NULL);
    _controller = controller;

    assert(run_mode == RUN_FOREGROUND);

    parseVariables();
}


void TorPTServer::checkIfVarsAreSet()
{
    if (std::getenv(PT_SERVER_TRANSPORTS) == NULL) {
        display(PT_ENV_ERROR, ENV_VAR_NSET, PT_SERVER_TRANSPORTS);
        exit(1);
    }
    if (std::getenv(PT_ORPORT) == NULL) {
        display(PT_ENV_ERROR, ENV_VAR_NSET, PT_ORPORT);
        exit(1);
    }
}


void TorPTServer::parseVariables()
{
    //No need to parse if mandatory vars are not set
    checkIfVarsAreSet();

    //Parse the common vars
    TorPT::parseCommonVars();

    char *env;

    env = std::getenv(PT_ORPORT);
    //split address and port information
    std::vector<std::string> params;
    params = boost::split(params, env, boost::is_any_of(":"));
    this->pt_addr_orport = params[0];
    this->pt_orport = params[1];
    display(PT_STATUS, "TRANSPORT=tork", "ONIONADDR=" + pt_addr_orport + " ONIONPORT=" + pt_orport);

    if ((env = std::getenv(PT_SERVER_TRANSPORTS_OPT)) != NULL) {
        std::vector<std::string> params;
        params = boost::split(params, env, boost::is_any_of(";"));
        for (std::string param : params) {
            std::vector<std::string> result;
            result = boost::split(result, param, boost::is_any_of(":"));
            this->pt_transport_options.insert(std::pair<std::string, std::string>(result[0], result[1]));
        }
    }

    if ((env = std::getenv(PT_SERVER_BINDADDR)) != NULL) {
        std::vector<std::string> params;
        params = boost::split(params, env, boost::is_any_of(","));
        for (std::string param : params) {
            std::vector<std::string> result;
            result = boost::split(result, param, boost::is_any_of("-"));
            this->pt_bindaddr.insert(std::pair<std::string, std::string>(result[0], result[1]));
        }
    }


    env = std::getenv(PT_SERVER_TRANSPORTS);
    this->pt_transports = boost::split(this->pt_transports, env, boost::is_any_of(","));

    std::vector<std::string>::const_iterator it = std::find(this->pt_transports.begin(), this->pt_transports.end(), TRANSPORT_NAME);

    if (it != this->pt_transports.end()) {
        std::vector<std::string> params = std::vector<std::string>();
        params.push_back(TRANSPORT_NAME);

        params.push_back(SERVER_LISTEN4CLIREQS_PORT);
        display(PT_STATUS, "TRANSPORT=tork", "ADDRESS=def");

        params.push_back(SER_ARGS);

        display(PT_S_METHOD, params);

    } else {
        display(PT_S_METHOD_ERROR, TRANSPORT_NAME, "Not available");
    }

    display(PT_S_METHOD_DONE);

    if ((env = std::getenv(PT_EXT_SERVER_PORT)) != NULL) {
        std::vector<std::string> params;
        params = boost::split(params, env, boost::is_any_of(":"));
        this->pt_extended_addr = params[0];
        this->pt_extented_port = params[1];
        display(PT_STATUS, "TRANSPORT=tork", "EXTONIONADDR=" + pt_extended_addr + " EXTONIONPORT=" + pt_extented_port);
    }

    if ((env = std::getenv(PT_AUTH_COOKIE)) != NULL) {
        //Not possible to define a auth cookie without specifying EXT PORT
        if (std::getenv(PT_EXT_SERVER_PORT) == NULL)
            display(PT_ENV_ERROR, ENV_VAR_SET, PT_AUTH_COOKIE);
        else {
            this->pt_auth_cookie = env;
        }
    }

    
}


std::string TorPTServer::getBindAddressPT(std::string pt) {
    std::vector<std::string>::const_iterator it = std::find(this->pt_transports.begin(), this->pt_transports.end(), pt);

    if (it != this->pt_transports.end()) {
        return *it;
    }
    return nullptr;
}