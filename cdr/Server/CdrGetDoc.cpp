/*
 * $Id: CdrGetDoc.cpp,v 1.2 2000-05-17 12:55:55 bkline Exp $
 *
 * Stub version of internal document retrieval commands needed by other
 * modules.
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.1  2000/05/03 15:18:12  bkline
 * Initial revision
 */

#include "CdrDom.h"
#include "CdrCommand.h"
#include "CdrException.h"
#include "CdrDbResultSet.h"
#include "CdrDbPreparedStatement.h"
#include "CdrGetDoc.h"
#include "CdrBlob.h"

// Local functions.
static cdr::String encodeBlob(const cdr::Blob& blob);

/**
 * Builds the XML string for a <CdrDoc> element for a document extracted from
 * the database.  I assume there will be another overloaded implementation
 * which takes a parameter specifying the version number.  This implementation
 * goes directly against the document table.
 */
cdr::String cdr::getDocString(
        const cdr::String&    docIdString,
        cdr::db::Connection&  conn)
{
    // Go get the document information.
    int docId = docIdString.extractDocId();
    std::string query = "          SELECT d.val_status,"
                        "                 d.val_date,"
                        "                 d.title,"
                        "                 d.xml,"
                        "                 d.comment,"
                        "                 t.name,"
                        "                 b.data"
                        "            FROM document d"
                        " LEFT OUTER JOIN doc_blob b"
                        "              ON b.id = d.id"
                        "            JOIN doc_type t"
                        "              ON t.id = d.doc_type"
                        "           WHERE d.id = ?";
    cdr::db::PreparedStatement select = conn.prepareStatement(query);
    select.setInt(1, docId);
    cdr::db::ResultSet rs = select.executeQuery();
    if (!rs.next())
        throw cdr::Exception(L"Unable to load document", docIdString);
    cdr::String     valStatus = rs.getString(1);
    cdr::String     valDate   = rs.getString(2);
    cdr::String     title     = rs.getString(3);
    cdr::String     xml       = rs.getString(4);
    cdr::String     comment   = rs.getString(5);
    cdr::String     docType   = rs.getString(6);
    cdr::Blob       blob      = rs.getBytes(7);
    select.close();

    // Just in case the caller sent an ID string not in canonical form.
    wchar_t canonicalIdString[25];
    swprintf(canonicalIdString, L"CDR%010d", docId);

    // Build the CdrDoc string.
    cdr::String cdrDoc = cdr::String(L"<CdrDoc><CdrDocCtl><DocId>")
                       + canonicalIdString
                       + L"</DocId><DocType>"
                       + docType
                       + L"</DocType><DocValStatus>"
                       + valStatus
                       + L"</DocValStatus>";
    if (!valDate.isNull()) {
        if (valDate.size() > 10)
            valDate[10] = L'T';
        cdrDoc += cdr::String(L"<DocValDate>") + valDate + L"</DocValDate>";
    }
    cdrDoc += cdr::String(L"<DocType>") + title + L"</DocType><DocTitle>"
           + title + L"</DocTitle>";
    if (!comment.isNull())
        cdrDoc += cdr::String(L"<Comment>") + comment + L"</Comment>";
    cdrDoc += L"</CdrDocCtl>";

    // Plug in the body of the document.
    cdrDoc += L"<CdrDocXml><![CDATA["
           +  xml
           +  L"]]></CdrDocXml>";

    // If there's a blob, add it, too.
    if (!blob.isNull())
        cdrDoc += cdr::String(L"<CdrDocBlob encoding='base64'>")
               +  encodeBlob(blob)
               +  L"</CdrDocBlob>";

    // Return the string to the caller.
    cdrDoc += L"</CdrDoc>";
    return cdrDoc;
}

cdr::String encodeBlob(const cdr::Blob& blob)
{
    wchar_t *buf = 0;
    static const wchar_t codes[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   L"abcdefghijklmnopqrstuvwsyz"
                                   L"0123456789+/";
    const wchar_t pad = L'=';
    try {
        size_t nBytes = blob.size();
        size_t nChars = (nBytes /  3 + 1) * 4   // encoding bloat
                      + (nBytes / 76 + 1) * 2;  // line breaks
        wchar_t* buf = new wchar_t[nChars];
        size_t i = 0;
        wchar_t* p = buf;
        size_t charsInLine = 0;
        while (i < nBytes) {
            switch (nBytes - i) {
            case 1:
                p[0] = codes[(blob[i + 0] >> 2)                    & 0x3F];
                p[1] = codes[(blob[i + 0] << 4)                    & 0x3F];
                p[2] = pad;
                p[3] = pad;
                break;
            case 2:
                p[0] = codes[(blob[i + 0] >> 2)                    & 0x3F];
                p[1] = codes[(blob[i + 0] << 4 | blob[i + 1] >> 4) & 0x3F];
                p[2] = codes[(blob[i + 1] << 2)                    & 0x3F];
                p[3] = pad;
                break;
            default:
                p[0] = codes[(blob[i + 0] >> 2)                    & 0x3F];
                p[1] = codes[(blob[i + 0] << 4 | blob[i + 1] >> 4) & 0x3F];
                p[2] = codes[(blob[i + 1] << 2 | blob[i + 2] >> 6) & 0x3F];
                p[3] = codes[(blob[i + 2])                         & 0x3F];
                break;
            }
            i += 3;
            p += 4;
            charsInLine += 4;
            if (charsInLine >= 76) {
                *p++ = L'\r';
                *p++ = L'\n';
                charsInLine = 0;
            }
        }

        // Release buffer and return as string
        cdr::String retval = cdr::String(buf, p - buf);
        delete [] buf;
        return retval;
    }
    catch (...) {
        delete [] buf;
        throw;
    }
}
