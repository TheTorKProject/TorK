# +-----------------------------------------------+
# | TorK Development - Docker image configuration |
# +-----------------------------------------------+
FROM ubuntu:bionic
RUN apt update && apt upgrade -y && apt install -y build-essential gcc g++ gdb
RUN apt install -y git libssl-dev tcpdump dnsutils curl tmux netcat-openbsd
RUN apt install -y libboost-all-dev

# Install latest cmake
RUN apt install -y apt-transport-https ca-certificates gnupg software-properties-common wget
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'
RUN apt install -y cmake

#Install Tor from source
RUN apt install -y autotools-dev automake libevent-dev
RUN git clone https://git.torproject.org/tor.git /usr/src/tor
WORKDIR /usr/src/tor
RUN git checkout tags/tor-0.4.2.8
RUN sed -i "s/entry_guards_note_guard_failure(guard->in_selection, guard);/\/\/entry_guards_note_guard_failure(guard->in_selection, guard);/g" /usr/src/tor/src/feature/client/entrynodes.c
RUN sh autogen.sh
RUN ./configure --disable-asciidoc
RUN make
RUN make install

RUN mkdir ~/tor
