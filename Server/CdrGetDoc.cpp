/*
 * Internal document retrieval commands needed by other modules.
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
#include "CdrBlobExtern.h"
#include "CdrFilter.h"

// Local functions.
static bool        isReadyForReview(int, cdr::db::Connection&);
static cdr::String fixString(const cdr::String& s);
static cdr::String makeDocBlob(const cdr::Blob& blob);
static cdr::String readOnlyWrap(const cdr::String text, const cdr::String tag);
static cdr::String denormalizeLinks(const cdr::String&, cdr::db::Connection&,
                                    const cdr::String&);
static cdr::String getCommonCtlString(int docId,
                                      cdr::db::Connection& conn,
                                      int elements);


/**
 * Builds the XML string for a <CdrDoc> element for a document extracted from
 * the database.  I assume there will be another overloaded implementation
 * which takes a parameter specifying the version number.  This implementation
 * goes directly against the document table.
 */
cdr::String cdr::getDocString(
        const cdr::String&    docIdString,
        cdr::db::Connection&  conn,
        bool                  usecdata,
        bool                  denormalize,
        bool                  getXml,
        bool                  getBlob)
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
                        "                 b.blob_id"
                        "            FROM document d"
                        " LEFT OUTER JOIN doc_blob_usage b"
                        "              ON b.doc_id = d.id"
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
    cdr::Int        blobId       = rs.getInt(8);
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
    cdrDoc += readOnlyWrap (title, L"DocTitle");
    if (!comment.isNull() && comment.length() > 0)
        cdrDoc += tagWrap (comment, L"DocComment");
    bool readyForReview = isReadyForReview(docId, conn);
    cdrDoc += readOnlyWrap (readyForReview ? L"Y" : L"N", L"ReadyForReview");
    cdrDoc += L"</CdrDocCtl>\n";

    // XML
    if (getXml) {
        // Denormalize the links if requested.
        if (denormalize)
            xml = denormalizeLinks(xml, conn, docType);

        // Plug in the body of the document.
        cdrDoc += usecdata ? makeDocXml (xml) : xml;
    }

    // If there's a blob, add it, too.
    if (getBlob && !blobId.isNull()) {
        cdr::Blob *pBlob = cdr::getBlobPtrByBlobId(conn, blobId);
        if (pBlob) {
            cdrDoc += makeDocBlob(*pBlob);
            delete pBlob;
        }
        else
            throw cdr::Exception(L"getDocString: No blob found for blobId=" +
                                 docIdString);
    }

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
        const struct cdr::CdrVerDoc *docVer,
        bool usecdata,
        bool denormalize,
        bool getXml,
        bool getBlob
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
    typSel.close();

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
    usrSel.close();

    // Create a string to return
    cdr::String cdrDoc = L"<CdrDoc Type='"
                       + typName
                       + L"' Id='"
                       + docIdString
                       + L"'>\n<CdrDocCtl>\n";

    // Create attributes for DocVersion
    cdr::String verAttrs = L"readonly=\"yes\" Publishable=\"" +
                           docVer->publishable + L"\"";

    // Individual elements of control info
    // These are not all the same as in the document table
    bool readyForReview = isReadyForReview(docId, conn);
    cdrDoc += readOnlyWrap (cdr::entConvert(docVer->title), L"DocTitle")
           +  tagWrap (versionStr, L"DocVersion", verAttrs)
           +  readOnlyWrap (docVer->updated_dt, L"DocModified")
           +  readOnlyWrap (usrName, L"DocModifier")
           +  readOnlyWrap (readyForReview ? L"Y" : L"N", L"ReadyForReview");

    // Comment is optional
    if (docVer->comment.size() > 0)
        cdrDoc += readOnlyWrap (cdr::entConvert(docVer->comment),
                                L"DocComment");
    // That's all we've got from the doc control
    // Add an end tag for it and fetch xml and blob
    cdrDoc += L"</CdrDocCtl>\n";

    // Denormalize the links if requested.
    if (getXml) {
        cdr::String xml = docVer->xml;
        if (denormalize)
            xml = denormalizeLinks(xml, conn, typName);
        cdrDoc += usecdata ? makeDocXml (xml) : xml;
    }

    if (getBlob && !docVer->blob_id.isNull())
        cdrDoc += makeDocBlob (docVer->data);

    // Terminate and return the string to the caller.
    cdrDoc += L"</CdrDoc>\n";
    return cdrDoc;
}

cdr::String cdr::getDocString(
        const cdr::String&    docIdString,
        cdr::db::Connection&  conn,
        int version,
        bool usecdata,
        bool denormalize,
        bool getXml,
        bool getBlob,
        const cdr::String& maxDate
) {
  // If no version number, and no max date, get it from the document table
  if (version == 0 && maxDate == cdr::DFT_VERSION_DATE)
    return cdr::getDocString(docIdString, conn, usecdata, denormalize,
                             getXml, getBlob);

  // We're going to search for a version
  // getVersion() will fill in the CdrVerDoc struct with all info
  cdr::CdrVerDoc v;

  // Else if there's a max date, get the highest version
  //   with that max date
  // Uses the date checking overload of getVersion
  // Note: No check for publishability.  Is that a problem? XXX
  if (maxDate != cdr::DFT_VERSION_DATE)
    if (!getVersion(docIdString.extractDocId(), conn, maxDate, &v))
      throw cdr::Exception(L"  : Unable to find document "
                               + docIdString + L" with date < " + maxDate);

  // Else get the requested version by version number
  if (!getVersion(docIdString.extractDocId(), conn, version, &v))
    throw cdr::Exception(L"getDocString: Unable to find document "
                             + docIdString
                             + L", version: "
                             + cdr::String::toString(version));

  // Everything is now in the CdrVerDoc structure
  // Format a <CdrDoc> XML object to return
  return cdr::getDocString(docIdString, conn, &v, usecdata, denormalize,
                           getXml, getBlob);
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

cdr::String cdr::makeDocXml (
    const cdr::String& xml
) {
    return (L"<CdrDocXml><![CDATA[" + xml +  L"]]>\n</CdrDocXml>\n");
}


/**
 * Build a blob element in base64.
 *
 * XXX - Optimization note:
 *
 *   For optimization, we could call cdr::getVerBlob here rather than
 *   in the code that fills in a CdrVerDoc structure.
 *
 *   Not sure the blob is used anywhere except by routines that
 *   call makeDocBlob to encode it in base64.
 *
 *   Then we should only call makeDocBlob if blob is actually requested.
 *
 *  @param  blob    Reference to xml string
 *  @return         CdrDocBlob element encoded in base64
 */

static cdr::String makeDocBlob (
    const cdr::Blob& blob
) {
    cdr::String encodedBlob = blob.encode();
    return (L"<CdrDocBlob encoding='base64'>"
            +  encodedBlob +  L"\n</CdrDocBlob>\n");
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
    int docId = docIdString.extractDocId();
    std::string query = "          SELECT d.val_status,"
                        "                 d.val_date,"
                        "                 d.title,"
                        "                 d.comment,"
                        "                 r.doc_id"
                        "            FROM document d"
                        " LEFT OUTER JOIN ready_for_review r"
                        "              ON r.doc_id = d.id"
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
    cdr::Int        rrId      = rs.getInt(5);
    select.close();

    // Build the CdrDoc string.
    cdr::String cdrDoc = cdr::String(L"<DocValStatus>")
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
    cdr::String rrString = rrId.isNull() ? L"N" : L"Y";
    cdrDoc += L"<ReadyForReview>" + rrString + L"</ReadyForReview>";

    return L"<CdrDocCtl>" + cdrDoc
         + getCommonCtlString(docId, conn, elements)
         + L"</CdrDocCtl>\n";
}

/**
 * Builds the XML string for a <CdrDocCtl> element for a document version
 * extracted from the database.
 */
cdr::String cdr::getDocCtlString(
        const cdr::String&    docIdString,
        int                   version,
        cdr::db::Connection&  conn,
        int elements)
{
  if (version == 0)
    return cdr::getDocCtlString(docIdString, conn, elements);

  // For now we ignore the select and get everything
  // Go get the document information.
  int docId = docIdString.extractDocId();
  std::string query = "SELECT d.val_status,"
                      "       d.val_date,"
                      "       d.title,"
                      "       d.comment"
                      "  FROM doc_version d"
                      " WHERE d.id = ?"
                      "   AND d.num=?";
  cdr::db::PreparedStatement select = conn.prepareStatement(query);
  select.setInt(1, docId);
  select.setInt(2, version);
  cdr::db::ResultSet rs = select.executeQuery();
  if (!rs.next())
    throw cdr::Exception(L"Unable to load document", docIdString);
  cdr::String     valStatus = rs.getString(1);
  cdr::String     valDate   = rs.getString(2);
  cdr::String     title     = cdr::entConvert(rs.getString(3));
  cdr::String     comment   = cdr::entConvert(rs.getString(4));
  select.close();

  // Build the CdrDoc string.
  cdr::String cdrDoc = cdr::String(L"<DocValStatus>")
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

  return L"<CdrDocCtl>" + cdrDoc
       + getCommonCtlString(docId, conn, elements)
       + L"</CdrDocCtl>\n";
}

/**
 * Export function to find the data a document was first published.
 */
cdr::String cdr::getDateFirstPublished(int docId,
                                       db::Connection& conn) {

    std::string query = "SELECT d.first_pub                          "
                        "  FROM document d                           "
                        " WHERE d.first_pub IS NOT NULL              "
                        "   AND d.first_pub_knowable = 'Y'           "
                        "   AND d.id = ?                             ";

    db::PreparedStatement select = conn.prepareStatement(query);
    select.setInt(1, docId);
    db::ResultSet rs = select.executeQuery();
    String answer(true);
    if (rs.next())
        answer = toXmlDate(rs.getString(1));
    select.close();
    return answer;
}

/**
 * Builds the common CdrDocCtl elements
 */
static cdr::String getCommonCtlString(int docId,
                                      cdr::db::Connection& conn,
                                      int elements)
{
  cdr::String cdrDoc;

  if (elements & cdr::DocCtlComponents::DocCreate)
  {
    std::string query = "SELECT a.dt,"
                        "       u.name"
                        "  FROM audit_trail a"
                        "  JOIN usr u"
                        "    ON a.usr = u.id"
                        "  JOIN action ac"
                        "    ON a.action = ac.id"
                        " WHERE a.document = ?"
                        "   AND ac.name = 'ADD DOCUMENT'";
    cdr::db::PreparedStatement select = conn.prepareStatement(query);
    select.setInt(1, docId);
    cdr::db::ResultSet rs = select.executeQuery();
    if (rs.next())
    {
      cdr::String date = rs.getString(1);
      if (date.length() > 10)
        date[10] = L'T';
      cdr::String user = rs.getString(2);
      cdrDoc += L"<Create><Date>" + date + L"</Date><User>"
              + user + L"</User></Create>";
    }
    select.close();
  }

  if (elements & cdr::DocCtlComponents::DocMod)
  {
    std::string query = "SELECT TOP 1 a.dt,"
                        "       u.name"
                        "  FROM audit_trail a"
                        "  JOIN usr u"
                        "    ON a.usr = u.id"
                        "  JOIN action ac"
                        "    ON a.action = ac.id"
                        " WHERE a.document = ?"
                        "   AND ac.name = 'MODIFY DOCUMENT'"
                     " ORDER BY a.dt DESC";

    cdr::db::PreparedStatement select = conn.prepareStatement(query);
    select.setInt(1, docId);
    cdr::db::ResultSet rs = select.executeQuery();
    if (rs.next())
    {
      cdr::String date = rs.getString(1);
      if (date.length() > 10)
        date[10] = L'T';
      cdr::String user = rs.getString(2);
      cdrDoc += L"<Modify><Date>" + date + L"</Date><User>"
              + user + L"</User></Modify>";
    }
    select.close();
  }

  if (elements & cdr::DocCtlComponents::DocFirstPub)
  {
    cdr::String date = cdr::getDateFirstPublished(docId, conn);
    if (!date.isNull())
      cdrDoc += L"<FirstPub><Date>" + date + L"</Date></FirstPub>";
  }

  return cdrDoc;
}

/**
 * Apply denormalization filter to the XML.
 * If denormalization fails (e.g., the document is malformed), just
 * return it as is.
 *
 *  @param  xml     reference to string representation of xml for document.
 *  @param  conn    reference to CDR database connection object.
 *  @param  docType name of the document's type.
 *  @return         denormalized xml string.
 */
cdr::String denormalizeLinks(const cdr::String& xml,
                             cdr::db::Connection& conn,
                             const cdr::String& docType)
{
    // Watch out for some document types which shouldn't be denormalized.
    if (docType == L"Filter"
    ||  docType == L"css"
    ||  docType == L"schema")
        return xml;

    // Empty parameter list.
    cdr::FilterParmVector pv;

    // Ignore warnings and other messages
    cdr::String dummyErrStr;
    cdr::String denormXml;
    try {
        denormXml = cdr::filterDocumentByScriptTitle(xml,
                L"Fast Denormalization Filter", conn, &dummyErrStr, &pv);
    }
    catch (cdr::Exception&) {
        // Can't do it, just return what we had
        denormXml = xml;
    }

    return denormXml;
}

cdr::String cdr::getDocTypeName(
    const cdr::String& docId,
    cdr::db::Connection& conn
) {
    // Look in the database for the document type.
    const static char* query =
        " SELECT name "
        "   FROM doc_type "
        "   JOIN document "
        "     ON document.doc_type = doc_type.id "
        "  WHERE document.id = ?";
    cdr::db::PreparedStatement stmt = conn.prepareStatement(query);
    stmt.setInt(1, docId.extractDocId());
    cdr::db::ResultSet rslt = stmt.executeQuery();
    if (!rslt.next()) {
        stmt.close();
        throw cdr::Exception(L"Cannot find document type for document", docId);
    }
    cdr::String s = rslt.getString(1);
    stmt.close();
    return s;
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
  bool        denorm = true;        // Flag for link denormalization
  struct cdr::CdrVerDoc docVer;     // Filled in by version control


  const cdr::dom::Element& cmdElem =
        static_cast<const cdr::dom::Element&>(commandNode);

  // Attributes on the command node can suppress retrieval of xml or blob
  bool getXml  = cdr::ynCheck(cmdElem.getAttribute(L"includeXml"), true, "");
  bool getBlob = cdr::ynCheck(cmdElem.getAttribute(L"includeBlob"), true, "");
  if (!getXml && !getBlob)
      throw cdr::Exception("getDoc: Can't specify no xml and no blob");

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
      else if (name == L"DocVersion")
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
            versionNum = version.getInt();
            if (versionNum == 0)
                versionType = current;
            else
                versionType = numbered;
        }
        else
            throw cdr::Exception (L"Unrecognized DocVersion value = '"
                                  + version
                                  + L"'");
      }

      else if (name == L"Lock")
      {
        lock = cdr::dom::getTextContent(child);
        if (lock == L"y")
            lock = L"Y";
        else if (lock == L"n")
            lock = L"N";
      }
      else if (name == L"UserId")
        user = cdr::dom::getTextContent(child);
      else if (name == L"DenormalizeLinks")
        denorm = cdr::ynCheck(cdr::dom::getTextContent(child), true,
                              L"DenormalizeLinks");
      else if (name == L"DocOffset")
        throw cdr::Exception(L"CdrGetDoc offset not yet supported");
      else if (name == L"DocMaxLength")
        throw cdr::Exception(L"CdrGetDoc length not yet supported");
    }
  }

  // Workaround for a bug in Sablotron's XSL/T processor.
  if (denorm && getDocTypeName(idStr, dbConnection) == L"Filter")
      denorm = false;

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
      docStr += getDocString (idStr, dbConnection, true, denorm,
                              getXml, getBlob);

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
      docStr += cdr::getDocString (idStr, dbConnection, &docVer, true, denorm,
                                   getXml, getBlob);
  }

  // Add xml termination for all
  return (docStr + L"</CdrGetDocResp>\n");
}

/**
 * Find out if the document has been marked as ready for pre-publication
 * review.
 */
bool isReadyForReview(int id, cdr::db::Connection& conn)
{
      std::string query =
          "SELECT doc_id FROM ready_for_review WHERE doc_id = ?";
      cdr::db::PreparedStatement stmt = conn.prepareStatement(query);
      stmt.setInt(1, id);
      cdr::db::ResultSet rs = stmt.executeQuery();
      bool result = rs.next() ? true : false;
      stmt.close();
      return result;
}
