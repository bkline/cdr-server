/*
 * $Id: CdrLog.cpp,v 1.2 2000-10-05 17:23:21 ameyer Exp $
 *
 * Implementation of writing info to the log table in the database.
 * If that can't be done, takes an alternative action to write to file.
 *
 *                                          Alan Meyer  June, 2000
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.1  2000/06/15 22:32:24  ameyer
 * Initial revision
 *
 */

#include <cstdlib>
#include <iostream>
#include <wchar.h>
#include <time.h>
#include "CdrString.h"
#include "CdrDbConnection.h"
#include "CdrDbPreparedStatement.h"
#include "CdrDbStatement.h"
#include "CdrDbResultSet.h"
#include "CdrLog.h"


/**
 * Thread global pointer to thread specific instance of a log object.
 * Microsoft specific approach to creating a variable in a single thread.
 *
 * This global variable makes it possible for any function, at any level
 * below the point where the pointer has been initialized (in
 * CdrServer.cpp - dispatcher()) to write the log table.
 *
 * The variable is thread specific so that we can write a thread identifier
 * to the log table.  Using this a programmer can read entries in the
 * log table which have been interleaved from multiple concurrent threads
 * and find entries from the same thread.
 */
cdr::log::Log __declspec(thread) * cdr::log::pThreadLog;

// Class static variables
int    cdr::log::Log::s_LogId     = 0;
HANDLE cdr::log::Log::s_hLogMutex = 0;

/**
 * Constructor.
 *   Creates object suitable for use anywhere in the current thread.
 *   Creates a unique thread identifier to stamp all messages written
 *      from this thread.
 */

cdr::log::Log::Log ()
{
    cdr::String ErrMsg = L"";        // For use in case of mutex error

    // No log id established yet for this instance
    LogId = 0;

    // If this is the first thread in the current process, create a mutex
    //   to control the static thread id counter.
    if (s_hLogMutex == 0)
        s_hLogMutex = CreateMutex (0, false, "CdrLogMutex");

    // Create unique id for this log object
    if (s_hLogMutex != 0) {

        DWORD stat = WaitForSingleObject (s_hLogMutex, 5000);

        switch (stat) {

            case WAIT_OBJECT_0:
            case WAIT_ABANDONED:
                LogId = ++s_LogId;
                if (!ReleaseMutex (s_hLogMutex)) {
                    ErrMsg =
                        L"Can't release mutex!  Error=";
                    ErrMsg += cdr::String::toString (GetLastError ());
                }
                break;
            case WAIT_TIMEOUT:
                ErrMsg =
                   L"Timeout waiting for mutex for log id";
                break;
            default:
                ErrMsg =
                   L"Unknown mutex error - can't happen!";
        }
    }
    else {
        ErrMsg = L"Can't create mutex.  Error=";
        ErrMsg += cdr::String::toString (GetLastError ());
    }

    if (!ErrMsg.empty()) {

        // This should never happen
        // If it does, let object write itself to wherever critical errors go
        cdr::String src = L"Log constructor";
        WriteFile (src, ErrMsg);
    }
}

/**
 * Write ()
 *   Write something to the log file.
 *   Front end when no connection available.
 */

void cdr::log::Log::Write (
    const cdr::String MsgSrc,   // Name of module or whatever caller wants
    const cdr::String Msg       // Message
) {
    // Create a database connection and write data
    try {
        cdr::db::Connection dbConn =
            cdr::db::DriverManager::getConnection (cdr::db::url,
                                                   cdr::db::uid, cdr::db::pwd);
        this->Write (MsgSrc, Msg, dbConn);
    }
    catch (cdr::Exception e) {
        // Failed to connect or failed to write
        // Use alternative instead
        WriteFile (L"CdrLog DB Write Failed", e.what());
        WriteFile (MsgSrc, Msg);
    }
    catch (...) {
        // Use alternative instead
        WriteFile (L"CdrLog DB Write Failed", L"Unknown exception");
        WriteFile (MsgSrc, Msg);
    }
}


/**
 * Write ()
 *   Write something to the log file.
 *   Full write function.
 */

void cdr::log::Log::Write (
    const cdr::String MsgSrc,   // Name of module or whatever caller wants
    const cdr::String Msg,      // Message
    cdr::db::Connection& dbConn // New or existing connection
) {
    // Copies to handle truncation if necessary
    // Efficient since copy is only deep when truncation occurs
    cdr::String cpySrc = MsgSrc;
    cdr::String cpyMsg = Msg;

    // Truncate strings if necessary
    if (cpySrc.size() > cdr::log::SrcMaxLen)
        cpySrc = cpySrc.substr (0, cdr::log::SrcMaxLen);
    if (cpyMsg.size() > cdr::log::MsgMaxLen)
        cpyMsg = cpyMsg.substr (0, cdr::log::MsgMaxLen);

    // Construct update query
    std::string sqlstmt = "INSERT INTO debug_log ("
                            "thread, "
                            "recorded, "
                            "source, "
                            "msg) "
                          "VALUES ("
                            "?, "
                            "GETDATE(), "
                            "?, "
                            "?)";

    cdr::db::PreparedStatement insert = dbConn.prepareStatement (sqlstmt);

    insert.setInt    (1, LogId);
    insert.setString (2, cpySrc);
    insert.setString (3, cpyMsg);

#define WE_GET_IT_WORKING 1
#ifdef WE_GET_IT_WORKING
    // Store it
    try {
        insert.executeQuery ();
    }
    catch (cdr::Exception e) {
        // Couldn't write.  Use exception reporting instead
        // Don't re-throw exception.  Would probably cause a loop.
        WriteFile (MsgSrc, Msg);
    }
#else
    WriteFile (MsgSrc, Msg);
#endif
}


/**
 * WriteFile ()
 *   Writes to ordinary os file, if it can't be logged to db.
 *   But can also be called directly.
 */

/* XXXX - BUG - XXXX
 * Can I write a wide string to a file with fwprintf?
 * Result ain't right.
 */

void cdr::log::WriteFile (
    const cdr::String MsgSrc,   // Name of module or whatever caller wants
    const cdr::String Msg,      // Message
    const std::string Fname     // Filename, may be defaulted
) {
    // First try to get exclusive control of the file
    HANDLE hMutex = CreateMutex (0, false, "CdrWriteFileMutex");
    DWORD stat    = WaitForSingleObject (hMutex, 5000);


    // Set datetime
    time_t ltime;
    time (&ltime);
    wchar_t *wct = _wctime (&ltime);

    // Try to log the message whether or not we got exclusive access
    // When writing to a file, we don't truncate the data - don't
    //   have the database string limits on output
    FILE  *fp;
    if ((fp = fopen (Fname.c_str(), "a")) != NULL) {

        // Datetime, source, message
        fwprintf (fp, L"---%s>>>%s:\n%s\n", wct,
                  MsgSrc.c_str(), Msg.c_str());

        fclose (fp);
    }
    else {
        // Last resort is stderr
        fwprintf (stderr, L"---%s>>>%s:\n%s\n", wct,
                  MsgSrc.c_str(), Msg.c_str());
    }


    // Release mutex
    if (stat == WAIT_OBJECT_0 || stat == WAIT_ABANDONED)
        ReleaseMutex (hMutex);
}
