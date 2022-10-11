#include "TorPT.hh"


inline void TorPT::checkIfCommonVarsAreSet() {
    if (std::getenv(PT_STATE_LOC) == NULL) {
        display(PT_ENV_ERROR, ENV_VAR_NSET, PT_STATE_LOC);
        exit(1);
    }
    if (std::getenv(PT_MAN_TRVER) == NULL) {
        display(PT_ENV_ERROR, ENV_VAR_NSET, PT_MAN_TRVER);
        exit(1);
    }
    if (std::getenv(PT_STDIN_CLOSE) == NULL) {
        display(PT_ENV_ERROR, ENV_VAR_NSET, PT_STDIN_CLOSE);
        exit(1);
    }
}

void TorPT::parseCommonVars() {
    display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"TorVars Checking common vars\"");
    //If any of mandatory vars are not set abort immediately
    checkIfCommonVarsAreSet();

    struct stat info;
    display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"TorVars Checking PT_STATE_LOC\"");
    char* env = std::getenv(PT_STATE_LOC);
    

    if( stat( env, &info ) != 0 ) {
        if (mkdir(env, 022) == -1) {
            display(PT_ENV_ERROR, "Could not create PT STATE folder");
            exit(1);
        }
        else {
            this->pt_state_location = env;
        }
    }
    display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"TorVars Checking PT_MAN_TRVER\"");
    env = std::getenv(PT_MAN_TRVER);
    this->pt_versions_supported = boost::split(this->pt_versions_supported, env, boost::is_any_of(","));

    std::vector<std::string>::const_iterator it = std::find(this->pt_versions_supported.begin(), this->pt_versions_supported.end(), "1");

    if(it == this->pt_versions_supported.end()) {
        display(PT_VER_ERROR, "no-version");
        exit(1);
    } else {
        display(PT_VER_INFO, TORVARS_VERSION);
    }
    
    display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"TorVars Checking PT_STDIN_CLOSE\"");
    env = std::getenv(PT_STDIN_CLOSE);

    if (env != NULL) {
        display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"TorVars PT_STDIN_CLOSE is " + std::string(env) + "\"");
        if (strcmp(env, "1") == 0) {
            this->pt_exit_on_stdin_close = true;
        } else if (strcmp(env, "0") == 0) {
            this->pt_exit_on_stdin_close = false;
        } else {
            display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"TorVars PT_STDIN_CLOSE not correcly parsed\"");
            display(PT_ENV_ERROR, "Failed to parse", PT_STDIN_CLOSE);
            exit(1);
        }
    }
    else {
        display(PT_STATUS, "TRANSPORT=tork", "DEBUG=\"TorVars PT_STDIN_CLOSE is NULL\"");
        this->pt_exit_on_stdin_close = false;
    }

}

void TorPT::display(std::string message) {
    std::cout << message << std::endl;
}

void TorPT::display(std::string prefix, std::vector<std::string> messages) {
    std::ostringstream os;
    os << prefix << " ";
    for (std::string message : messages)
        os << message << " ";
    
    os << std::endl;

    std::cout << os.str();
}

void TorPT::display(std::string prefix, std::map<std::string, std::string> messages) {
    std::ostringstream os;
    std::map<std::string, std::string>::iterator it;

    os << prefix << " ";

    for (it = messages.begin(); it != messages.end(); ++it) {
        os << it->first << "=" << it->second << " ";
    }
    os << std::endl;

    std::cout << os.str();
}

void TorPT::display(std::string prefix, std::string message) {
    std::cout << prefix << " " << message << std::endl;
}

void TorPT::display(std::string prefix, std::string message, std::string env_var) {
    std::cout << prefix << " " << message << " " << env_var << std::endl;
}

bool TorPT::validURI(std::string url) {
    //unsigned counter = 0;
    std::regex url_regex (R"(^(([^:\/?#]+):)?(//([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)", std::regex::extended);
    std::smatch url_match_result;

    if (std::regex_match(url, url_match_result, url_regex)) {
        return true;
    } else {
        return false;
    }
}

int TorPT::waitUntilStdinClose() {
    fd_set read_set;

    while (true) {
    FD_ZERO (&read_set);
    FD_SET (fileno(stdin), &read_set);

    if (select (FD_SETSIZE, &read_set, NULL, NULL, NULL) < 0) {
            return -1;
        }

        if (FD_ISSET (fileno(stdin), &read_set)) {
            return 0;
        }
    }
}
