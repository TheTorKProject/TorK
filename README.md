# TorK

## Releases and Version Information

The `main` branch should be considered as the latest `beta` version. This
indicates that the version is reasonably stable but lacks exhaustive testing.

Features branches should be considered as `unstable` as they contain fresh
new features and experimental patches which were not throughly tested.

**Latest Stable Version:** 2.0.9.1

**Current Stable Branch:** 2.0.x

For exahustive testing, only stable versions should be used.

**Branch 1.x:** Branch `1.x` already reached it's end-of-life, thus it will not
receive any new features, patches or security fixes.

## Build

New variant of the Tor system which aims at increasing its resilience against traffic correlation attacks by introducing K-anonymous circuits

Dependencies:
*   CMake version 3.10+
*   gcc (g++) 7.5.0+
*   Lib OpenSSL (Dev) 1.1.1+
*   Libboost C++ 1.65.0+
*   Tor 0.4.1.9+

Developed and tested under Ubuntu bionic 18.04.5 LTS (4.15.0-112-generic).

To build from the source:
```
cmake .
make
```

To disable SSL:

Edit the `src/common/Common.hh` and set the `USE_SSL` to zero:

```
...
#define USE_SSL  (1)
...
```

SSL disable causes a compilation warning and it should be use for debugging purposes.

### Requirements
Disable bridge / guard mark as down by comment the line:

```
src/feature/client/entrynodes.c in entry_guard_failed():

entry_guards_note_guard_failure(guard->in_selection, guard);
```

### Using Docker
To quickly deploy a TorK developing setup, use both images of Docker:
*   `tork:regular` -> contains the TorK and all its dependencies
*   `tork:sgx`     -> contains the TorK, its dependencies and support for SCONE,
an extension to Intel SGX in order to provide additional security.

**Tip**: This images can be generated using the `Docker` and `DockerSGX` folders
available on the `tork-analysis` repo. The building process can take some time,
specially for the `tork:sgx` image. You will need to be able to access the
`sconecuratedimages/crosscompilers` private docker image, by requesting
access through email (See SCONE Refs at bottom).

You will need to setup a Docker network:
```
docker network create torknet
docker network ls
docker network inspect torknet
```

**Attention:** Depending on the IP subnet used by Docker you may need to
change `/usr/src/tork/tor_confs/torrc_docker_client_clean` configuration file
to successfully connect TorK clients to the bridge.

To run a TorK Bridge container simply run:
```
docker run --rm --hostname=bridge --name=bridge --network=torknet -it tork:regular tor -f /usr/src/tork/tor_confs/torrc_docker_bridge_clean
```

To run a TorK Client container under the same network run:
```
docker run --rm --hostname=client1 --name=client1 --network=torknet -p 9051:9051 -p 9091:9091 -it tork:regular tor -f /usr/src/tork/tor_confs/torrc_docker_client_clean
```
*   9091 will be the control port, where TorK clients can send commands via CLI interface.
*   9051 will be the Tor SOCKS port, where clients can configure their browser to send
request through Tor.

Access the CLI interface by using the forward port on the host, set a KCircuit of N, (in this case 1),
and activate the channel:
```
nc 127.0.0.1 9091
set_k 1
ch active
```

A few seconds later, Tor should give a indication of 100% Bootstrap complete, thus
we can send request to the Tor Network:
```
curl -x socks5h://127.0.0.1:9051 https://ifconfig.io
```

The output should be the IP address of the exit node used by Tor.

To cleanly exit the client, use the command `shut` on the cli interface.

#### Using Intel SGX Support
To run TorK under Intel SGX support use the `tork:sgx` image.
Only TorK will be running inside SGX enclave, and then, the executable of Tor
must be provided mounted on the container.

First, make sure you are using a compatible Intel CPU and the Linux SGX Driver
is installed:

*   [List of compatible CPU](https://ark.intel.com/content/www/us/en/ark/search/featurefilter.html?productType=873&2_SoftwareGuardExtensions=Yes%20with%20Intel%C2%AE%20ME)
*   [Installation of SGX Driver](https://sconedocs.github.io/sgxinstall/)

You can test if TorK is able to run in `Hardware Mode` by running the following command:

```
docker run --rm --privileged $MOUNT_SGXDEVICE -e SCONE_VERSION=1 -e SCONE_MODE=HW -it tork:sgx ./tork
```

You should obtain an output like this when you are able to run TorK inside an enclave (confirm that `SCONE_MODE=hw`):
```
export SCONE_QUEUES=4
export SCONE_SLOTS=256
export SCONE_SIGPIPE=0
export SCONE_MMAP32BIT=0
export SCONE_SSPINS=100
export SCONE_SSLEEP=4000
export SCONE_LOG=3
export SCONE_HEAP=67108864
export SCONE_STACK=2097152
export SCONE_CONFIG=/etc/sgx-musl.conf
export SCONE_ESPINS=10000
export SCONE_MODE=hw
export SCONE_ALLOW_DLOPEN=no
export SCONE_MPROTECT=no
musl version: 1.1.24
Revision: f4655f36e58c3f91fd4e085285c3a163a841e802 (Thu Apr 30 11:19:42 2020 +0000)
Branch: HEAD

Enclave hash: 92e69d133e2adb1de7f7cc4b0e96d1f01a272faed4cc7543cb7a476808105573
[TORK] Welcome to Tork!
usage: ./tork --mode=string --port=int [options] ...
options:
  -m, --mode               TorK mode operation (client/bridge) (string)
  -p, --port               Port to use in services (int)
  -s, --bridge_ssl_cert    Bridge SSL certificate path (string [=../certs/bridge_cert.pem])
  -x, --bridge_ssl_key     Bridge SSL private key path (string [=../certs/bridge_private.key])
  -?, --help               print this message
```

If you receive the following output:
```
[SCONE|ERROR] ./tools/starter-exec.c:1231:main(): Could not create enclave: Error opening SGX device
```
Make sure that the SGX drivers are properly installed and the container can access the sgx device (usually under `/dev/sgx/`).

##### Running TorK

Use the following commands to launch a bridge
and client with support for SGX enclaves:

```
docker run --rm --privileged --hostname=bridge --name=bridge --network=torknet $MOUNT_SGXDEVICE -v ~/tor/src/app/tor:/usr/local/bin/tor -it tork:sgx tor -f /usr/src/tork/tor_confs/torrc_docker_bridge_clean
```

```
docker run --rm --privileged --hostname=client1 --name=client1 --network=torknet $MOUNT_SGXDEVICE -v ~/tor/src/app/tor:/usr/local/bin/tor -p 9051:9051 -p 9091:9091 -it tork:sgx tor -f /usr/src/tork/tor_confs/torrc_docker_client_clean
```

### Using Vagrant
To deploy a TorK developing setup using vagrant, locate the `machine_setup`
under the `tork-analysis` git repo and run the following commands to deploy
one TorK bridge and two TorK clients:

```
vagrant up tork_bridge tork_client1 tork_client2
```

After a few minutes, the VMs will be ready. Just SSH into them and locate TorK
folder under the ´/home/vagrant/SharedFolder/`.

```
vagrant ssh tork_bridge
cd ~/SharedFolder/tork
tor -f tor_confs/torrc_bridge_clean
```

```
vagrant ssh tork_client1
cd ~/SharedFolder/tork
tor -f tor_confs/torrc_client_clean
```

The process of accessing and controlling the CLI is the same, as in Docker.
