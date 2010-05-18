/*
 * $Id$
 *
 * Implementation for ODBC result fetching wrapper (modeled after JDBC).
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.14  2002/11/13 15:03:06  bkline
 * Added code to handle SQL_NO_DATA_FOUND in getString().
 *
 * Revision 1.13  2002/03/28 18:26:59  bkline
 * Added some support for ResultSetMetaData.
 *
 * Revision 1.12  2001/12/19 12:26:29  bkline
 * Fixed bug in blob retrieval which resulted in loss of first byte.
 *
 * Revision 1.11  2001/12/14 15:20:08  bkline
 * Added space for a single byte for large values; even though the ODBC
 * docs say nothing will be stored for a call with insufficient space,
 * BoundsChecker says otherwise.
 *
 * Revision 1.10  2001/06/12 22:37:04  bkline
 * Fixed bug in handling of large blobs (buffer too small).
 *
 * Revision 1.9  2001/04/16 17:59:16  bkline
 * Removed unreferenced local variable nRows.
 *
 * Revision 1.8  2001/04/08 22:46:22  bkline
 * Added code in constructor to skip to the first result set.
 *
 * Revision 1.7  2000/12/28 13:24:55  bkline
 * Made ref count for ResultSet dynamic.  Added debugging statements.
 *
 * Revision 1.6  2000/05/04 12:48:13  bkline
 * Implemented reference counting.
 *
 * Revision 1.5  2000/05/03 15:25:41  bkline
 * Fixed database statement creation.
 *
 * Revision 1.4  2000/04/26 01:24:05  bkline
 * Fixed BLOB retrieval.
 *
 * Revision 1.3  2000/04/22 22:15:04  bkline
 * Added more comments.
 *
 * Revision 1.2  2000/04/17 21:26:06  bkline
 * Added nullability for ints and strings.
 *
 * Revision 1.1  2000/04/15 12:21:38  bkline
 * Initial revision
 */

#include "CdrDbResultSet.h"
#include "CdrException.h"
#include <iostream> // XXX for debugging

/**
 * Gathers information about the columns of the results of the current query
 * in preparation for retrieving the results.
 */
cdr::db::ResultSet::ResultSet(cdr::db::Statement& s)
    : st(s), pRefCount(new int(1))
{
#if DEBUG
    std::cout << "ResultSet constructor; *pRefCount=" << *pRefCount << '\n';
    std::cout << "st.hstmt=" << st.hstmt << '\n';
#endif
    SQLSMALLINT nCols;
    SQLRETURN   rc;

    // Move to the first result set (if any).
    try {
        do {
            rc = SQLNumResultCols(st.hstmt, &nCols);
            if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
                throw cdr::Exception(L"Failure determining result columns",
                                     st.getErrorMessage(rc));
            if (nCols < 1) {
                rc = SQLMoreResults(st.hstmt);
                if (rc != SQL_SUCCESS &&
                    rc != SQL_SUCCESS_WITH_INFO &&
                    rc != SQL_NO_DATA)
                {
                    throw cdr::Exception(L"Failure moving to result set",
                                         st.getErrorMessage(rc));
                }
            }
        } while (nCols < 1 && rc != SQL_NO_DATA);
    }
    catch (...) {
      cdr::log::pThreadLog->Write(L"CdrDbResultSet::constructor",
      L"Exception thrown positioning to start of result set");
      throw;
    }


    // Extract the column information (if any).
    for (SQLSMALLINT i = 1; i <= nCols; ++i) {
        ResultSetMetaData::Column c;
        char name[1024];
        SQLSMALLINT cbName;
        try {
            rc = SQLDescribeCol(st.hstmt, (SQLSMALLINT)i, (SQLCHAR *)name,
                            sizeof name, &cbName, &c.type, &c.size,
                            &c.digits, &c.nullable);
        }
        catch (...) {
            cdr::log::pThreadLog->Write(L"CdrDbResultSet::constructor",
            L"Exception thrown describing columns");
            throw;
        }

        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
            throw cdr::Exception(L"Database failure fetching column info",
                                 st.getErrorMessage(rc));
        c.name = std::string(name, cbName);
        metaData.columnVector.push_back(c);
    }
}

/**
 * Copy constructor.  Uses reference counting to avoid deep copying.  Doesn't
 * seem to matter, as the compiler appears to have optimized these away.
 */
cdr::db::ResultSet::ResultSet(const ResultSet& rs)
    : st(rs.st), metaData(rs.metaData), pRefCount(rs.pRefCount)
{
    ++*pRefCount;
#if DEBUG
    std::cout << "ResultSet copy constructor; *pRefCount="
              << *pRefCount << '\n';
    std::cout << "st.hstmt=" << st.hstmt << '\n';
#endif
}

/**
 * Cleans up.
 */
cdr::db::ResultSet::~ResultSet()
{
#if DEBUG
    std::cout << "Top of ResultSet destructor; *pRefCount="
              << *pRefCount << '\n';
#endif
    if (--*pRefCount > 0)
        return;
    delete pRefCount;
}

/**
 * Returns true if there is another row to be fetched and prepares to extract
 * the values.  A future enhancement may actually extract the values here, in
 * order to support the getXXX() methods in any order, and multiple times for
 * the same value.
 */
bool cdr::db::ResultSet::next()
{
#if DEBUG
    std::cout << "Top of ResultSet::next()\n";
    std::cout << "st.hstmt=" << st.hstmt << '\n';
#endif
    SQLRETURN rc = SQLFetch(st.hstmt);
#if DEBUG
    std::cout << "Back from SQLFetch()\n";
#endif
    if (rc == SQL_NO_DATA_FOUND) {
#if DEBUG
        std::cout << "Returning 'true' from ResultSet::next()\n";
#endif
        return false;
    }
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
        throw cdr::Exception(L"Database failure fetching data",
                             st.getErrorMessage(rc));
#if DEBUG
    std::cout << "Returning 'true' from ResultSet::next()\n";
#endif
    return true;
}

/**
 * Extracts the string value for the <code>pos</code> of the result row.
 */
cdr::String cdr::db::ResultSet::getString(int pos)
{
    if (pos < 1 || pos > metaData.getColumnCount())
        throw cdr::Exception(L"ResultSet: column request out of range");
    SQLRETURN rc;
    ResultSetMetaData::Column& c = metaData.columnVector[pos - 1];
    bool largeValue = c.size > 10000;
    size_t bufSize = largeValue ? 1 : c.size + 1;
    wchar_t* data = new wchar_t[bufSize];
    memset(data, 0, bufSize * sizeof(wchar_t));
    SDWORD cbData = (bufSize) * sizeof(wchar_t);
    rc = SQLGetData(st.hstmt, (UWORD)pos, SQL_C_WCHAR, (PTR)data,
                    cbData, &cbData);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        delete [] data;
        if (rc == SQL_NO_DATA_FOUND)
            return cdr::String();
        throw cdr::Exception("Database failure extracting data",
                             st.getErrorMessage(rc));
    }
    if (cbData == SQL_NULL_DATA) {
        delete[] data;
        return cdr::String(true);
    }
    if (largeValue) {
        delete[] data;
        size_t chars = cbData / sizeof(wchar_t);
        data = new wchar_t[chars + 1];
        data[chars] = 0;
        rc = SQLGetData(st.hstmt, (UWORD)pos, SQL_C_WCHAR, (PTR)data,
                        cbData + sizeof(wchar_t), &cbData);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
            delete [] data;
            if (rc == SQL_NO_DATA_FOUND)
                return cdr::String();
            throw cdr::Exception("Database failure extracting data",
                                 st.getErrorMessage(rc));
        }
    }

    cdr::String s(data, cbData / sizeof(wchar_t));
    delete [] data;
    return s;
}

/**
 * Extracts the integer value for the <code>pos</code> of the result row.
 */
cdr::Int cdr::db::ResultSet::getInt(int pos)
{
    SQLRETURN rc;
    int data;
    SDWORD cbData;
    rc = SQLGetData(st.hstmt, (UWORD)pos, SQL_C_SLONG, (PTR)&data, 0,
                    &cbData);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
        throw cdr::Exception("Database failure extracting data",
                             st.getErrorMessage(rc));
    if (cbData == SQL_NULL_DATA)
        return cdr::Int(true);
    return data;
}

/**
 * Extracts a vector of bytes for the <code>pos</code> of the result row.
 */
cdr::Blob cdr::db::ResultSet::getBytes(int pos)
{
    if (pos < 1 || pos > metaData.getColumnCount())
        throw cdr::Exception(L"ResultSet: column request out of range");
    SQLRETURN rc;
    ResultSetMetaData::Column& c = metaData.columnVector[pos - 1];
    bool largeValue = c.size > 10000;
    size_t bufSize = largeValue ? 0 : c.size;
    unsigned char* data = new unsigned char[bufSize + 1];
    memset(data, 0, bufSize + 1);
    SDWORD cbData = bufSize;
    rc = SQLGetData(st.hstmt, (UWORD)pos, SQL_C_BINARY, (PTR)data,
                    cbData, &cbData);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        delete [] data;
        throw cdr::Exception("Database failure extracting data",
                             st.getErrorMessage(rc));
    }
    if (cbData == SQL_NULL_DATA) {
        delete[] data;
        return cdr::Blob(true);
    }
    if (largeValue) {
        delete[] data;
        size_t chars = cbData;
        data = new unsigned char[chars + 1];
        data[chars] = 0;
        rc = SQLGetData(st.hstmt, (UWORD)pos, SQL_C_BINARY, (PTR)data,
                        cbData, &cbData);
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
            delete [] data;
            throw cdr::Exception("Database failure extracting data",
                                 st.getErrorMessage(rc));
        }
    }

    cdr::Blob b(data, cbData);
    delete [] data;
    return b;
}
