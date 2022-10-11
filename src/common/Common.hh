#ifndef COMMON_HH
#define COMMON_HH

#define RUN_FOREGROUND (0)
#define RUN_BACKGROUND (1)

/* ============================== Debug Options =========================== */

/* Enable or disable log verbose by category */
#define LOG_CONN         (0)
#define LOG_ERRORS       (0)
#define LOG_CLI          (0)
#define LOG_CTRL         (0)
#define LOG_CTRL_EVENTS  (0)
#define LOG_TR_SHAPER    (0)
#define LOG_CTRL_FRAMES  (0)
#define LOG_CTRL_LOCK    (0)
#define LOG_SSL          (0)

/* When LOG_CTRL_FRAMES = 1 only display verbose for specific ctrl frames */
#define LOG_CTRL_TYPE_NULL         (0)
#define LOG_CTRL_TYPE_HELLO        (0)
#define LOG_CTRL_TYPE_HELLO_OK     (0)
#define LOG_CTRL_TYPE_ACTIVE       (0)
#define LOG_CTRL_TYPE_WAIT         (0)
#define LOG_CTRL_TYPE_CHANGE       (0)
#define LOG_CTRL_TYPE_CHANGE_OK    (0)
#define LOG_CTRL_TYPE_INACTIVE     (0)
#define LOG_CTRL_TYPE_SHUT         (0)
#define LOG_CTRL_TYPE_SHUT_OK      (0)
#define LOG_CTRL_TYPE_TS_RATE      (0)

/* Enable bytes statistics logging */
#define STATS            (0)

/* Enable handler timing statistics. TIME_STATS > 0 is the number of samples
to store. */
#define TIME_STATS       (0)

/* Enable SYNC_DLV stats */
#define SYNC_DLV_STATS   (0)

/* Enable advanced debug tools */
#define DEBUG_TOOLS      (0)

/* ============================ Security Options ========================== */

/* Enable or disable SSL */
#define USE_SSL        (1)

/* If (1) bridges only deliver one DATA frame to Tor upon receiving one frame
from every client. */
#define DATA_FRAMES_SYNC_DLV (1)

/* =============================== Connections ============================ */

/* Maximum number of pending connections on the listen socket file descriptor.*/
#define MAX_LISTEN_USERS (50)

/* ============================ Handling Failures ========================= */

/* Maximum number of attempts to create a circuit. */
#define CIRC_RETRY_ATMPS  (5)

/* ================================== Versions ============================= */
#define TORK_VERSION "2.0.9.1"

#define TORK_BANNER "  ______           __ __\n /_  __/___  _____/ //_/\n\
  / / / __ \\/ ___/ ,<   \n / / / /_/ / /  / /| |  \n/_/  \\____/_/  /_/ |_|\n"

/* ================================ Log Bitmap ============================ */

#define LOG_BIT_ALL         ((1u<<9) - 1)
#define LOG_BIT_CONN        (1u<<0)
#define LOG_BIT_ERRORS      (1u<<1)
#define LOG_BIT_CLI         (1u<<2)
#define LOG_BIT_CTRL        (1u<<3)
#define LOG_BIT_CTRL_EVENTS (1u<<4)
#define LOG_BIT_TR_SHAPER   (1u<<5)
#define LOG_BIT_CTRL_FRAMES (1u<<6)
#define LOG_BIT_CTRL_LOCK   (1u<<7)
#define LOG_BIT_SSL         (1u<<8)
#define LOG_VERBOSE         (LOG_CONN | (LOG_ERRORS << 1) | (LOG_CLI << 2) |   \
                            (LOG_CTRL << 3) | (LOG_CTRL_EVENTS << 4)       |   \
                            (LOG_TR_SHAPER << 5) | (LOG_CTRL_FRAMES << 6)  |   \
                            (LOG_CTRL_LOCK << 7) | (LOG_SSL << 8))

#define LOG_BIT_TYPE_NULL       (1u<<0)
#define LOG_BIT_TYPE_HELLO      (1u<<1)
#define LOG_BIT_TYPE_HELLO_OK   (1u<<2)
#define LOG_BIT_TYPE_ACTIVE     (1u<<3)
#define LOG_BIT_TYPE_WAIT       (1u<<4)
#define LOG_BIT_TYPE_CHANGE     (1u<<5)
#define LOG_BIT_TYPE_CHANGE_OK  (1u<<6)
#define LOG_BIT_TYPE_INACTIVE   (1u<<7)
#define LOG_BIT_TYPE_SHUT       (1u<<8)
#define LOG_BIT_TYPE_SHUT_OK    (1u<<9)
#define LOG_BIT_TYPE_TS_RATE    (1u<<10)
#define LOG_CTRL_TYPES          (LOG_CTRL_TYPE_NULL             |  \
                                 (LOG_CTRL_TYPE_HELLO     << 1) |  \
                                 (LOG_CTRL_TYPE_HELLO_OK  << 2) |  \
                                 (LOG_CTRL_TYPE_ACTIVE    << 3) |  \
                                 (LOG_CTRL_TYPE_WAIT      << 4) |  \
                                 (LOG_CTRL_TYPE_CHANGE    << 5) |  \
                                 (LOG_CTRL_TYPE_CHANGE_OK << 6) |  \
                                 (LOG_CTRL_TYPE_INACTIVE  << 7) |  \
                                 (LOG_CTRL_TYPE_SHUT      << 8) |  \
                                 (LOG_CTRL_TYPE_SHUT_OK   << 9) |  \
                                 (LOG_CTRL_TYPE_TS_RATE   << 10))

/* ================================ Libraries ============================= */

#include <assert.h>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include <shared_mutex>
#include <condition_variable>

#if USE_SSL
    #include  <openssl/bio.h>
    #include  <openssl/ssl.h>
    #include  <openssl/err.h>
#endif

/* Error code for SSL_WANT_READ or SSL_WANT_WRITE */
#define SSL_TRY_LATER   (-2)

/* ================================ Functions ============================= */

int readn(int fd, void *buf, int n);

int writen(int fd, void *buf, int n);

#endif //COMMON_HH
