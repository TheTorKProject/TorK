#include "Common.hh"

/* ================================ Functions ============================= */

int readn(int fd, void *buf, int n)
{
    int nread, left = n;
    while (left > 0) {
        if ((nread = read(fd, buf, left)) == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return nread;
        } else {
            if (nread == 0) {
                return 0;
            } else {
                left -= nread;
                buf = (char*) buf + nread;
            }
        }
    }
    return n;
}

int writen(int fd, void *buf, int n)
{
    int nwrite, left = n;
    while (left > 0) {
        if ((nwrite = write(fd, buf, left)) == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return nwrite;
        } else {
            if (nwrite == n) {
                return n;
            } else {
                left -= nwrite;
                buf = (char*) buf + nwrite;
            }
        }
    }
    return n;
}