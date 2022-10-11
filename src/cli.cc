#include "cli/CliUnixClient.hh"

void display_commands_help() {
    std::cout << "Common commands:" << std::endl;
    std::cout << "stats_fp  - Display the frame pool status.";

    std::cout << "Bridge-only commands:" << std::endl;
    std::cout << "stats_cli - Display the client's frame queues stats";

    std::cout << "Client-only commands:" << std::endl;
}

int main(int argc, char **argv)
{
    CliUnixClient client = CliUnixClient("/tmp/tork_bridge");

    std::cout << "Tork: Cli tool started. Type your commands:" << std::endl;
    client.run();
}
