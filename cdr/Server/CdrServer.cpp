/*
 * $Id: CdrServer.cpp,v 1.32 2002-08-12 15:48:11 bkline Exp $
 *
 * Server for ICIC Central Database Repository (CDR).
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.31  2002/08/10 20:18:46  bkline
 * More thread logging.
 *
 * Revision 1.30  2002/08/10 19:28:22  bkline
 * Guest session support; better hacker protection; message logging.
 *
 * Revision 1.29  2002/07/11 18:56:19  ameyer
 * Added directory path for exception catcher crash log.
 *
 * Revision 1.28  2002/06/16 03:02:43  bkline
 * Sidestep 'helpful' attacks from NIH network administration.
 *
 * Revision 1.27  2002/03/09 04:21:35  bkline
 * Added some #ifdef _DEBUG fences.
 *
 * Revision 1.26  2002/03/07 12:58:22  bkline
 * Added more tracing to standard output, as well as conditional memory
 * allocation dumps at the bottom of each time through the main loop.
 *
 * Revision 1.25  2002/03/06 20:33:28  bkline
 * Made catch argument a reference instead of an object.  Removed redundant
 * heap trace information.
 *
 * Revision 1.24  2002/02/27 23:33:09  bkline
 * Added test of buffer allocation for new client message.
 *
 * Revision 1.23  2002/02/01 20:48:21  bkline
 * Added more logging for top-level failures.
 *
 * Revision 1.22  2002/01/28 23:10:36  bkline
 * Added handler for unexpected exception when processing a single command.
 *
 * Revision 1.21  2001/12/14 18:28:46  bkline
 * Added use of heap debugging conditional macros.
 *
 * Revision 1.20  2001/12/14 15:18:14  bkline
 * Made code friendlier for optional heap debugging.
 *
 * Revision 1.18  2001/09/19 18:48:22  bkline
 * Added support for timing processing time for commands.
 *
 * Revision 1.17  2001/05/21 20:31:58  bkline
 * Fixed typo (missing right angle bracket for element closing tag).
 *
 * Revision 1.16  2000/10/04 18:31:36  bkline
 * Added code to catch more exception types.
 *
 * Revision 1.15  2000/08/24 20:07:33  ameyer
 * Added NT structured exception handling for crashes.
 *
 * Revision 1.14  2000/06/23 15:29:01  bkline
 * Added additional logging (for startup and shutdown).
 *
 * Revision 1.13  2000/06/15 23:02:20  ameyer
 * Modified logging in processCommand().
 *
 * Revision 1.12  2000/06/09 04:00:09  ameyer
 * Added logging.
 *
 * Revision 1.11  2000/06/09 00:46:36  bkline
 * Replaced hardwired database logon strings with named constants.
 *
 * Revision 1.10  2000/06/02 20:56:01  bkline
 * Fixed typo in progress message reporting clearing of inactive sessions.
 *
 * Revision 1.9  2000/06/01 18:49:57  bkline
 * Removed some debugging output.
 *
 * Revision 1.8  2000/05/21 00:52:15  bkline
 * Added CdrShutdown command support and sweep for stale sessions.
 *
 * Revision 1.7  2000/05/09 20:15:34  bkline
 * Replaced direct reading of Session fields with accessor methods.
 *
 * Revision 1.6  2000/05/04 12:45:25  bkline
 * Changed getString() to what() for cdr::Exceptions.
 *
 * Revision 1.5  2000/05/03 15:24:34  bkline
 * Added timestamp for command batch response.
 *
 * Revision 1.4  2000/04/22 09:33:56  bkline
 * Added transaction support, calling rollback() in exception handlers.
 *
 * Revision 1.3  2000/04/16 21:55:48  bkline
 * Fixed code for getting sessionId.  Added calls to lookupSession() and
 * setLastActivity().  Added code to set Status attribute in <CdrResponse>
 * element.
 *
 * Revision 1.2  2000/04/15 14:12:07  bkline
 * Added conditional Sleep() for 0-byte recv.  Replaced cdr::DbConnection*
 * with cdr::db::Connection&.  Catch added for exception thrown by
 * command implementation.
 *
 * Revision 1.1  2000/04/13 17:08:44  bkline
 * Initial revision
 *
 */

// System headers.
#include <winsock.h>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <ctime>
#include <process.h>

// Project headers.
#include "catchexp.h"
#include "CdrCommand.h"
#include "CdrSession.h"
#include "CdrDom.h"
#include "CdrDbConnection.h"
#include "CdrException.h"
#include "CdrDbStatement.h"
#include "HeapDebug.h"

// Local constants.
const short CDR_PORT = 2019;
const int   CDR_QUEUE_SIZE = 10;
const unsigned int MAX_REQUEST_LENGTH = 25000000;

// Local functions.
static void             cleanup();
static int              readRequest(int, std::string&, const cdr::String&);
static void             sendResponse(int, const cdr::String&);
static void             processCommands(int, const std::string&,
                                        cdr::db::Connection&,
                                        const cdr::String&);
static cdr::String      processCommand(cdr::Session&,
                                       const cdr::dom::Node&,
                                       cdr::db::Connection&,
                                       const cdr::String&);
static cdr::String      getElapsedTime(DWORD);
static void             sendErrorResponse(int, const cdr::String&,
                                          const cdr::String&);
static void __cdecl     dispatcher(void*);
static void             realDispatcher(void* arg);
static void __cdecl     sessionSweep(void*);
static int              handleNextClient(int sock);
static void             logTopLevelFailure(const cdr::String what,
                                           unsigned long code);
static bool             timeToShutdown = false;
static cdr::log::Log    log;

/**
 * Creates a socket and listens for connections on it.  Spawns
 * a new thread to handle each incoming connection.
 */
main(int ac, char **av)
{
    WSAData             wsadata;
    int                 sock;
    struct sockaddr_in  addr;
    short               port = CDR_PORT;

    if (ac > 1) {
        port = atoi(av[1]);
        SET_HEAP_DEBUGGING(true);
    }

    // In case of catastrophe, don't hang up on console
    if (!getenv ("NOCATCHCRASH"))
        set_exception_catcher ("d:/cdr/log/CdrServer.crash");

    if (WSAStartup(0x0101, &wsadata) != 0) {
        int err = WSAGetLastError();
        std::cerr << "WSAStartup: " << err << '\n';
        logTopLevelFailure(L"WSAStartup", (unsigned long)err);
        return EXIT_FAILURE;
    }
    atexit(cleanup);
    std::cout << "initialized...\n";
    log.Write("CdrServer", "Starting");

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        logTopLevelFailure(L"socket", (unsigned long)WSAGetLastError());
        return EXIT_FAILURE;
    }
    std::cout << "socket created...\n";
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&addr, sizeof addr) == SOCKET_ERROR) {
        perror("bind");
        logTopLevelFailure(L"bind", (unsigned long)WSAGetLastError());
        return EXIT_FAILURE;
    }
    std::cout << "bound...\n";
    if (listen(sock, CDR_QUEUE_SIZE) == SOCKET_ERROR) {
        perror("listen");
        logTopLevelFailure(L"listen", (unsigned long)WSAGetLastError());
        return EXIT_FAILURE;
    }
    std::cout << "listening...\n";

#ifndef SINGLE_THREAD_DEBUGGING
    std::cout << "sweeping for expired sessions...\n";
    if (_beginthread(sessionSweep, 0, (void*)0) == -1) {
        std::cerr << "_beginthread: " << GetLastError() << '\n';
        logTopLevelFailure(L"listen", (unsigned long)WSAGetLastError());
        return EXIT_FAILURE;
    }
#else
    std::cout << "running single-threaded for debugging...\n";
#endif

    while (!timeToShutdown) {
        try {
            MEM_START();
            int rc = handleNextClient(sock);
            if (rc != EXIT_SUCCESS)
                return rc;
            SHOW_HEAP_USED("Bottom of main processing loop");
            MEM_REPORT();
        }
        catch (...) {
            logTopLevelFailure(L"main processing loop exception",
                    GetLastError());
        }
    }
    return EXIT_SUCCESS;
}

/**
 * Meat of main processing loop, broken out to make stack cleanup
 * easier to track during heapdebuggin.
 */
int handleNextClient(int sock)
{
    struct sockaddr_in client_addr;
    int len = sizeof client_addr;
    memset(&client_addr, 0, sizeof client_addr);
    int fd = accept(sock, (struct sockaddr *)&client_addr, &len);
    if (timeToShutdown)
        return EXIT_SUCCESS;
    if (fd < 0) {
        perror("accept");
        logTopLevelFailure(L"accept", (unsigned long)WSAGetLastError());
        return EXIT_FAILURE;
    }
#ifndef SINGLE_THREAD_DEBUGGING
    int tries = 5;
    while (_beginthread(dispatcher, 0, (void*)fd) == -1) {
        DWORD err = errno; // GetLastError();
        std::cerr << "_beginthread: " << err << '\n';
        if (tries-- <= 0) {
            logTopLevelFailure(L"_beginthread", err);
            closesocket(fd);
            return EXIT_FAILURE;
        }
        Sleep(1000);
    }
#else
    // Use this version (and turn off the invocation of the session cleanup
    // thread below) when you want to debug without dealing with multiple
    // threads.
    realDispatcher((void*)fd);
#endif
    return EXIT_SUCCESS;
}

/**
 * Logs server shutdown and cleans up winsock resources.
 */
void cleanup()
{
    log.Write("CdrServer", "Stopping");
    WSACleanup();
}

/**
 * Implements thread processing to handle a new connection.  Capable
 * of handling multiple command sets for the connection.
 */
void realDispatcher(void* arg) {
    cdr::db::Connection conn =
        cdr::db::DriverManager::getConnection(cdr::db::url,
                                              cdr::db::uid,
                                              cdr::db::pwd);
    cdr::String now = conn.getDateTimeString();
    now[10] = L'T';
#ifdef _DEBUG
    std::wcout << L"NOW=" << now << L"\n";
#endif

    // Create thread specific log pointer
    // Done early in thread creation so anything in the thread can
    //   log whatever it wants to.
    cdr::log::pThreadLog = new cdr::log::Log;
    cdr::String threadId = cdr::String::toString(GetCurrentThreadId());
    cdr::log::pThreadLog->Write(L"Thread Starting", threadId);

    int fd = (int)arg;
    std::string request;
    int nBytes;
    int response = 0;
    try {
        while ((nBytes = readRequest(fd, request, now)) > 0) {
#ifdef _DEBUG
            std::cout << "received request with " << nBytes << " bytes...\n";
#endif
            processCommands(fd, request, conn, now);
        }
    }
    catch (...) {
        cdr::log::pThreadLog->Write (L"realDispatcher", L"Exception caught");
    }

    // Thread is about to go, done with thread specific log
    cdr::log::pThreadLog->Write(L"Thread Stopping", threadId);
    delete cdr::log::pThreadLog;
    closesocket(fd);
}

/**
 * Do the real work in a separate function to force Microsoft to clean
 * up the stack.
 */
void __cdecl dispatcher(void* arg) {
    realDispatcher(arg);
    _endthread();
}

size_t readBytes(int fd, size_t requested, char* buf)
{
    // Keep reading until we have all the bytes, an error occurs, or we give
    // up after getting no bytes, sleeping for awhile, and then trying again.
    size_t totalRead = 0;
    bool canSleep = true;
    while (totalRead < requested) {
        size_t bytesLeft = requested - totalRead;
        int nRead = recv(fd, buf + totalRead, bytesLeft, 0);
        if (nRead < 0)
            return 0;
        if (nRead == 0) {
            if (canSleep) {
                Sleep(500);
                canSleep = false;
            }
            else 
                return totalRead;
        }
        else
            canSleep = true;
        totalRead += nRead;
    }
    return totalRead;
}

/**
 * Reads the 4-byte count of bytes in the command transmission,
 * then reads those bytes into the caller's string object.  Length
 * prefix must be sent in network byte order (most significant bytes
 * first).
 */
int readRequest(int fd, std::string& request, const cdr::String& when) {

    // Determine the length of the command buffer.
    unsigned long length;
    char lengthBytes[4];
    size_t totalRead = readBytes(fd, sizeof lengthBytes, lengthBytes);
    if (totalRead != sizeof lengthBytes)
        return 0;
    memcpy(&length, lengthBytes, sizeof length);
    length = ntohl(length);
    std::cout << "getting " << length << " bytes\n";

    // Avoid bogus requests from attackers (the only attacks we have had
    // so far are from NIH network administration software).
    if (length > MAX_REQUEST_LENGTH || length == 0) {
        cdr::log::pThreadLog->Write (L"readRequest", 
                L"refusing " +
                cdr::String::toString(length) +
                L"-byte request");
        return 0;
    }

    // Check the first bytes for expected substring.
    char firstBytes[4097];
    memset(firstBytes, 0, sizeof firstBytes);
    size_t nFirstBytes = sizeof firstBytes - 1;
    if (nFirstBytes > length)
        nFirstBytes = length;
    totalRead = readBytes(fd, nFirstBytes, firstBytes);
    if (totalRead < nFirstBytes)
        return 0;
    if (!strstr(firstBytes, "<CdrCommandSet")) {
        cdr::log::pThreadLog->Write (L"readRequest", 
                L"refusing bogus request string: " + cdr::String(firstBytes));
        return 0;
    }

    // See if we need to read any more.
    if (totalRead == length)
        request = std::string(firstBytes, totalRead);
    else {

        // Allocate a working buffer.
        char *buf = new char[length + 1];
        if (!buf) {
            char tmp[256];
            sprintf(tmp, "Failure allocating %lu bytes", length);
            sendErrorResponse(fd, tmp, when);
            return 0;
        }
        memset(buf, 0, length + 1);
        memcpy(buf, firstBytes, totalRead);
        totalRead += readBytes(fd, length - totalRead, buf + totalRead);
        if (totalRead == length)
            request = std::string(buf, totalRead);
        delete [] buf;
        if (totalRead != length)
            return 0;
    }
    return totalRead;
}

#ifdef _DEBUG
void logCommand(cdr::db::Connection& conn, const std::string& buf)
{
    try {
        cdr::db::PreparedStatement s = conn.prepareStatement(
                " INSERT INTO command_log (thread, received, command) "
                "      VALUES (?, GETDATE(), ?)                       ");
        s.setInt(1, cdr::log::pThreadLog->GetId());
        s.setString(2, buf);
        s.executeUpdate();
    }
    catch (...) {}
}
#endif

/**
 * Parses command set buffer, extracts each command and has it
 * processed.  Wraps all the responses in a buffer, which is returned
 * to the client.
 */
void processCommands(int fd, const std::string& buf,
                     cdr::db::Connection& conn,
                     const cdr::String& when)
{
#ifdef _DEBUG
    logCommand(conn, buf);
#endif
    try {
        cdr::dom::Parser parser;
        parser.parse(buf);
        cdr::dom::Document document = parser.getDocument();
        if (document == 0) {
            sendErrorResponse(fd, "Failure parsing command buffer", when);
            return;
        }
        cdr::dom::Element docElement = document.getDocumentElement();
        if (docElement == 0) {
            sendErrorResponse(fd, "Failure parsing command buffer", when);
            return;
        }
        cdr::String elementName = docElement.getNodeName();
        if (elementName != L"CdrCommandSet") {
            sendErrorResponse(fd, "Top element must be CdrCommandSet", when);
            return;
        }
        cdr::Session session;
        cdr::String response = L"<CdrResponseSet Time='" + when + L"'>\n";
        cdr::dom::Node n = docElement.getFirstChild();
        while (n != 0) {
            if (n.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
                elementName = n.getNodeName();
                if (elementName == L"SessionId") {
                    cdr::String sessionId = cdr::dom::getTextContent(n);
                    //std::wcerr << L"sessionId=" << sessionId << L"\n";
                    session.lookupSession(sessionId, conn);
                    session.setLastActivity(conn);
                }
                else if (elementName == L"CdrCommand")
                    response += processCommand(session, n, conn, when);
            }
            n = n.getNextSibling();
        }
        response += L"</CdrResponseSet>\n";
        sendResponse(fd, response);
    }
    catch (const cdr::Exception& cdrEx) {
        if (!conn.getAutoCommit())
            conn.rollback();
        sendErrorResponse(fd, cdrEx.what(), when);
    }
    catch (const cdr::dom::XMLException& ex) {
        if (!conn.getAutoCommit())
            conn.rollback();
        sendErrorResponse(fd, cdr::String(ex.getMessage()), when);
    }
    catch (...) {
        if (!conn.getAutoCommit())
            conn.rollback();
        sendErrorResponse(fd, L"Unexpected exception caught", when);
    }
}

/**
 * Sends a response buffer to the client reporting a general error
 * which prevents processing of any of the commands in the command
 * buffer.
 */
void sendErrorResponse(int fd, const cdr::String& errMsg,
                       const cdr::String& when)
{
    cdr::String response = L"<CdrResponseSet Time='"
                         + when
                         + L"'>\n <Errors>\n  <Err>"
                         + errMsg
                         + L"</Err>\n </Errors>\n</CdrResponseSet>\n";
    sendResponse(fd, response);
}

/**
 * Finds the command handler responsible for the current command and
 * invokes it.
 */
cdr::String processCommand(cdr::Session& session,
                           const cdr::dom::Node& cmdNode,
                           cdr::db::Connection& conn,
                           const cdr::String& when)
{
    DWORD start = GetTickCount();
    const cdr::dom::Element& cmdElement = static_cast<const cdr::dom::Element&>
        (cmdNode);
    cdr::String cmdId = cmdElement.getAttribute(L"CmdId");
    cdr::String rspTag;
    if (cmdId.size() > 0)
        rspTag = L" <CdrResponse CmdId='" + cmdId + L"' Status='";
    else
        rspTag = L" <CdrResponse Status='";
    cdr::dom::Node specificCmd = cmdNode.getFirstChild();
    cdr::String elapsedTime;
    while (specificCmd != 0) {
        int type = specificCmd.getNodeType();
        if (type == cdr::dom::Node::ELEMENT_NODE) {
            cdr::String cmdName = specificCmd.getNodeName();
#ifdef _DEBUG
            std::wcout << L"processing command: " << cmdName << L"...\n";
#endif

            // Log info about the command
            cdr::String cmdText = L"Cmd: " + cmdName + L"  User: "
                                + session.getUserName();

            cdr::log::pThreadLog->Write (L"processCommand", cmdText);

            cdr::Command cdrCommand = cdr::lookupCommand(cmdName);
            if (!cdrCommand) {
                elapsedTime = getElapsedTime(start);
                return cdr::String(rspTag + L"failure' Elapsed='"
                                          + elapsedTime
                                          + L"'>\n  <Errors>\n   "
                                          + L"<Err>Unknown command: "
                                          + cmdName
                                          + L"</Err>\n  </Errors>\n"
                                          + L" </CdrResponse>\n");
            }
            cdr::String cmdResponse;
            try {
                /*
                 * Only way you can get in the door without a valid session
                 * is with a logon command.
                 */
                if (cdrCommand != cdr::logon && !session.isOpen()) {
                    elapsedTime = getElapsedTime(start);
                    return cdr::String(rspTag + L"failure' Elapsed='"
                                              + elapsedTime
                                              + L"'>\n  <"
                                              + cmdName
                                              + L"Resp>\n"
                                              + L"   <Errors>\n    <Err>"
                                              + L"Missing session ID"
                                              + L"</Err>\n   </Errors>\n"
                                              + L"  </"
                                              + cmdName
                                              + L"Resp>\n </CdrResponse>\n");
                }

                // Optimistic assumption.
                session.setStatus(L"success");
                conn.setAutoCommit(true);
                cmdResponse = cdrCommand(session, specificCmd, conn);
            }
            catch (const cdr::Exception& e) {
                if (!conn.getAutoCommit())
                    conn.rollback();

                // Exception text already logged in exception constructor
                elapsedTime = getElapsedTime(start);
                return cdr::String(rspTag + L"failure' Elapsed='"
                                          + elapsedTime
                                          + L"'>\n  <"
                                          + cmdName
                                          + L"Resp>\n"
                                          + L"   <Errors>\n    <Err>"
                                          + e.what()
                                          + L"</Err>\n   </Errors>\n"
                                          + L"  </"
                                          + cmdName
                                          + L"Resp>\n </CdrResponse>\n");
            }
            catch (cdr::dom::DOMException* de) {
                if (!conn.getAutoCommit())
                    conn.rollback();
                wchar_t tBuf[40];
                swprintf(tBuf, L"DOM Exception Code %d: ", de->code);
                elapsedTime = getElapsedTime(start);
                cdr::String result =
                       cdr::String(rspTag + L"failure' Elapsed='"
                                          + elapsedTime
                                          + L"'><"
                                          + cmdName
                                          + L"Resp><Errors><Err>"
                                          + cdr::String(tBuf)
                                          + cdr::String(de->msg)
                                          + L"</Err></Errors></"
                                          + cmdName
                                          + L"Resp></CdrResponse>");
                delete de;
                return result;
            }
            catch (const cdr::dom::SAXParseException& spe) {
                if (!conn.getAutoCommit())
                    conn.rollback();
                wchar_t tmpBuf[80];
                swprintf(tmpBuf, L" at line %d, Column %d",
                         spe.getLineNumber(), spe.getColumnNumber);
                cdr::String locString = tmpBuf;
                elapsedTime = getElapsedTime(start);
                return cdr::String(rspTag + L"failure' Elapsed='"
                                          + elapsedTime
                                          + L"'><"
                                          + cmdName
                                          + L"Resp><Errors>"
                                          + L"<Err>SAX Parse Exception: "
                                          + spe.getMessage()
                                          + locString
                                          + L"</Err></Errors></"
                                          + cmdName
                                          + L"Resp></CdrResponse>");
            }
            catch (const cdr::dom::SAXException& se) {
                if (!conn.getAutoCommit())
                    conn.rollback();
                elapsedTime = getElapsedTime(start);
                return cdr::String(rspTag + L"failure' Elapsed='"
                                          + elapsedTime
                                          + L"'><"
                                          + cmdName
                                          + L"Resp><Errors><Err>SAX Exception: "
                                          + se.getMessage()
                                          + L"</Err></Errors></"
                                          + cmdName
                                          + L"Resp></CdrResponse>");
            }
            catch (...) {
                if (!conn.getAutoCommit())
                    conn.rollback();
                elapsedTime = getElapsedTime(start);
                return cdr::String(rspTag + L"failure' Elapsed='"
                                          + elapsedTime
                                          + L"'><"
                                          + cmdName
                                          + L"Resp><Errors><Err>Unexpected "
                                            L"exception caught."
                                            L"</Err></Errors></"
                                          + cmdName
                                          + L"Resp></CdrResponse>");
            }
            elapsedTime = getElapsedTime(start);
            cdr::String response = rspTag + session.getStatus()
                                          + L"' Elapsed='"
                                          + elapsedTime
                                          + L"'>\n"
                                          + cmdResponse
                                          + L" </CdrResponse>\n";
            if (cmdName == L"CdrShutdown")
                timeToShutdown = true;

            cdr::log::pThreadLog->Write (L"processCommand result",
                                         session.getStatus());

            return response;
        }
        specificCmd = specificCmd.getNextSibling();
    }
    return cdr::String(rspTag + L"failure' Elapsed='"
                              + elapsedTime
                              + L"'>\n  <Errors>\n   "
                              + L"<Err>Missing specific command element"
                              + L"</Err>\n  </Errors>\n"
                              + L" </CdrResponse>\n");
}

/**
 * Sends the client the count of bytes in the response buffer (using
 * a network-order 4-byte integer), followed by the contents of the
 * buffer.
 */
void sendResponse(int fd, const cdr::String& response)
{
    std::string s = response.toUtf8();
    unsigned long bytes  = s.size();
    unsigned long length = htonl(bytes);
    send(fd, (char *)&length, sizeof length, 0);
    send(fd, s.c_str(), bytes, 0);
}

/**
 * Cleans up stale sessions (those which have been inactive for 24 hours or
 * longer).
 */
void __cdecl sessionSweep(void* arg) {

    const char* query = "UPDATE session"
                        "   SET ended = GETDATE()"
                        " WHERE ended IS NULL"
                        "   AND name <> 'guest'"
                        "   AND DATEDIFF(hour, last_act, GETDATE()) > 24";
    int counter = 0;
    cdr::log::Log log;
#ifdef LOGSWEEP
    FILE *fp = fopen("sweep.log", "a");
    if (fp) {
        time_t now = time(0);
        fprintf(fp, "sweep starting at %s", ctime(&now));
        fclose(fp);
    }
#endif
    while (!timeToShutdown) {
        try {
            if (counter++ % 60 == 0) {
                cdr::db::Connection conn =
                    cdr::db::DriverManager::getConnection(cdr::db::url,
                                                          cdr::db::uid,
                                                          cdr::db::pwd);
                cdr::db::Statement s = conn.createStatement();
                int rows = s.executeUpdate(query);
#ifdef LOGSWEEP
                fp = fopen("sweep.log", "a");
                if (fp) {
                    time_t now = time(0);
                    fprintf(fp, "sweep found %d rows at %s", rows, ctime(&now));
                    fclose(fp);
                }
#endif
                if (rows > 0) {
                    cdr::String now = conn.getDateTimeString();
                    now[10] = L'T';
                    std::wcerr << L"*** CLEARED " << rows
                               << L" INACTIVE SESSIONS AT " << now << L"\n";
                }
            }
            Sleep(5000);
        }
        catch (cdr::Exception& e) {
            log.Write("sessionSweep", e.what());
        }
        catch (...) {
            log.Write("sessionSweep", "unknown exception");
        }
    }
    _endthread();
}

/**
 * Returns a string containing the number of seconds elapsed since the start.
 */
cdr::String getElapsedTime(DWORD start)
{
    DWORD now = GetTickCount();
    DWORD millis = now - start; // OK if it wraps around.
    wchar_t buf[80];
    swprintf(buf, L"%lu.%03lu", millis / 1000L, millis % 1000L);
    return buf;
}

/**
 * Logs an error which occurs outside of a specific thread.
 */
void logTopLevelFailure(const cdr::String what, unsigned long code)
{
    wchar_t buf[80];
    swprintf(buf, L"%lu", code);
    cdr::log::WriteFile(what, buf);
}
