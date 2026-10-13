#pragma once
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cerrno>
inline int testSocketRecv(int fd,char* buffer,int count,int flags) {
  int result=::recv(fd,buffer,count,flags);
  if(result==SOCKET_ERROR) {
    int error=WSAGetLastError();
    errno=error==WSAECONNRESET?ECONNRESET:error==WSAENOTCONN?ENOTCONN:error==WSAEWOULDBLOCK?EWOULDBLOCK:error;
  }
  return result;
}
#define recv testSocketRecv
// WinSock uses FIONBIO rather than a per-call MSG_DONTWAIT flag.
#define MSG_DONTWAIT 0
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#endif
