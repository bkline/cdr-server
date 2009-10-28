/*
 * $Id: CdrTestClient.cpp,v 1.10 2009-09-25 04:16:28 ameyer Exp $
 *
 * Test client (C++ version) for sending commands to CDR server.
 *
 * Usage:
 *  CdrTestClient [command-file [host [port]]]
 *
 * Example:
 *  CdrTestClient CdrCommandSamples.xml mmdb2
 *
 * Default for host is "localhost"; default for port is 2019.
 * If no command-line arguments are given, commands are read from standard
 * input.  Command buffer must be valid XML, conforming to the DTD
 * CdrCommandSet.dtd, and the top-level element must be <CdrCommandSet>.
 * The encoding for the XML must be UTF-8.
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.9  2008/10/03 23:54:05  bkline
 * Fixing problems flushed out by Visual Studio 2008.
 *
 * Revision 1.8  2004/03/31 03:29:55  ameyer
 * Added new parameter to set_exception_catcher.
 *
 * Revision 1.7  2004/03/17 20:58:28  bkline
 * Fixed setmode() call.
 *
 * Revision 1.6  2001/05/03 18:43:11  bkline
 * Used binary mode for file I/O.
 *
 * Revision 1.5  2001/01/18 14:57:05  bkline
 * Removed unused count variable.
 *
 * Revision 1.4  2001/01/18 14:51:50  bkline
 * Make "Server response:" header into a comment.
 *
 * Revision 1.3  2000/08/24 20:08:21  ameyer
 * Added NT structured exception handling for crashes.
 *
 * Revision 1.2  2000/04/17 03:16:12  bkline
 * Allowed "-" to mean standard input on command line.
 *
 * Revision 1.1  2000/04/15 14:10:50  bkline
 * Initial revision
 */

// System headers.
#include <winsock.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include "catchexp.h"

// Local constants.
const short CDR_PORT = 2019;
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#define setmode _setmode
#endif

// Local functions.
static std::string  readFile(std::istream&);
static void         cleanup() { WSACleanup(); }

int main(int ac, char **av)
{
    WSAData             wsadata;
    char *              host;
    int                 sock;
    long                address;
    struct sockaddr_in  addr;
    struct hostent *    ph;
    std::string         requests;

    // In case of catastrophe, don't hang up on console
    // But do abort
    if (!getenv ("NOCATCHCRASH"))
        set_exception_catcher ("CdrTestClient.crash", 1);

    /*
    // Usage
    if (ac > 1 && av[1][0] == '-' || av[1][0] == '/') {
        if (av[1][1] == '?' || av[1][1] == 'h' || av[1][0] == 'H') {
            std::cerr << "Submit commands to a CdrServer\n"
                      << "usage: " << av[0]
                      << " [command-file [host [port]]]"
                      << std::endl;
            exit(1);
        }
    }
    */

    // Load the requests.
    if (ac > 1 && strcmp(av[1], "-")) {
        std::ifstream is(av[1], std::ios::binary | std::ios::in);
        if (!is) {
            std::cerr << av[1] << ": " << strerror(errno) << '\n';
            return EXIT_FAILURE;
        }
        requests = readFile(is);
    }
    else {
        setmode(fileno(stdin), O_BINARY);
        requests = readFile(std::cin);
    }

    // Initialize socket I/O.
    if (WSAStartup(0x0101, &wsadata) != 0) {
        std::cerr << "WSAStartup: " << WSAGetLastError() << '\n';
        return EXIT_FAILURE;
    }
    atexit(cleanup);

    // Connect a socket to the server.
    host = ac > 2 ? av[2] : "localhost";
    if (isdigit(host[0])) {
        if ((address = inet_addr(host)) == -1) {
            std::cerr << "invalid host name " << host << '\n';
            return EXIT_FAILURE;
        }
        addr.sin_addr.s_addr = address;
        addr.sin_family = AF_INET;
    }
    else {
        ph = gethostbyname(host);
        if (!ph) {
            perror("gethostbyname");
            return EXIT_FAILURE;
        }
        addr.sin_family = ph->h_addrtype;
        memcpy(&addr.sin_addr, ph->h_addr, ph->h_length);
    }
    addr.sin_port = htons(ac > 3 ? atoi(av[3]) : CDR_PORT);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }
    if (connect(sock, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("connect");
        return EXIT_FAILURE;
    }

    // Send the commands to the server.
    unsigned long bytes = requests.size();
    unsigned long length = htonl(bytes);
    if (send(sock, (char *)&length, sizeof length, 0) < 0) {
        std::cerr << "send error: " << GetLastError() << '\n';
        return EXIT_FAILURE;
    }
    if (send(sock, requests.c_str(), bytes, 0) < 0) {
        std::cerr << "send error: " << GetLastError() << '\n';
        return EXIT_FAILURE;
    }

    // Determine the number of bytes in the server's reply.
    size_t totalRead = 0;
    char lengthBytes[4];
    while (totalRead < sizeof lengthBytes) {
        int leftToRead = sizeof lengthBytes - totalRead;
        int bytesRead = recv(sock, lengthBytes + totalRead, leftToRead, 0);
        if (bytesRead < 0) {
            std::cerr << "recv error: " << GetLastError() << '\n';
            return EXIT_FAILURE;
        }
        totalRead += bytesRead;
    }
    memcpy(&length, lengthBytes, sizeof length);
    length = ntohl(length);

    // Catch the server's response.
    char *response = new char[length + 1];
    memset(response, 0, length + 1);
    totalRead = 0;
    while (totalRead < length) {
        int leftToRead = length - totalRead;
        int bytesRead = recv(sock, response + totalRead, leftToRead, 0);
        if (bytesRead < 0) {
            std::cerr << "recv error: " << GetLastError() << '\n';
            return EXIT_FAILURE;
        }
        totalRead += bytesRead;
    }
    setmode(fileno(stdout), O_BINARY);
    std::cout << "<!-- Server response: -->\n" << response << '\n';
    return EXIT_SUCCESS;
}

std::string  readFile(std::istream& is)
{
    int nBytes = 0;
    char *bytes = new char[nBytes];
    char buf[4096];
    while (is) {
        is.read(buf, sizeof buf);
        int n = is.gcount();
        if (n > 0) {
            char *tmp = new char[nBytes + n];
            if (nBytes > 0)
                memcpy(tmp, bytes, nBytes);
            memcpy(tmp + nBytes, buf, n);
            delete [] bytes;
            bytes = tmp;
            nBytes += n;
        }
    }
    return std::string(bytes, nBytes);
}