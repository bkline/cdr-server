/*
 * $Id: CdrGetDoc.cpp,v 1.10 2001-06-12 20:55:03 ameyer Exp $
 *
 * Stub version of internal document retrieval commands needed by other
 * modules.
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.9  2001/03/13 22:15:09  mruben
 * added ability to use CdrDoc element for filter
 *
 * Revision 1.8  2001/03/02 13:58:39  bkline
 * Moved the body for fixString() to cdr::entConvert() in CdrString.cpp.
 *
 * Revision 1.7  2000/10/30 17:41:47  mruben
 * modified interface to version control functions
 *
 * Revision 1.6  2000/10/24 23:08:40  ameyer
 * Added version control and locking.
 *
 * Revision 1.5  2000/09/25 14:01:45  mruben
 * added getDocCtlString to get information from document table
 *
 * Revision 1.4  2000/05/23 18:22:50  mruben
 * implemented primitive form of CdrGetDoc
 *
 * Revision 1.3  2000/05/19 00:07:22  bkline
 * Added required attributes to <CdrDoc> element.  Fixed control section.
 *
 * Revision 1.2  2000/05/17 12:55:55  bkline
 * Removed code to process columns which are no longer present in the
 * document table.
 *
 * Revision 1.1  2000/05/03 15:18:12  bkline
 * Initial revision
 */

#include <iostream>
#include "CdrDom.h"
#include "CdrCommand.h"
#include "CdrException.h"
#include "CdrDbResultSet.h"
#include "CdrDbPreparedStatement.h"
#include "CdrGetDoc.h"
#include "CdrVersion.h"
#include "CdrBlob.h"

// Local functions.
static cdr::String encodeBlob(const cdr::Blob& blob);
static cdr::String fixString(const cdr::String& s);
static cdr::String makeDocXml(const cdr::String& xml);
static cdr::String makeDocBlob(const cdr::Blob& blob);
static cdr::String readOnlyWrap(const cdr::String text, const cdr::String tag);

/**
 * Builds the XML string for a <CdrDoc> element for a document extracted from
 * the database.  I assume there will be another overloaded implementation
 * which takes a parameter specifying the version number.  This implementation
 * goes directly against the document table.
 */
cdr::String cdr::getDocString(
        const cdr::String&    docIdString,
        cdr::db::Connection&  conn,
        bool usecdata)
{
    // Go get the document information.
    int docId = docIdString.extractDocId();
    std::string query = "          SELECT d.val_status,"
                        "                 d.val_date,"
                        "                 d.title,"
                        "                 d.xml,"
                        "                 d.active_status,"
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
    cdr::String     valStatus    = rs.getString(1);
    cdr::String     valDate      = rs.getString(2);
    cdr::String     title        = cdr::entConvert(rs.getString(3));
    cdr::String     xml          = rs.getString(4);
    cdr::String     activeStatus = rs.getString(5);
    cdr::String     comment      = cdr::entConvert(rs.getString(6));
    cdr::String     docType      = rs.getString(7);
    cdr::Blob       blob         = rs.getBytes(8);
    select.close();

    // Just in case the caller sent an ID string not in canonical form.
    wchar_t canonicalIdString[25];
    swprintf(canonicalIdString, L"CDR%010d", docId);

    // Build the CdrDoc string.
    cdr::String cdrDoc = cdr::String(L"<CdrDoc Type='")
                       + docType
                       + L"' Id='"
                       + canonicalIdString
                       + L"'>\n<CdrDocCtl>\n";

    cdrDoc += readOnlyWrap (valStatus, L"DocValStatus");
    cdrDoc += readOnlyWrap (activeStatus, L"DocActiveStatus");
    if (!valDate.isNull() && valDate.length() > 0) {
        if (valDate.length() > 10)
            valDate[10] = L'T';
        cdrDoc += readOnlyWrap (valDate, L"DocValDate");
    }
    cdrDoc += tagWrap (title, L"DocTitle");
    if (!comment.isNull() && comment.length() > 0)
        cdrDoc += tagWrap (comment, L"DocComment");
    cdrDoc += L"</CdrDocCtl>\n";

    // Plug in the body of the document.
    cdrDoc += usecdata ? makeDocXml (xml) : xml;

    // If there's a blob, add it, too.
    if (!blob.isNull())
        cdrDoc += makeDocBlob (blob);

    // Terminate and return the string to the caller.
    cdrDoc += L"</CdrDoc>\n";
    return cdrDoc;
}

/**
 * Builds the XML string for a <CdrDoc> element for a document extracted
 * from Version Control.
 *
 * Overloaded from the one without version info.
 */

cdr::String cdr::getDocString(
        const cdr::String&    docIdString,
        cdr::db::Connection&  conn,
        struct cdr::CdrVerDoc *docVer,
        bool usecdata
) {
    // Extract doc id from string
    int docId = docIdString.extractDocId();

    // Human readable form of version number
    cdr::String versionStr = cdr::String::toString (docVer->num);

    // Get human readable document type
    std::string typQry = "SELECT name FROM doc_type WHERE id = ?";
    cdr::db::PreparedStatement typSel = conn.prepareStatement(typQry);
    typSel.setInt(1, docVer->doc_type);
    cdr::db::ResultSet typRs = typSel.executeQuery();
    if (!typRs.next())
        throw cdr::Exception(L"getDocString: Unable to find doc type "
                             + cdr::String::toString (docVer->doc_type)
                             + L" for document "
                             + docIdString
                             + L", version: "
                             + versionStr);
    cdr::String typName = typRs.getString (1);

    // Get human readable user identifier
    std::string usrQry = "SELECT name FROM usr WHERE id = ?";
    cdr::db::PreparedStatement usrSel = conn.prepareStatement(usrQry);
    usrSel.setInt(1, docVer->usr);
    cdr::db::ResultSet usrRs = usrSel.executeQuery();
    if (!usrRs.next())
        throw cdr::Exception(L"getDocString: Unable to find user id "
                             + cdr::String::toString (docVer->usr)
                             + L" for document "
                             + docIdString
                             + L", version: "
                             + versionStr);
    cdr::String usrName = usrRs.getString (1);

    // Create a string to return
    cdr::String cdrDoc = L"<CdrDoc Type='"
                       + docIdString
                       + L"'>\n<CdrDocCtl>\n";

    // Create attributes for DocVersion
    cdr::String verAttrs = L"readonly=\"yes\" Publishable=\"" +
                           docVer->publishable + L"\"";

    // Individual elements of control info
    // These are not all the same as in the document table
    cdrDoc += readOnlyWrap (docVer->title, L"DocTitle")
           +  tagWrap (versionStr, L"DocVersion", verAttrs)
           +  readOnlyWrap (docVer->updated_dt, L"DocModified")
           +  readOnlyWrap (usrName, L"DocModifier");

    // Comment is optional
    if (docVer->comment.size() > 0)
        cdrDoc += readOnlyWrap (docVer->comment, L"DocComment");

    // That's all we've got from the doc control
    // Add an end tag for it and fetch xml and blob
    cdrDoc += L"</DocCtl>\n";
    cdrDoc += usecdata ? makeDocXml (docVer->xml) : docVer->xml;
    if (!docVer->data.isNull())
        cdrDoc += makeDocBlob (docVer->data);

    // Terminate and return the string to the caller.
    cdrDoc += L"</CdrDoc>\n";
    return cdrDoc;
}


/**
 * Wrap a tag around an element, giving it a readonly attribute.
 * We also append a newline just for nicer formatting.
 *
 * @param   text    Reference to string to appear as element text.
 * @param   tag     Reference to tag string.
 *
 * @return          XMLified text
 */

static cdr::String readOnlyWrap (
    cdr::String text,
    cdr::String tag
) {
    return (cdr::tagWrap (text, tag, L"readonly=\"yes\"") + L"\n");
}


/**
 * Build a document body as a CDATA section
 *
 *  @param  xml     Reference to xml string
 *  @return         CdrDocXml element containing CDATA section
 */

static cdr::String makeDocXml (
    const cdr::String& xml
) {
    return (L"<CdrDocXml><![CDATA[" + xml +  L"]]>\n</CdrDocXml>\n");
}


/**
 * Build a blob element in base64.
 *
 *  @param  blob    Reference to xml string
 *  @return         CdrDocBlob element encoded in base64
 */

static cdr::String makeDocBlob (
    const cdr::Blob& blob
) {
    return (L"<CdrDocBlob encoding='base64'>"
            +  encodeBlob(blob) +  L"\n</CdrDocBlob>\n");
}


/**
 * Builds the XML string for a <CdrDocCtl> element for a document
 * extracted from the database.  I assume there will be another
 * overloaded implementation which takes a parameter specifying the
 * version number.  This implementation goes directly against the
 * document table.
 */
cdr::String cdr::getDocCtlString(
        const cdr::String&    docIdString,
        cdr::db::Connection&  conn,
        int elements)
{
    // For now we ignore the select and get everything
    // Go get the document information.
    int docId = docIdString.extractDocId();
    std::string query = "          SELECT d.val_status,"
                        "                 d.val_date,"
                        "                 d.title,"
                        "                 d.comment"
                        "            FROM document d"
                        "           WHERE d.id = ?";
    cdr::db::PreparedStatement select = conn.prepareStatement(query);
    select.setInt(1, docId);
    cdr::db::ResultSet rs = select.executeQuery();
    if (!rs.next())
        throw cdr::Exception(L"Unable to load document", docIdString);
    cdr::String     valStatus = rs.getString(1);
    cdr::String     valDate   = rs.getString(2);
    cdr::String     title     = cdr::entConvert(rs.getString(3));
    cdr::String     comment   = cdr::entConvert(rs.getString(4));
    select.close();

    // Build the CdrDoc string.
    cdr::String cdrDoc = cdr::String(L"<CdrDocCtl><DocValStatus>")
                       + valStatus
                       + L"</DocValStatus>";
    if (!valDate.isNull() && valDate.length() > 0) {
        if (valDate.length() > 10)
            valDate[10] = L'T';
        cdrDoc += cdr::String(L"<DocValDate>") + valDate + L"</DocValDate>";
    }
    cdrDoc += cdr::String(L"<DocTitle>") + title + L"</DocTitle>";
    if (!comment.isNull() && comment.length() > 0)
        cdrDoc += cdr::String(L"<DocComment>") + comment + L"</DocComment>";
    cdrDoc += L"</CdrDocCtl>\n";

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

/**
 * Processes CdrGetDoc command
 */

cdr::String cdr::getDoc(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection)
{
  enum {
      current,                      // Current version
      numbered,                     // Specific numbered version
      last,                         // Last checked in version
      before,                       // Last version in before specified date
      label                         // Version with specified label
  } versionType = current;

  int         id;                   // String form of doc id
  cdr::String idStr;                // String form of doc id
  cdr::String lock = "N";           // Default value of <lock> element
  cdr::String user = "";            // Check with Bob on what this is XXXX
  cdr::String version = L"0";       // Default value of <Version> element
  cdr::String versionDate = L"";    // Look for version checked in before this
  cdr::String versionLabel = L"";   // Label affixed to version
  int         versionNum = 0;       // Version number, 0 = current, -1 = last
  struct cdr::CdrVerDoc docVer;     // Filled in by version control


  // extract command arguments
  for (cdr::dom::Node child = commandNode.getFirstChild();
       child != NULL;
       child = child.getNextSibling())
  {
    if (child.getNodeType() == cdr::dom::Node::ELEMENT_NODE)
    {
      cdr::String name = child.getNodeName();
      if (name == L"DocId") {
        idStr = cdr::dom::getTextContent(child);
        id    = idStr.extractDocId();
      }
      else
      if (name == L"DocVersion")
      {
        // Optional request for version
        version = cdr::dom::getTextContent(child);

        // Legal version values are:
        //    "Current"
        //    "LastVersion"
        //    "Before YYYY-MM-DD"
        //    "Label ..."
        //    0..N version number

        // Parse the type and get associated values
        if (version == L"Current" || version.size() == 0)
            ; // Defaults okay
        else if (version == L"LastVersion") {
            versionType = last;
            versionNum  = cdr::getVersionNumber (id, dbConnection);
        }
        else if (version.substr (0, 7) == L"Before ") {
            versionType = before;
            versionDate = version.substr (7, 99);
        }
        else if (version.substr (0, 6) == L"Label ") {
            versionType  = label;
            versionLabel = version.substr (6, 99);
        }
        else if (iswdigit(version.c_str()[0])) {
            versionNum  = version.getInt();
            versionType = numbered;
        }
        else
            throw cdr::Exception (L"Unrecognized DocVersion value = '"
                                  + version
                                  + L"'");
      }

      if (name == L"Lock")
      {
        lock = cdr::dom::getTextContent(child);
        if (lock == L"y")
            lock = L"Y";
        else if (lock == L"n")
            lock = L"N";
      }
      else
      if (name == L"UserId")
        user = cdr::dom::getTextContent(child);
      if (name == L"DocOffset")
        throw cdr::Exception(L"CdrGetDoc offset not yet supported");
      if (name == L"DocMaxLength")
        throw cdr::Exception(L"CdrGetDoc length not yet supported");
    }
  }

  // If lock requested, try to check out doc
  if (lock == L"Y") {

      int userId = session.getUserId();

      // User can only lock a document if authorized to modify it
      // First find document type so we can check user's authorization
      std::string qry = "SELECT t.name "
                        "FROM   document d "
                        "JOIN   doc_type t "
                        "ON     d.doc_type = t.id "
                        "WHERE  d.id = ?";
      cdr::db::PreparedStatement stmt = dbConnection.prepareStatement (qry);
      stmt.setInt (1, idStr.extractDocId());
      cdr::db::ResultSet rs = stmt.executeQuery ();
      if (!rs.next())
          throw cdr::Exception (L"Document " + cdr::String::toString(idStr) +
                                L" not found");
      cdr::String typeName = rs.getString (1);

      if (!session.canDo (dbConnection, "MODIFY DOCUMENT", typeName))
          throw cdr::Exception (L"User not authorized to checkout documents "
                                L"of type '" + typeName + L"'");

      // See if the document has been checked out already to someone else
      int         outUser;  // User id of person who has the doc now
      cdr::String outDate;  //
      if (isCheckedOut (id, dbConnection, &outUser)) {

          // If user is the same, he can re-acquire the document
          // If different, he's got to get the other user to check it in first
          if (outUser != userId) {

              // Find out who it was and inform this user
              std::string uQry = "SELECT name FROM usr WHERE id = ?";
              cdr::db::PreparedStatement uStmt =
                     dbConnection.prepareStatement (uQry);
              uStmt.setInt (1, outUser);
              cdr::db::ResultSet uRs = uStmt.executeQuery ();
              if (uRs.next())
                  throw cdr::Exception (L"Document "
                            + idStr
                            + L" already checked out to user "
                            + uRs.getString (1));
              else
                  throw cdr::Exception (L"Document "
                            + idStr
                            + L" checked out to unknown user "
                            + cdr::String::toString (outUser)
                            + L" (Internal Error)");
          }
      }

      else
          // Check it out and get version number
          version = cdr::String::toString (cdr::checkOut (session, id,
                      dbConnection, L"", false));
  }

  cdr::String docStr = L"<CdrGetDocResp>\n" + readOnlyWrap (idStr, L"DocId");

  // What comes next depends on requested version
  if (versionType == current)
      // Get it from the document table
      docStr += getDocString (idStr, dbConnection);

  else {
      switch (versionType) {
          case numbered:
              // Fill in struct using number
              if (!cdr::getVersion (id, dbConnection, versionNum, &docVer))
                throw cdr::Exception(L"getDocString: Unable to find version "
                                     + cdr::String::toString (versionNum)
                                     + L" of document "
                                     + idStr);
              break;

          case label:
              // Fill in struct using label
              if (!cdr::getLabelVersion (id, dbConnection, versionLabel,
                                         &docVer))
                throw cdr::Exception(L"getDocString: Unable to find version '"
                                     + versionLabel
                                     + L"' of document "
                                     + idStr);
              break;

          case before:
              // Fill in struct using date
              if (!cdr::getVersion (id, dbConnection, versionDate, &docVer))
                throw cdr::Exception(L"getDocString: Unable to find version "
                                     L"dated before "
                                     + versionLabel
                                     + L" of document "
                                     + idStr);
              break;

          case last:
              // XXXX ASK MIKE HOW TO GET THIS
              throw cdr::Exception (
                      L"Request for last version not yet implemented");
      }

      // We've retrieved the version, format it into XML
      docStr += cdr::getDocString (idStr, dbConnection, &docVer);
  }

  // Add xml termination for all
  return (docStr + L"</CdrGetDocResp>\n");
}
