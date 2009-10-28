#pragma warning(disable : 4786) // While including iostream
#include <iostream> // DEBUG

/* XXXX
 * Possible optimizations if needed:
 *
 *  1. Search control tables only once for a transaction, retrieving
 *     info into an in-memory map, instead of searching for each link.
 *  2. Keep control info in memory between transactions - but requires
 *     server bounce to change tables.
 *  3. Cache all link_net updates and only apply the changes instead
 *     of deleting all entries for source document and rebuilding all.
 *
 * We'll only do any of these if needed, and may want to profile before
 * deciding which to do.
 */

/*
 * CdrLink.cpp
 *
 * Module for creating, maintaining, and searching the link network
 * stored in the link_net table.
 *
 *                                          Alan Meyer  July, 2000
 *
 * $Id: CdrLink.cpp,v 1.32 2008-10-03 23:54:36 bkline Exp $
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.31  2008/03/25 23:43:13  ameyer
 * Revised error handling to support cdr:eid error ID attributes on validation.
 *
 * Revision 1.30  2007/07/13 00:39:51  ameyer
 * Fixed missing question mark in link_type update statement.
 *
 * Revision 1.29  2006/11/17 05:29:30  ameyer
 * Added link fragment validation using the query_term and query_term_pub
 * tables instead of the link_fragment table.  Did not yet remove update
 * of the link_fragment table.  We can do that after more testing.
 *
 * Revision 1.28  2006/10/18 20:32:45  bkline
 * Fixed name of link_xml table's foreign key into link_type table.
 *
 * Revision 1.27  2006/10/06 02:40:42  ameyer
 * Modifications for link target version type handling.
 * Affects link validation, getLinkType, putLinkType, and the external
 * XML transactions that drive them.
 *
 * Revision 1.26  2004/02/10 22:11:20  ameyer
 * Added check to be sure a target document type has been specified if a new
 * link type is created.
 *
 * Revision 1.25  2003/12/30 22:49:16  ameyer
 * Added more error checking for creation of link types.  Also added
 * a cheap runtime check.
 *
 * Revision 1.24  2003/09/29 18:15:18  bkline
 * Fixed a memory leak on line 1663 (unwanted use of new operator).
 *
 * Revision 1.23  2003/09/23 23:02:42  ameyer
 * Modified link_net.target_doc updating to insert null if no target_doc id
 * is present instead of 0.
 * This has no effect for now because we are not installing xrefs in the
 * link_net table.  But it is desirable if we ever do.
 *
 * Revision 1.22  2002/08/29 21:52:07  ameyer
 * Added some constants to represent initial values of object members.
 *
 * Revision 1.21  2002/08/14 01:51:04  ameyer
 * Added length checks on fragment and url sizes.
 * Restored saveLink checking in link table insertions, left out of the
 * new speedup code.
 *
 * Revision 1.20  2002/08/11 22:48:34  bkline
 * Plugged in makeshift solution for overlong values for link_net table.
 *
 * Revision 1.19  2002/07/25 09:57:33  bkline
 * Turned off timing code.
 *
 * Revision 1.18  2002/07/24 15:09:36  bkline
 * Added performance enhancements to link and query_term table updates.
 *
 * Revision 1.17  2002/05/08 20:31:24  pzhang
 * Added implementation of getSearchLinksResp.
 *
 * Revision 1.16  2002/04/17 19:19:07  bkline
 * Eliminated duplicate space in error message.
 *
 * Revision 1.15  2002/02/19 15:44:18  ameyer
 * Closing statements after use.
 *
 * Revision 1.14  2002/02/15 06:46:13  ameyer
 * Bug fix in routines to update link types.
 *
 * Revision 1.13  2002/02/12 21:29:10  ameyer
 * Fixed error message.
 *
 * Revision 1.12  2002/01/31 16:40:06  ameyer
 * Reorganized order of events so that the check for a link target's existence
 * is performed in the CdrLink constructor.  This is needed so that we can
 * avoid referential integrity failures when updating the link_net with an
 * unvalidated document.
 *
 * Revision 1.11  2002/01/24 15:18:07  ameyer
 * Improved error messages.
 * Changed fragment field to store null instead of "" in database.
 *
 * Revision 1.10  2002/01/22 18:59:49  ameyer
 * Fixed dumb bug not initializing frag and srcId.
 *
 * Revision 1.9  2001/12/19 15:49:29  ameyer
 * Can now update link tables without performing validation.
 *
 * Revision 1.8  2001/11/06 21:41:24  bkline
 * Fixed a SQL bug.
 *
 * Revision 1.7  2001/09/25 14:56:35  ameyer
 * Bob added preliminary version of pasteLink().
 *
 * Revision 1.6  2001/05/17 17:31:43  ameyer
 * Added administrative functions to maintain the link tables.
 * Made minor modifications in error displays and catching errors.
 *
 * Revision 1.5  2001/04/17 23:11:24  ameyer
 * Added links within a document to itself.
 * Added call to external link procs.
 *
 * Revision 1.4  2000/12/13 01:50:59  ameyer
 * Implemented delLinks, added changes for docs deleted by marking them
 * deleted.
 *
 * Revision 1.3  2000/09/27 20:25:16  bkline
 * Fixed last argument to findTargetDocTypes().
 *
 * Revision 1.2  2000/09/27 11:28:44  bkline
 * Added cdrDelLinks() and findTargetDocTypes().
 *
 * Revision 1.1  2000/09/26 19:04:26  ameyer
 * Initial revision
 *
 *
 */

// Eliminate annoying warnings about truncated debugging information.
#pragma warning(disable : 4786)

#include "CdrSession.h"
#include "CdrDom.h"
#include "CdrString.h"
#include "CdrException.h"
#include "CdrDbConnection.h"
#include "CdrCommand.h"
#include "CdrDbPreparedStatement.h"
#include "CdrDbStatement.h"
#include "CdrDbResultSet.h"
#include "CdrValidateDoc.h"
#include "CdrLink.h"
// #define CDR_TIMINGS // Uncomment line to activate timing code.
#include "CdrTiming.h"

// Object representing one row in the link_net table.
struct LinkNetRow {
    int         link_type;
    int         source_doc;
    cdr::String source_elem;
    cdr::Int    target_doc;
    cdr::String target_frag;
    cdr::String url;
    LinkNetRow(int t, int sd, const cdr::String& se, const cdr::Int& td,
               const cdr::String& tf, const cdr::String& u)
        : link_type(t), source_doc(sd), source_elem(se), target_doc(td),
          target_frag(tf), url(u) {}
    bool operator==(const LinkNetRow& r) const {
        return link_type   == r.link_type   &&
               source_doc  == r.source_doc  &&
               source_elem == r.source_elem &&
               target_doc  == r.target_doc  &&
               target_frag == r.target_frag &&
               url         == r.url;
    }
    // XXX 2008-03-20 Is this right?  Why six '<'?
    //     That allows any lower valued field to indicate <
    //     Don't see where it's used?
    bool operator<(const LinkNetRow& r) const {
        if (source_doc  < r.source_doc)  return true;
        if (source_doc  > r.source_doc)  return false;
        if (link_type   < r.link_type)   return true;
        if (link_type   > r.link_type)   return false;
        if (source_elem < r.source_elem) return true;
        if (source_elem > r.source_elem) return false;
        if (target_doc  < r.target_doc)  return true;
        if (target_doc  > r.target_doc)  return false;
        if (target_frag < r.target_frag) return true;
        if (target_frag > r.target_frag) return false;
        return url < r.url;
    }
};
typedef std::set<LinkNetRow> LinkNetRowSet;

// Prototypes for internal functions
static int linkTree (cdr::db::Connection&, const cdr::dom::Node&,
                     int, int, cdr::String&, cdr::link::LnkList&,
                     cdr::StringSet&, cdr::StringSet&,
                     cdr::ValidationControl&);

static int checkMissedFrags (cdr::db::Connection&, int,
                     cdr::ValidRule, cdr::StringSet&,
                     cdr::ValidationControl&);

static void updateFragList (cdr::db::Connection&, int, cdr::StringSet&);

static bool findDocType (cdr::db::Connection&, int, int *, cdr::String *);

static void addLinkSource (cdr::dom::Node&, cdr::db::Connection&, int);

static void addLinkProperty (cdr::dom::Node&, cdr::db::Connection&, int);

static void addLinkTarget (cdr::String&, cdr::db::Connection&, int);

// Update queries to execute when deleting or replacing link info
// Terminate list with null string
static std::string S_delLinkQuery[] = {
    "DELETE FROM link_xml WHERE link_id = ?",
    "DELETE FROM link_target WHERE source_link_type = ?",
    "DELETE FROM link_properties WHERE link_id = ?",
    ""
};

/**
 * Constructor for a CdrLink object
 * See CdrLink.h for documentation
 */

cdr::link::CdrLink::CdrLink (
    cdr::db::Connection& conn,
    cdr::dom::Node&      elemDom,
    int                  docId,
    int                  docType,
    cdr::String          docTypeStr,
    cdr::String          refValue,
    cdr::link::LinkStyle lnkStyle
) : fldNode (elemDom), srcId (docId), srcDocType (docType),
    srcDocTypeStr (docTypeStr), ref (refValue), style (lnkStyle)
{
    std::string qry;        // For database SQL string


    // Default values for uninitialized object
    type          = 0;
    typeStr       = L"";
    srcField      = L"";
    trgId         = cdr::link::NO_DOC_ID;
    trgIdStr      = L"";
    trgFound      = false;
    trgDocType    = 0;
    trgDocTypeStr = L"";
    trgActiveStat = L"";
    chkType       = L"C";
    trgFrag       = cdr::link::NO_FRAGMENT;
    hasData       = false;
    saveLink      = true;


    // Sanity check
    if (ref.size() == 0)
        throw cdr::Exception (L"Empty or missing ref value - Can't happen!");

    // Get info from the dom concerning this link
    const cdr::dom::Element& element
             = static_cast<const cdr::dom::Element&> (fldNode);
    srcField = fldNode.getNodeName ();

    // The ValidationControl object also uses the node to report errors
    errCtl.setElementContext(fldNode);

    // Get link type for this link based on doctype and field
    // Also finds out what table to check against
    qry = "SELECT x.link_id, t.chk_type "
          "  FROM link_xml x "
          "  JOIN link_type t "
          "    ON x.link_id = t.id "
          " WHERE x.doc_type = ? AND x.element = ?";
    cdr::db::PreparedStatement select = conn.prepareStatement (qry);
    select.setInt (1, srcDocType);
    select.setString (2, srcField);
    cdr::db::ResultSet rs = select.executeQuery();

    if (!rs.next()) {
        // No link type defined for this doctype + element
        // Error will be caught again and reported later
        saveLink = false;
    }
    else {
        type    = rs.getInt (1);
        chkType = rs.getString(2);

        // Runtime check of table integrity
        // Should never be more than one definition for a doctype + element
        if (rs.next())
            throw cdr::Exception (
              L"System Error - More than one link type defined for doctype=\""
              + srcDocTypeStr + L"\" element=\"" + srcField + L"\"");
    }
    // Is this a reference to a CDR document?
    if (style == cdr::link::ref || style == cdr::link::href) {

        // Get the fragment portion of the reference, if any
        const wchar_t *fragptr = wcschr (ref.c_str(), '#');
        if (fragptr) {

            // Isolate base reference and fragment
            trgFrag  = cdr::String (fragptr + 1);
            trgIdStr = cdr::String (ref.c_str(), (fragptr - ref.c_str()));

            // If no base reference found, base is the current doc
            // I.e., reference is to an id inside this document
            if (trgIdStr.size() == 0)
                trgIdStr = cdr::stringDocId (srcId);
        }
        else {
            trgFrag  = cdr::link::NO_FRAGMENT;
            trgIdStr = ref;
        }

        // Get the id portion of the reference
        // This also tests for valid 'CDRnnn...' format
        try {
            trgId = trgIdStr.extractDocId ();
        }
        catch (const cdr::Exception& e) {
            // Probably an invalid format
            // If we add cdr:xref to the link_net, this has to change
            trgId = cdr::link::NO_DOC_ID;
            this->addLinkErr (e.what());
        }

        // If this is a self-link, we know things about the target
        if (trgId == srcId) {
            trgFound      = true;
            trgDocType    = srcDocType;
            trgDocTypeStr = srcDocTypeStr;
            trgActiveStat = L"A";
        }
        else {
            // Setup query for current working doc, any pub version, or any
            //   version.  Also check that doc is active, not blocked or
            //   deleted.
            if (chkType == L"C")
                // Current working doc
                qry = "SELECT doc_type, active_status "
                      "  FROM all_docs WHERE id = ?";
            else if (chkType == L"P")
                // Publishable doc, at least one required
                qry = "SELECT top 1 d.doc_type, d.active_status "
                      "  FROM all_docs d "
                      "  JOIN doc_version v "
                      "    ON d.id = v.id "
                      "   AND v.publishable = 'Y' "
                      "   AND d.id = ?";
            else if (chkType == L"V")
                // Any version at exists, any one okay
                qry = "SELECT top 1 d.doc_type, d.active_status "
                      "  FROM all_docs d "
                      "  JOIN doc_version v "
                      "    ON d.id = v.id "
                      "   AND d.id = ?";
            else
                throw cdr::Exception(
                 L"link_type table chk_type has illegal value - can't happen");

            // Find out if target exists, get its type & deletion status
            // This is needed whether or not we validate the link because
            //   we can't save a link that doesn't point to an existing
            //   document
            // Note that if trgId==NO_DOC_ID, this will (correctly) get
            //   no hits
            cdr::db::PreparedStatement doc_sel = conn.prepareStatement (qry);
            doc_sel.setInt (1, trgId);
            cdr::db::ResultSet doc_rs = doc_sel.executeQuery();
            if (doc_rs.next()) {
                trgFound      = true;
                trgDocType    = doc_rs.getInt (1);
                trgActiveStat = doc_rs.getString (2);
            }
            else {
                // Target not in the database
                // Saving the link would fail referential integrity
                saveLink = false;
            }
        }
    }

    // Validate that lengths don't violate database sizes
    if (trgFrag.size() > cdr::link::MAX_FRAG_SIZE) {
        this->addLinkErr (L"Fragment '" +
                trgFrag + L"' too large for database table");
        saveLink = false;
    }
    if (ref.size() > cdr::link::MAX_URL_SIZE) {
        this->addLinkErr (L"Full reference '" +
                ref + L"' too large for database table");
        saveLink = false;
    }

} // Constructor


/**
 * Validate one free standing link
 * See CdrLink.h for documentation
 */

int cdr::link::CdrLink::validateLink (
    cdr::db::Connection& dbConn,
    cdr::StringSet&      fragSet
) {
    std::string qry;        // For database SQL string


    // If linked document not found, no other validation possible
    if (!trgFound) {
        // Generate appropriate error message based on link chkType
        cdr::String notFoundMsg;
        if (chkType == L"C")
            notFoundMsg = L"No current document found for link target";
        else if (chkType == L"P")
            notFoundMsg = L"No publishable document found for link target";
        else if (chkType == L"V")
            notFoundMsg = L"No document version found for link target";
        notFoundMsg += L": " + srcField + L"=" + trgIdStr;
        this->addLinkErr (notFoundMsg);
    }

    // None of these checks can be made without a link type or target id
    else if (type != 0 && trgId != cdr::link::NO_DOC_ID) {

        // Tests are done differently for links to self and links to other
        if (trgId != srcId) {

            // The target was searched for in the CdrLink constructor
            if (trgFound) {
                // Is it to an active document?
                if (trgActiveStat == L"D")
                    this->addLinkErr (L"Target link is to deleted document");

                // Does the target contain the expected fragment id, if any?
                if (trgFrag != cdr::link::NO_FRAGMENT) {

                    // Check link fragments against publishable or
                    //   current working docs, depending on chkType
                    // We don't have a special table just for last version
                    if (chkType == L"P")
                        qry = "SELECT 0 FROM query_term_pub "
                              " WHERE value=? AND doc_id=?";
                    else
                        qry = "SELECT 0 FROM query_term "
                              " WHERE value=? AND doc_id=?";
                    cdr::db::PreparedStatement frag_sel =
                                dbConn.prepareStatement (qry);
                    frag_sel.setString (1, trgFrag);
                    frag_sel.setInt (2, trgId);
                    cdr::db::ResultSet frag_rs = frag_sel.executeQuery();

                    // Error if it doesn't
                    if (!frag_rs.next()) {
                        if (chkType == L"P")
                            this->addLinkErr (L"cdr:id matching fragment '" +
                                trgFrag + L"' not found in publishable"
                                          L" target document");
                        else
                            this->addLinkErr (L"cdr:id matching fragment '" +
                                trgFrag + L"' not found in current working"
                                          L" target document");
                    }
                }
            }
        }

        else {
            // Target doc is same as source doc
            // If this is a link to a fragment, check to be sure it's here
            if (trgFrag != cdr::link::NO_FRAGMENT) {
                if (fragSet.find (trgFrag) == fragSet.end())
                    this->addLinkErr (L"cdr:id matching fragment '" +
                            trgFrag + L"' not found in this document");
            }
        }

        // Validate target doctype for this link
        qry = "SELECT target_doc_type FROM link_target "
              "WHERE  source_link_type = ?";
        cdr::db::PreparedStatement targ_sel = dbConn.prepareStatement (qry);
        targ_sel.setInt (1, type);
        cdr::db::ResultSet targ_rs = targ_sel.executeQuery();

        // Need to find a target doc type for this link
        // And need to make sure at least one of them matches
        bool found_target_doc_type = false;
        bool match_target_doc_type = false;
        while (targ_rs.next()) {
            found_target_doc_type = true;
            if (trgFound && trgDocType == targ_rs.getInt (1))
                match_target_doc_type = true;
        }

        // Nothing in the table is a system error
        // But we'll deal with it as an ordinary error for now
        if (!found_target_doc_type)
            this->addLinkErr (L"System Error: This link type has no target"
			                  L" doc_type defined");

        // Wrong target type is a link error
        else if (!match_target_doc_type)
            this->addLinkErr (L"Link from " + srcDocTypeStr + L"." + srcField +
                              L" to document type" + trgDocTypeStr +
                              L" is illegal");

        // Execute any custom link procedures (in CdrLinkProcs.cpp)
        customLinkCheck (dbConn, this);
    }
    else {
        if (type == 0)
            this->addLinkErr (L"No link type is defined for field " +
                              srcField + L" in document type " + srcDocTypeStr);
        else { // trgId==NO_DOC_ID
            if (style == cdr::link::ref || style == cdr::link::href)
                this->addLinkErr (L"No target document id specified in link");
        }
    }

    return getErrorCount();

} // validateLink()


/**
 * Delete all links from a document.
 *
 * See CdrLink.h
 */
int cdr::link::cdrDelLinks (
    cdr::db::Connection&    dbConn,
    int                     docId,
    cdr::ValidRule          vRule,
    cdr::StringList&        errList
) {

    // Find out if any other documents link to this one
    // Get the titles and fragment identifiers too for reporting purposes
    std::string breakQry =
            "SELECT DISTINCT source_doc, title, target_frag "
            "  FROM link_net, document "
            " WHERE link_net.target_doc = ? "
            "   AND link_net.source_doc = document.id";

    cdr::db::PreparedStatement breakSel = dbConn.prepareStatement (breakQry);
    breakSel.setInt (1, docId);
    cdr::db::ResultSet breakRs = breakSel.executeQuery();

    while (breakRs.next()) {
        // Retrieve info about link
        cdr::String idStr = cdr::String::toString (breakRs.getInt (1));
        cdr::String title = breakRs.getString (2);
        cdr::String frag  = breakRs.getString (3);

        // Construct informative error message
        cdr::String errMsg = L"Document " + idStr + L": (" + title +
              L") links to this document";
        if (frag.size() > 0)
            errMsg += L" Fragment(" + frag + L")";

        errList.push_back (errMsg);
    }

    // If only validating, or if only updating if valid, then
    // Stop here if there were links to this doc
    if (vRule == ValidateOnly ||
            (vRule == UpdateIfValid && errList.size() == 0))
        return errList.size();

    // Delete all links from this document to other documents
    std::string delLinkQry = "DELETE FROM link_net WHERE source_doc = ?";
    cdr::db::PreparedStatement delLinkSel =
                dbConn.prepareStatement (delLinkQry);
    delLinkSel.setInt (1, docId);
    cdr::db::ResultSet delLinkRs = delLinkSel.executeQuery();

    // Delete all fragment table entries for this document
    std::string delFragQry = "DELETE FROM link_fragment WHERE doc_id = ?";
    cdr::db::PreparedStatement delFragSel =
                dbConn.prepareStatement (delFragQry);
    delFragSel.setInt (1, docId);
    cdr::db::ResultSet delFragRs = delFragSel.executeQuery();

    return errList.size();

} // cdrDelLinks()


/**
 * Process a CdrCommand to retrieve back links from the link_net tables.
 *
 * This is how we find out what links to a document, as opposed to
 * what documents it links to.
 *
 * Takes standard parameters for a CdrCommand.
 *
 *  @return             XML string containing one LnkItem for each link
 *                      to this document from another document.
 */

cdr::String cdr::getLinks (
    cdr::Session&         session,
    const cdr::dom::Node& commandNode,
    cdr::db::Connection&  dbConnection
) {
    int         docId;      // Doc id from transaction
    cdr::String docIdStr,   // String form "CDR00..."
                name;       // Element name


    // Get document id from command
    cdr::dom::Node node = commandNode.getFirstChild();
    while (node != 0) {

        if (node.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
            name = node.getNodeName();
            if (name != L"DocId")
                throw cdr::Exception (L"getLinks: Element '" + name
                                    + L"' in CdrGetLinks request is "
                                      L"currently unsupported");

            docIdStr = cdr::dom::getTextContent (node);
            docId    = docIdStr.extractDocId();
        }

        node = node.getNextSibling();
    }

    // In this simple getLinks we're just retrieving links that
    //   point to the current document
    // cdrDelLinks can already do that.
    cdr::StringList errList;
    int errCount = cdr::link::cdrDelLinks (dbConnection, docId,
                                           cdr::ValidateOnly, errList);

    // Pack it up the transaction and links
    cdr::String resp  = L"<CdrGetLinksRes>\n"
                        L"  <DocID>" + docIdStr + L"</DocID>\n"
                        L"  <LnkList>\n"
                        L"    <LnkCount>";
                resp += errCount + L"</LnkCount>\n";

    if (errCount) {
        // Show each error as a LinkItem
        cdr::StringList::const_iterator i = errList.begin();
        while (i != errList.end())
            resp += L"    " + cdr::tagWrap (*i++, L"LnkItem");
    }

    // Terminate and return it
    resp += L"  </LnkList>\n"
            L"</CdrGetLinksRes>\n";

    return resp;

} // getLinks()


/**
 * Dump link info in XML format
 * See CdrLink.h for documentation
 */

cdr::String cdr::link::CdrLink::dumpXML (
    cdr::db::Connection& dbConn
) {
    cdr::String xml;                    // String to return
    std::string qry;                    // SQL query for database access


    // Top level tag for XML
    xml = L"<LnkItem>";

    // Get link type name from database
    if (typeStr.size() == 0) {
        qry = "SELECT name FROM link_type WHERE id = ?";
        cdr::db::PreparedStatement stmt = dbConn.prepareStatement (qry);
        stmt.setInt (1, type);
        cdr::db::ResultSet rs = stmt.executeQuery();
        if (!rs.next())
            throw cdr::Exception (
                      L"CdrLink::dumpXML: Bad link type - Can't happen");
        typeStr = rs.getString (1);
    }
    xml += cdr::tagWrap (typeStr, L"LnkType");

    // Source document id
    xml += cdr::tagWrap (cdr::stringDocId (srcId), L"LnkSrcId");

    // Source document type
    xml += cdr::tagWrap (srcDocTypeStr, L"LnkSrcDocType");

    // Element tag in which link was found
    xml += cdr::tagWrap (srcField, L"LnkSrcField");

    // Target document id
    xml += cdr::tagWrap (trgIdStr, L"LnkTrgId");

    // Target document type, if known, may require lookup
    if (trgDocType > 0) {
        if (trgDocTypeStr.size() == 0) {
            qry = "SELECT name FROM doc_type WHERE id = ?";
            cdr::db::PreparedStatement stmt = dbConn.prepareStatement (qry);
            stmt.setInt (1, trgDocType);
            cdr::db::ResultSet rs = stmt.executeQuery();
            if (!rs.next())
                throw cdr::Exception (
                      L"CdrLink::dumpXML: Bad document type - Can't happen");
            trgDocTypeStr = rs.getString (1);
        }
        xml += cdr::tagWrap (trgDocTypeStr, L"LnkTrgDocType");
    }

    // Fragment identifier, if any
    if (trgFrag != cdr::link::NO_FRAGMENT)
        xml += cdr::tagWrap (trgFrag, L"LnkTrgFrag");

    // Reference string
    if (ref.size() > 0)
        xml += cdr::tagWrap (ref, L"LnkHref");

    // List of errors
    if (errCtl.getErrorCount() > 0) {
        // Get error info as a simple list of strings
        cdr::StringList errList;
        errCtl.getErrorMsgs(errList);

        // Show each error as a LinkErr
        cdr::StringList::const_iterator i = errList.begin();
        while (i != errList.end())
            xml += tagWrap (*i++, L"LinkErr");
    }

    // Terminate item
    xml += L"</LnkItem>";

    return xml;

} // dumpXML()


/**
 * Convert link info to string
 *
 * Produces strings like:
 *  "Link:Vocabulary Protocol/Diagnosis:1234=Terminology:2351"
 * See CdrLink.h for documentation
 */

cdr::String cdr::link::CdrLink::dumpString (
    cdr::db::Connection& dbConn
) {
    cdr::String str;                    // String to return
    std::string qry;                    // SQL query for database access


    // Get link type name from database, if not known
    if (type > 0) {
        if (typeStr.size() == 0) {
            qry = "SELECT name FROM link_type WHERE id = ?";
            cdr::db::PreparedStatement stmt = dbConn.prepareStatement (qry);
            stmt.setInt (1, type);
            cdr::db::ResultSet rs = stmt.executeQuery();
            if (!rs.next())
                throw cdr::Exception (
                      L"CdrLink::dumpString: Bad link type - Can't happen");
            typeStr = rs.getString (1);
        }
    }

    // Get target doc type from database, if not known
    if (trgDocType > 0) {
        if (trgDocTypeStr.size() == 0) {
            qry = "SELECT name FROM doc_type WHERE id = ?";
            cdr::db::PreparedStatement stmt = dbConn.prepareStatement (qry);
            stmt.setInt (1, trgDocType);
            cdr::db::ResultSet rs = stmt.executeQuery();
            if (!rs.next())
                throw cdr::Exception (
                      L"CdrLink::dumpString: Bad document type - Can't happen");
            trgDocTypeStr = rs.getString (1);
        }
    }

    // Doctype/Field=
    str = srcDocTypeStr + L"/" + srcField + L"=";

    // targettype:Id#Fragment
    if (trgDocType > 0) {
        str += trgDocTypeStr + L":" + cdr::String::toString (trgId);
        if (trgFrag.size() > 0)
            str += L"#" + trgFrag;
    }

    // Or hyperlink reference if link is to outside CDR
    else
        str += ref;

    // List of errors
    if (errCtl.getErrorCount() > 0) {
        cdr::StringList errList;
        errCtl.getErrorMsgs(errList);
        cdr::StringList::const_iterator i = errList.begin();
        while (i != errList.end())
            str += L" ERROR: " + *i++;
    }

    return str;

} // dumpString()


/**
 * Add an error message to a link object.
 *
 * The error is associated with an errCtl for this one link, not for
 * the document as a whole.  The errors must be cumulated later.
 */
void cdr::link::CdrLink::addLinkErr (
    cdr::String msg
) {
    // Get context from CdrLink object into ValidationError object
    errCtl.setElementContext(fldNode);

    // And add the message
    errCtl.addError (msg);
}

/**
 * Get the count of errors for the current link
 */
int cdr::link::CdrLink::getErrorCount()
{
    return errCtl.getErrorCount();
}

/**
 * Validate links in a document and, optionally, update the link_net
 * database by calling updateLinkNet().
 *
 * See CdrLink.h
 */

int cdr::link::cdrSetLinks (
    cdr::dom::Node&         xmlNode,
    cdr::db::Connection&    dbConn,
    int                     ui,
    cdr::String             docTypeStr,
    cdr::ValidRule          validRule,
    cdr::ValidationControl& errCtl
) {
    cdr::link::LnkList lnkList;     // List of all link objects from this doc
    cdr::StringSet uniqSet;         // For finding duplicate link info
    cdr::StringSet fragSet;         // Set of all ref fragments found
    int docType,                    // Internal id of docType
        err_count;                  // Number of errors found

    MAKE_TIMER(setLinksTimer);

    // Get internal form of doctype
    std::string qry = "SELECT id FROM doc_type WHERE name = ?";
    cdr::db::PreparedStatement select = dbConn.prepareStatement (qry);
    select.setString (1, docTypeStr);
    cdr::db::ResultSet rs = select.executeQuery();

    if (!rs.next())
        throw cdr::Exception (L"cdrSetLinks: Unknown doctype");
    docType = rs.getInt (1);
    SHOW_ELAPSED("doctype retrieved", setLinksTimer);

    // Set the current error context to the top level
    errCtl.setElementContext(xmlNode);

    // Build list of links found in this doc
    linkTree (dbConn, xmlNode, ui, docType, docTypeStr,
              lnkList, uniqSet, fragSet, errCtl);
    SHOW_ELAPSED("back from linkTree", setLinksTimer);

    // Unless we're not validating
    if (validRule != cdr::UpdateLinkTablesOnly) {

        // Validate all links, appending to an error message return string
        cdr::link::LnkList::iterator i = lnkList.begin();
        err_count = 0;
        while (i != lnkList.end()) {
            err_count += i->validateLink (dbConn, fragSet);
            if (i->getErrorCount() > 0)
                errCtl.cumulateErrors (i->getErrCtl());
            ++i;
        }
        SHOW_ELAPSED("links validated", setLinksTimer);

        // Check for missing fragments
        // This is not part of validateLink because the errors are not
        //   found in a link, but rather in the absence of a fragment
        // Set the current error context to the top level.  It should
        //   have changed after building the link tree
        errCtl.setElementContext(xmlNode);
        err_count += checkMissedFrags (dbConn, ui, validRule,
                                       fragSet, errCtl);
        SHOW_ELAPSED("missing fragments checked", setLinksTimer);
    }

    // Update the link net if required
    if (validRule == cdr::UpdateLinkTablesOnly ||
        validRule == cdr::UpdateUnconditionally ||
        (validRule == cdr::UpdateIfValid && err_count == 0)) {

        // All together now
        bool oldCommitted = dbConn.getAutoCommit();
        dbConn.setAutoCommit (false);

        // Update link net and fragment table
        cdr::link::updateLinkNet (dbConn, ui, lnkList);
        updateFragList (dbConn, ui, fragSet);

        dbConn.setAutoCommit (oldCommitted);
        SHOW_ELAPSED("back from setAutoCommit()", setLinksTimer);
    }

    // XXXX
    //      If we are deleting text from nodes, we might do part
    //      or all of it here.  Might delete the nodes in validateLink()
    //      then reconstruct the XML string here
    //      But we only do it if we are actually updating something.
    //      If just validating a document on which editing may continue,
    //      we don't want to change anything under the eyes of the human
    //      editor.
    //      The other, probably more efficient, place to delete text
    //      from nodes is in the filter module.  It simply ignores the
    //      text to be deleted when copying from its input to its output.
    // XXXX

    SHOW_ELAPSED("leaving cdrSetLinks", setLinksTimer);
    return err_count;

} // cdrSetLinks()


// See comments in CdrCommand.h

cdr::String cdr::putLinkType (
    cdr::Session&         session,  // User session info
    const cdr::dom::Node& node,     // DOM tree for transaction
    cdr::db::Connection&  conn      // Connection to CDR database
) {
    bool           newType,         // True=Adding new link type, else mod
                   autoCommitted;   // True=Not inside SQL transaction at start
    cdr::dom::Node child;           // Single node within transaction
    cdr::String    name,            // Element name
                   content,         // Element text content
                   cmdName,         // Name of command CdrAdd... or CdrMod...
                   typeName = L"",  // Link type name
                   newName  = L"",  // For renaming link type
                   chkType  = L"",  // 'P'ub, 'V'er, or 'C'wd
                   comment  = L"",  // Description of link type
                   action;          // Check this in authorization check
    std::string    updqry;          // Update query string
    int            linkId;          // Internal link type identifier

    // Use this for simple queries
    cdr::db::Statement stmt = conn.createStatement();

    // Top level command node is always an element
    if (node.getNodeType() != cdr::dom::Node::ELEMENT_NODE)
        throw cdr::Exception (L"cdrPutLinks called on non-element node"
                              L" - can't happen");

    // Find out whether we have an add or modify transaction
    cmdName = node.getNodeName();
    if (cmdName == L"CdrAddLinkType") {
        newType = true;
        action  = L"ADD LINKTYPE";
    }
    else if (cmdName == L"CdrModLinkType") {
        newType = false;
        action  = L"MODIFY LINKTYPE";
    }
    else
        throw cdr::Exception (L"cdrPutLinks called for unknown transaction "
                              L"type '" + cmdName + L"'");

    // Is it okay to do this?
    if (!session.canDo (conn, action, L""))
        throw cdr::Exception (L"User not authorized to perform " + action);

    // Get link type information
    child = node.getFirstChild();
    while (child != 0) {
        if (child.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
            name    = child.getNodeName();
            content = cdr::dom::getTextContent (child);

            if (name == L"Name")
                typeName = content;

            else if (name == L"NewName")
                newName = content;

            else if (name == L"LinkChkType")
                chkType = content;

            else if (name == L"Comment")
                comment = content;
        }
        child = child.getNextSibling();
    }

    // Name is a required element
    if (typeName.size() == 0)
        throw cdr::Exception (L"Missing Name element in " + cmdName +
                              L" transaction");

    // New name is only permitted when modifying an existing type
    if (newName.size() > 0 && newType)
        throw cdr::Exception (L"Attempt to rename link type '" + typeName +
                              L"' and create it at the same time");

    // Validate type of version we check against
    if (chkType.size() == 0)
        throw cdr::Exception(
                L"Missing target version type of 'P', 'V', or 'C'");
    else if (chkType != L"P" && chkType != L"V" && chkType != L"C")
        throw cdr::Exception(
                L"Expecting target version type=P V or C \"" + chkType +
                L"\" invalid");

    // Rest of operations must be processed as a unit
    autoCommitted = conn.getAutoCommit();
    conn.setAutoCommit(false);

    // Find out if we have a link type with this name already
    cdr::db::PreparedStatement tstmt = conn.prepareStatement (
        "SELECT id, name FROM link_type WHERE name = ?");
    tstmt.setString (1, typeName);
    cdr::db::ResultSet rs = tstmt.executeQuery ();

    // If no match on link type name
    if (!rs.next()) {
        // Validate that caller thinks he's adding a new link type
        if (!newType)
            throw cdr::Exception (L"Attempt to modify non-existent link type '"
                                  + typeName + L"'");

        // Create insertion statement for new link type
        updqry="INSERT INTO link_type (name,chk_type,comment) VALUES (?,?,?)";
    }
    else {
        // Validate that caller thinks he's modifying an existing link type
        if (newType)
            throw cdr::Exception (L"Attempt to add link type '" + typeName +
                                  L"' but it already exists");

        // Retrieve info for this element
        linkId = rs.getInt (1);

        // Create update statement for new link type
        updqry = "UPDATE link_type SET name = ?, chk_type = ?, comment = ? "
                 " WHERE id = ?";
    }
    tstmt.close();

    // Create or update the link_type record
    cdr::db::PreparedStatement ustmt = conn.prepareStatement (updqry);
    if (newType) {
        ustmt.setString (1, typeName);
        ustmt.setString (2, chkType);
        ustmt.setString (3, comment);
    }
    else {
        ustmt.setString (1, typeName);
        ustmt.setString (2, chkType);
        ustmt.setString (3, comment);
        ustmt.setInt    (4, linkId);
    }
    ustmt.executeUpdate();
    ustmt.close();

    // If we created a new link type, get its id
    if (newType) {
        cdr::db::PreparedStatement nstmt = conn.prepareStatement (
            "SELECT id FROM link_type WHERE name = ?");
        nstmt.setString (1, typeName);
        cdr::db::ResultSet rs = nstmt.executeQuery ();
        if (!rs.next())
            throw cdr::Exception (L"Can't find new link type '" +
                                  typeName + L"' - can't happen");
        linkId = rs.getInt (1);
        nstmt.close();
    }

    // Delete all rows in link control tables that use this type
    // They'll be replaced with data from the transaction
    int i = 0;
    while (S_delLinkQuery[i].size() > 0) {
        cdr::db::PreparedStatement pstmt =
                 conn.prepareStatement (S_delLinkQuery[i]);
        pstmt.setInt (1, linkId);
        pstmt.executeQuery();
        pstmt.close();
        ++i;
    }

    // Process the rest of the transaction, restarting at the top
    child = node.getFirstChild();
    bool gotTargetDocType = false;
    while (child != 0) {
        if (child.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
            name = child.getNodeName();

            // Field from which link is allowed
            if (name == L"LinkSource")
                addLinkSource (child, conn, linkId);

            // Document type we can link to
            if (name == L"TargetDocType") {
                addLinkTarget (cdr::dom::getTextContent (child), conn, linkId);
                gotTargetDocType = true;
            }

            // Special Properties for this link
            if (name == L"LinkProperties")
                addLinkProperty (child, conn, linkId);
        }
        child = child.getNextSibling();
    }

    // A target document type was required
    if (!gotTargetDocType)
            throw cdr::Exception (
                L"No target document type specified for link type '" +
                 typeName + L"'");

    // Restore autocommit status if required
    conn.setAutoCommit(autoCommitted);

    // Not recognizing any errors at the moment, just exceptions
    return (L"<" + cmdName + L"Resp/>");

} // putLinkType()


// See comments in CdrCommand.h

cdr::String cdr::getLinkType (
    cdr::Session&         session,  // User session info
    const cdr::dom::Node& node,     // DOM tree for transaction
    cdr::db::Connection&  conn      // Connection to CDR database
) {
    cdr::dom::Node child;       // Element in command
    cdr::String    linkName,    // Name of the link type
                   linkChkType, // Comment in link_type table
                   comment;     // Comment in link_type table
    int            linkId,      // Internal link type id
                   count;       // Count of names, must be 1

    // Get the name of the type for which the user wants info
    count = 0;
    child = node.getFirstChild();
    while (child != 0) {
        if (child.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
            if (cdr::String (child.getNodeName()) != L"Name")
                throw cdr::Exception (L"Expecting Name element in command");
            linkName = cdr::dom::getTextContent (child);
            ++count;
        }
        child = child.getNextSibling();
    }

    if (count != 1)
        throw cdr::Exception (L"Expecting exactly one Name element in command");

    // Find internal link type id and comment for this name
    cdr::db::PreparedStatement istmt = conn.prepareStatement (
        "SELECT id, name, chk_type, comment FROM link_type WHERE name = ?");
    istmt.setString (1, linkName);
    cdr::db::ResultSet trs = istmt.executeQuery ();

    // Found?
    if (!trs.next())
        throw cdr::Exception (
                L"Name '" + linkName + L"' not found in link types");
    linkId      = trs.getInt(1);
    linkChkType = trs.getString(2);
    linkName    = trs.getString(3);
    comment     = trs.getString(4);

    // Output transaction header and definining elements
    cdr::String resp = "<CdrGetLinkTypeResp>\n";
    resp += L"  <Name>" + linkName + L"</Name>\n";
    resp += L"  <LinkChkType>" + linkName + L"</LinkChkType>\n";
    if (!comment.isNull())
        resp += L"  <LinkTypeComment>" + comment +  L"</LinkTypeComment>\n";

    // All doctype + element fields which can contain this link
    cdr::db::PreparedStatement rstmt = conn.prepareStatement (
                  "SELECT doc_type.name,"
                  "       link_xml.element"
                  "  FROM doc_type,"
                  "       link_xml"
                  " WHERE link_xml.doc_type = doc_type.id"
                  "   AND link_xml.link_id  = ?");
    rstmt.setInt (1, linkId);
    cdr::db::ResultSet srs = rstmt.executeQuery();

    while (srs.next()) {
        resp += L"  <LinkSource>\n";
        resp += L"    " + cdr::tagWrap (srs.getString(1), "SrcDocType") + L"\n";
        resp += L"    " + cdr::tagWrap (srs.getString(2), "SrcField") + L"\n";
        resp += L"  </LinkSource>\n";
    }

    // Target doc types
    cdr::db::PreparedStatement dt_stmt = conn.prepareStatement (
        "SELECT doc_type.name"
        "  FROM doc_type,"
        "       link_target"
        " WHERE link_target.source_link_type = ?"
        "   AND link_target.target_doc_type  = doc_type.id");
    dt_stmt.setInt (1, linkId);
    cdr::db::ResultSet xrs = dt_stmt.executeQuery();

    while (xrs.next())
        resp += L"  <TargetDocType>" + xrs.getString(1) + L"</TargetDocType>\n";

    // Link properties
    cdr::db::PreparedStatement pstmt = conn.prepareStatement (
        "SELECT pt.name, p.value, p.comment "
        "FROM link_prop_type pt, link_properties p "
        "WHERE p.link_id = ? AND p.property_id = pt.id");
    pstmt.setInt (1, linkId);
    cdr::db::ResultSet prs = pstmt.executeQuery();

    cdr::String propComment;
    while (prs.next()) {
        resp += L"  <LinkProperties>\n";
        resp += L"    " + cdr::tagWrap (prs.getString(1), "LinkProperty")
             + L"\n";
        resp += L"    " + cdr::tagWrap (prs.getString(2), "PropertyValue")
             + L"\n";
        propComment = prs.getString (3);
        if (!(propComment.isNull()))
            resp += L"    " + cdr::tagWrap (propComment, "PropertyComment")
                 + L"\n";
        resp += L"  </LinkProperties>\n";
    }

    // Terminator
    resp += L"</CdrGetLinkTypeResp>";

    return resp;

} // getLinktype()


// See comments in CdrCommand.h

cdr::String cdr::listLinkTypes (
    cdr::Session&         session,  // User session info
    const cdr::dom::Node& node,     // DOM tree for transaction
    cdr::db::Connection&  conn      // Connection to CDR database
) {

    // Find them all
    cdr::db::Statement stmt = conn.createStatement();
    char *qry = "SELECT name FROM link_type";
    cdr::db::ResultSet rs = stmt.executeQuery (qry);

    // Output transaction header
    cdr::String resp = L"<CdrListLinkTypesResp>\n";

    // Output each one
    while (rs.next())
        resp += L"  " + cdr::tagWrap (rs.getString(1), L"Name") + L"\n";

    // Terminator
    resp += L"</CdrListLinkTypesResp>";

    return resp;

} // listLinkTypes()


// See comments in CdrCommand.h

cdr::String cdr::listLinkProps (
    cdr::Session&         session,  // User session info
    const cdr::dom::Node& node,     // DOM tree for transaction
    cdr::db::Connection&  conn      // Connection to CDR database
) {
    // Find them
    cdr::db::Statement stmt = conn.createStatement();
    char *qry = "SELECT name, comment FROM link_prop_type";
    cdr::db::ResultSet rs = stmt.executeQuery (qry);

    // Output transaction header
    cdr::String resp = L"<CdrListLinkPropsResp>\n";

    // Output each one
    cdr::String comment;
    while (rs.next()) {
        resp += L"  <LinkProperty>/n";
        resp += L"    " + cdr::tagWrap (rs.getString(1), L"Name") + L"\n";
        comment = rs.getString (2);
        if (!(comment.isNull()))
            resp += L"    " + cdr::tagWrap (comment, L"Comment")
                 + L"\n";
        resp += L"  </LinkProperty>/n";
    }

    // Terminator
    resp += L"</CdrListLinkPropsResp>";

    return resp;

} // listLinkProps()


/**
 * Add link source information to the link_xml table
 *
 *  @param  topNode     Reference to node for LinkSource element
 *  @param  conn        Database connection
 *  @param  typeId      Type of link, references link_type
 *  @return void
 *  @exception cdr::Exception if database error.
 */

static void addLinkSource (
    cdr::dom::Node&      topNode,
    cdr::db::Connection& conn,
    int                  typeId
) {
    cdr::dom::Node node;            // Single node within transaction
    cdr::String    name,            // Element name
                   srcDocType=L"",  // Name of doc type which can have link
                   srcField  =L"";  // Field which can have it


    // Descend to get source doc type and field
    node = topNode.getFirstChild();
    while (node != 0) {
        if (node.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
            name = node.getNodeName();

            // Doc type which can contain this link
            if (name == L"SrcDocType")
                srcDocType = cdr::dom::getTextContent (node);

            // Field
            else if (name == L"SrcField")
                srcField = cdr::dom::getTextContent (node);

            else
                throw cdr::Exception (L"Unknown subelement '" + name +
                                      L"' in LinkSource field");
            node = node.getNextSibling();
        }
    }

    // Got required subelements?
    if (srcDocType.size() == 0)
        throw cdr::Exception (L"Missing required SrcDocType field "
                              L"in LinkSource");
    if (srcField.size() == 0)
        throw cdr::Exception (L"Missing required SrcField field "
                              L"in LinkSource");

    // Make sure we don't already have a link type defined for this
    //   doc_type + element name.
    // We aren't supporting creation of two entries in the link_net
    //   for one actual link.
    std::string chkQry = "SELECT l.name FROM link_type l "
                         "  JOIN link_xml x "
                         "    ON l.id = x.link_id "
                         "  JOIN doc_type t "
                         "    ON t.id = x.doc_type "
                         " WHERE t.name = ? "
                         "   AND x.element = ?";
    cdr::db::PreparedStatement cstmt = conn.prepareStatement (chkQry);
    cstmt.setString (1, srcDocType);
    cstmt.setString (2, srcField);
    cdr::db::ResultSet chkRs = cstmt.executeQuery();
    if (chkRs.next()) {
        cdr::String existingName = chkRs.getString (1);
        cstmt.close();
        throw cdr::Exception (L"Link type \"" + existingName +
              L"\" is already defined for doctype=\"" + srcDocType +
              L"\" and element name=\"" + srcField + L"\"");
    }
    cstmt.close();

    // Failures here shouldn't happen through the
    //  admin interface, so I won't bother to catch
    //  and analyze any exceptions.
    // Anyway, if an exception is thrown, this is all inside a transaction
    // I'll change this if it turns out to be a problem.
    std::string qry = "INSERT INTO link_xml "
                      "(element, link_id, doc_type) "
                      "SELECT ?, ?, id FROM doc_type "
                      "WHERE doc_type.name = ?";
    cdr::db::PreparedStatement pstmt = conn.prepareStatement (qry);
    pstmt.setString (1, srcField);
    pstmt.setInt (2, typeId);
    pstmt.setString (3, srcDocType);
    pstmt.executeQuery();
    pstmt.close();

} // addLinkSource()


/**
 * Add link property information to the link_properties table
 *
 *  @param  topNode     Reference to node for LinkProperties element
 *  @param  conn        Database connection
 *  @param  typeId      Type of link, references link_type
 *  @return void
 *  @exception cdr::Exception if database error.
 */

static void addLinkProperty (
    cdr::dom::Node&      topNode,
    cdr::db::Connection& conn,
    int                  typeId
) {
    cdr::dom::Node node;            // Single node within transaction
    cdr::String    name,            // Name of element
                   linkProp  = L"", // Name of link property
                   propValue = L"", // Value passed to property procedure
                   comment   = L""; // Optional comment


    // Descend to get subelements
    node = topNode.getFirstChild();
    while (node != 0) {
        if (node.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
            name = node.getNodeName();

            // Doc type which can contain this link
            if (name == L"LinkProperty")
                linkProp = cdr::dom::getTextContent (node);

            // Field
            else if (name == L"PropertyValue")
                propValue = cdr::dom::getTextContent (node);

            // Optional comment
            else if (name == L"Comment")
                comment = cdr::dom::getTextContent (node);

            else
                throw cdr::Exception (L"Unknown subelement '" + name +
                                      L"' in LinkProperties field");
            node = node.getNextSibling();
        }
    }

    // Got required subelements?
    if (linkProp.size() == 0)
        throw cdr::Exception (L"Missing required LinkProperty field "
                              L"in LinkProperties");
    if (propValue.size() == 0)
        throw cdr::Exception (L"Missing required PropertyValue field "
                              L"in LinkProperties");

    // Again, failures shouldn't happen
    std::string qry = "INSERT INTO link_properties"
                      "(link_id, value, comment, property_id) "
                      "SELECT ?, ?, ?, id FROM link_prop_type "
                      "WHERE link_prop_type.name = ?";
    cdr::db::PreparedStatement pstmt = conn.prepareStatement (qry);
    pstmt.setInt (1, typeId);
    pstmt.setString (2, propValue);
    pstmt.setString (3, comment);
    pstmt.setString (4, linkProp);
    pstmt.executeQuery();
    pstmt.close();

} // addLinkProperty()


/**
 * Add link target information to the link_target table
 *
 *  @param  targName    Reference to name of target document type
 *  @param  conn        Database connection
 *  @param  typeId      Type of link, references link_type
 *  @return void
 *  @exception cdr::Exception if database error.
 */
static void addLinkTarget (
    cdr::String&         targName,
    cdr::db::Connection& conn,
    int                  typeId
) {
    cdr::dom::Node node;            // Single node within transaction
    cdr::String    linkProp  = L"", // Name of link property
                   propValue = L"", // Value passed to property procedure
                   comment   = L""; // Optional comment

    // This one is straightforward
    std::string qry = "INSERT INTO link_target "
                      "(source_link_type, target_doc_type) "
                      "SELECT ?, id FROM doc_type "
                      "WHERE doc_type.name=?";
    cdr::db::PreparedStatement pstmt = conn.prepareStatement (qry);
    pstmt.setInt (1, typeId);
    pstmt.setString (2, targName);
    pstmt.executeQuery();
    pstmt.close();

} // addLinkTarget()


/**
 * Traverse the tree of nodes, constructing a CdrLink object for each
 * node which contains a link attribute.
 *
 *  @param      conn        Reference to database connection.
 *  @param      topNode     Reference to the top node of the DOM parse
 *                           tree for the XML document.
 *  @param      docId       CDR document ID for XML document containing node.
 *  @param      docType     Document type for containing document.
 *  @param      docTypeStr  String name of document type.  As an
 *                           optimization, we retrieve this only once and
 *                           pass it in to linkTree.
 *  @param      lnkList     Reference to std::list of link objects.  We append
 *                           a new one each time we find a linking attribute
 *                           in a node.
 *  @param      uniqSet     Reference to a set of strings containing
 *                           source field + url, so we can avoid creating
 *                           useless duplicate link records.
 *  @param      fragSet     Reference to a list of cdr:id attribute
 *                           values which can be used as targets of links.
 *  @return                 Number of objects in list.
 */

static int linkTree (
    cdr::db::Connection&    conn,
    const cdr::dom::Node&   topNode,
    int                     docId,
    int                     docType,
    cdr::String&            docTypeStr,
    cdr::link::LnkList&     lnkList,
    cdr::StringSet&         uniqSet,
    cdr::StringSet&         fragSet,
    cdr::ValidationControl& errCtl
) {
    int  link_count = 0;            // Number of links found
    cdr::String frag,               // Fragment identifier in node
                uniq,               // Enough info to find duplicates
                refAttr,            // Link attribute value
                refValue;           // Copy refAttr to here for one we want
    cdr::link::LinkStyle lnkStyle;  // ref, xref, etc.
    bool        multiLinkErr;       // True=Two or more links in one element


    // Return value from set insertion
    std::pair<cdr::StringSet::iterator, bool> ins_stat;

    // Process this node and it's siblings, while there are any
    cdr::dom::Node node = topNode;
    while (node != 0) {

        // Only concerned with element nodes
        if (node.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
            const cdr::dom::Element& element
                     = static_cast<const cdr::dom::Element&> (node);

            // Don't know what kind of link we have yet or how many
            lnkStyle     = cdr::link::not_set;
            multiLinkErr = false;

            // Does this node contain a link
            // Check for all three types
            // But only one is allowed
            refValue = element.getAttribute (L"cdr:ref");
            if (refValue.size() > 0)
                lnkStyle = cdr::link::ref;

            refAttr = element.getAttribute (L"cdr:href");
            if (refAttr.size() > 0) {
                if (lnkStyle == cdr::link::not_set) {
                    lnkStyle = cdr::link::href;
                    refValue = refAttr;
                }
                else
                    multiLinkErr = true;
            }

            refAttr = element.getAttribute (L"cdr:xref");
            if (refAttr.size() > 0) {
                if (lnkStyle == cdr::link::not_set) {
                    lnkStyle = cdr::link::xhref;
                    refValue = refAttr;
                }
                else
                    multiLinkErr = true;
            }

            // If we found either of the two types of internal link
            if (lnkStyle == cdr::link::ref ||
                lnkStyle == cdr::link::href) {

                // Create a link object for this node
                cdr::link::CdrLink lnk (conn, node, docId, docType,
                                        docTypeStr, refValue, lnkStyle);


                // Is it a duplicate of a link object we've already seen?
                // Can happen if two occurrences of a field both link
                //   to the same object.
                // This might be legitimate, so we don't forbid it, but
                //   neither do we store duplicate link records or, if
                //   they are invalid, report duplicate errors.
                // Two links are duplicates if they come from the same
                //   field tag and point to the same url.
                uniq     = lnk.getSrcField() + L"+" + lnk.getRef();
                ins_stat = uniqSet.insert (uniq);

                // If it duplicates another link, we don't store it
                if (!ins_stat.second)
                    lnk.setSaveLink (false);

                // If there was more than one link attribute in the element
                // record the error
                if (multiLinkErr)
                    lnk.addLinkErr (
                         L"Can only have one link from a single element");

                // Add it to the list of links
                lnkList.push_back (lnk);
                ++link_count;
            }

            // Does the node have an id attribute which can be the
            //   target of an external link?
            frag = element.getAttribute (L"cdr:id");
            if (frag.size() > 0) {

                // Attempt insertion
                ins_stat = fragSet.insert (frag);

                // If it was already there, declare an error
                if (!ins_stat.second)
                    errCtl.addError (L"cdr:id \"" + frag +
                                       L"\" used more than once");
            }

            // Does the node have children?
            cdr::dom::Node child = node.getFirstChild();
            if (child != 0) {
                // Process subtree
                link_count += linkTree (conn, child, docId, docType,
                                        docTypeStr, lnkList, uniqSet,
                                        fragSet, errCtl);
            }
        }

        // Are there siblings?
        node = node.getNextSibling();
    }

    return link_count;

} // linkTree()

/**
 * Update the link_net table in the database.
 *
 *  @param      conn        Reference to database connection.
 *  @param      docId       CDR document ID for XML document containing node.
 *  @param      lnkList     Reference to std::list of link objects.  We append
 *                           a new one each time we find a linking attribute
 *                           in a node.
 *  @return                 Void.
 */

void cdr::link::updateLinkNet (
    cdr::db::Connection& conn,
    int                  docId,
    cdr::link::LnkList&  lnkList
) {
    MAKE_TIMER(linkTimer);

    // Build a set of the rows which should be in the table.
    LinkNetRowSet newRows, oldRows, wantedRows, unwantedRows;
    cdr::link::LnkList::iterator linkIter;
    for (linkIter = lnkList.begin(); linkIter != lnkList.end(); ++linkIter) {
        cdr::String targetFrag(true);

        // Link objects may have been created for erroneous links that
        //   can/should not be saved.  They are in the list because
        //   the associated error messages are useful, but they must
        //   not be written to the database.
        if (linkIter->getSaveLink() == false)
            continue;

        if (linkIter->getTrgFrag().size() > 0)
            targetFrag = linkIter->getTrgFrag();
        LinkNetRow row(linkIter->getType(),
                       linkIter->getSrcId(),
                       linkIter->getSrcField(),
                       linkIter->getTrgId(),
                       targetFrag,
                       linkIter->getRef());
        newRows.insert(row);
    }

    // Build a set of the rows which are in the table now.
    cdr::db::PreparedStatement getLinksStmt = conn.prepareStatement(
            "SELECT link_type, source_elem, target_doc, target_frag, url"
            "  FROM link_net                                            "
            " WHERE source_doc = ?                                      ");
    getLinksStmt.setInt(1, docId);
    cdr::db::ResultSet getLinksResults = getLinksStmt.executeQuery();
    while (getLinksResults.next()) {
        int         type     = getLinksResults.getInt   (1);
        cdr::String elem     = getLinksResults.getString(2);
        cdr::Int    targDoc  = getLinksResults.getInt   (3);
        cdr::String targFrag = getLinksResults.getString(4);
        if (targFrag.isNull())
            targFrag == cdr::link::NO_FRAGMENT;
        cdr::String url      = getLinksResults.getString(5);
        LinkNetRow row(type, docId, elem, targDoc, targFrag, url);
        oldRows.insert(row);
    }

    // Build sets for the deltas (rows to be inserted, deleted).
    LinkNetRowSet::const_iterator rsi;
    for (rsi = newRows.begin(); rsi != newRows.end(); ++rsi)
        if (oldRows.find(*rsi) == oldRows.end())
            wantedRows.insert(*rsi);
    for (rsi = oldRows.begin(); rsi != oldRows.end(); ++rsi)
        if (newRows.find(*rsi) == newRows.end())
            unwantedRows.insert(*rsi);
    SHOW_ELAPSED("built delta link sets", linkTimer);

    // If delta too large, just start from scratch.
    LinkNetRowSet* rowsToInsert = NULL;
    if (unwantedRows.size() > 500 &&
        unwantedRows.size() > newRows.size() / 2) {

        const char* query = "DELETE FROM link_net WHERE source_doc = ?";
        cdr::db::PreparedStatement delStmt = conn.prepareStatement(query);
        delStmt.setInt(1, docId);
        delStmt.executeUpdate();
        rowsToInsert = &newRows;
    }
    else {
        // Otherwise, delete just the rows which are no longer needed.
        // We need two different prepared statements to do our deletes.
        // One is used when the target fragment is null, and one when it's not.
        // The alternative is to build the statement dynamically and prepare
        //   it separately for each row - which is more expensive.
        // We're probably optimizing too much here but, here it is anyway.

        // Two statements one for links with a fragment and one without
        cdr::db::PreparedStatement delStmtFrag = conn.prepareStatement(
            "DELETE FROM link_net       "
            "      WHERE source_doc  = ?"
            "        AND link_type   = ?"
            "        AND source_elem = ?"
            "        AND target_doc  = ?"
            "        AND target_frag = ?"
            "        AND url         = ?");
        cdr::db::PreparedStatement delStmtNullFrag = conn.prepareStatement(
            "DELETE FROM link_net       "
            "      WHERE source_doc  = ?"
            "        AND link_type   = ?"
            "        AND source_elem = ?"
            "        AND target_doc  = ?"
            "        AND target_frag IS NULL"
            "        AND url         = ?");

        // Pointer to one or the other of them
        cdr::db::PreparedStatement *delStmt;

        // Delete each unwanted row in the table
        for (rsi = unwantedRows.begin(); rsi != unwantedRows.end(); ++rsi) {
            // Point to one or the other prepared statement
            if (rsi->target_frag == cdr::link::NO_FRAGMENT)
                delStmt = &delStmtNullFrag;
            else
                delStmt = &delStmtFrag;

            // Insert values into whichever statement we're using
            delStmt->setInt   (1, docId);
            delStmt->setInt   (2, rsi->link_type);
            delStmt->setString(3, rsi->source_elem);
            delStmt->setInt   (4, rsi->target_doc);
            if (rsi->target_frag == cdr::link::NO_FRAGMENT)
                delStmt->setString(5, rsi->url);
            else {
                delStmt->setString(5, rsi->target_frag);
                delStmt->setString(6, rsi->url);
            }
            delStmt->executeUpdate();
        }
        rowsToInsert = &wantedRows;
    }
    SHOW_ELAPSED("deleted unwanted link rows", linkTimer);

    // Insert the (new) links.
    cdr::db::PreparedStatement insStmt = conn.prepareStatement(
            "INSERT INTO link_net      "
            "            (source_doc,  "
            "             link_type,   "
            "             source_elem, "
            "             target_doc,  "
            "             target_frag, "
            "             url)         "
            "     VALUES (?,?,?,?,?,?) ");
    for (rsi = rowsToInsert->begin(); rsi != rowsToInsert->end(); ++rsi) {
        // XXX This is a stopgap measure.  Problems should be detected and
        //     reported when the link tree is built.
        if (rsi->target_frag.size() > 32) {
            cdr::log::pThreadLog->Write(L"updateLinkNet",
                    L"target_frag too long" + rsi->target_frag);
            continue;
        }
        if (rsi->url.size() > 256) {
            cdr::log::pThreadLog->Write(L"updateLinkNet",
                    L"url too long" + rsi->url);
            continue;
        }
        insStmt.setInt   (1, docId);
        insStmt.setInt   (2, rsi->link_type);
        insStmt.setString(3, rsi->source_elem);
        if (rsi->target_doc > 0)
            insStmt.setInt   (4, rsi->target_doc);
        else {
            cdr::Int nullTarg(true);
            insStmt.setInt (4, nullTarg);
        }
        insStmt.setString(5, rsi->target_frag);
        insStmt.setString(6, rsi->url);
        insStmt.executeUpdate();
    }
    SHOW_ELAPSED("inserted new link rows", linkTimer);

} // updateLinkNet()


/**
 * Check whether there are any links to fragments that don't exist
 *
 * We only check for missing fragments if we have a document id
 * A user might send us a document for validation without an ID,
 * e.g., if the document is created in a workstation or conversion
 * program and submitted for validation before it has ever been
 * stored.
 *
 *  @param      conn        Reference to database connection.
 *  @param      docId       CDR document ID for XML document containing node.
 *  @param      validRule   Relationship between validation and update.
 *                           If updating, we mark any links/docs as invalid
 *                           if they point to a non-existent fragment.
 *  @param      fragSet     Reference to cdr::StringSet of fragment texts.
 *                           These are identifiers stored in cdr:ref
 *                           attributes in a document.
 *  @param      errCtl      Append errors here.
 *                           Should be pre-loaded with the current context
 *                           set at the document element.
 *  @return                 Void.
 */

static int checkMissedFrags (
    cdr::db::Connection&    conn,
    int                     docId,
    cdr::ValidRule          validRule,
    cdr::StringSet&         fragSet,
    cdr::ValidationControl& errCtl
) {
    std::string qry;            // SQL query for database access
    cdr::String frag,           // Fragment id of link to our doc
                lastFrag;       // Last one seen before current one
    int         srcId,          // ID of source doc for fraglink
                frag_errs,      // Count of errors for one missing fragment
                total_errs;     // Count of all errors


    // Don't check for links to a document that has no id, i.e., has
    //   never been stored.
    if (docId < 1)
        return 0;

    // Find all docs that link to any fragment in this doc
    qry = "SELECT target_frag, source_doc "
          "FROM   link_net "
          "WHERE  (target_doc = ? AND target_frag IS NOT NULL) "
          "ORDER BY target_frag";

    cdr::db::PreparedStatement stmt = conn.prepareStatement (qry);
    stmt.setInt (1, docId);
    cdr::db::ResultSet rs = stmt.executeQuery();

    // Start evaluating from scratch
    lastFrag   = L"";
    frag_errs  = 0;
    total_errs = 0;

    // Check each link fragment to this doc to be sure the frag exists
    while (rs.next()) {

        // Get data from the link
        frag  = rs.getString (1);
        srcId = rs.getInt    (2);

        // Is it pointing to a fragment actually in the current doc?
        if (fragSet.find (frag) == fragSet.end()) {

            // Not found, we have an error
            if (frag != lastFrag) {

                // Were there unreported errors?
                if (frag_errs > cdr::link::MaxFragMissErrors)
                    errCtl.addError (cdr::String::toString (frag_errs) +
                        L" documents link to missing cdr:ref '" +
                        frag + L"'.  Stopped listing them after " +
                        cdr::String::toString (cdr::link::MaxFragMissErrors) +
                        L" errors reported");

                // Ready for next
                lastFrag  = frag;
                frag_errs = 0;
            }

            // Report this particular error
            // But don't report the same error more than some max
            //   number of times
            if (frag_errs++ < cdr::link::MaxFragMissErrors)
                errCtl.addError (L"Document " +
                        cdr::String::toString (srcId) +
                        L" expects cdr:id='" + frag +
                        L"' but no such id found");

#if 0
THIS ISN'T A GOOD IDEA BECAUSE A USER CAN FIX THE SOURCE RECORD, LEAVING
THE TARGET MARKED BAD WHEN IT'S NOT.
THE ONLY RECORD MARKED SHOULD BE THE ONE WE'RE CURRENTLY VALIDATING.
            // Caller may be updating the database even if there are
            //   errors.  If so, must mark the remote source of the
            //   link as in error.
            if (validRule == cdr::UpdateUnconditionally) {
                qry = "UPDATE document "
                      "SET    val_status = ?, val_date GETDATE() "
                      "WHERE  id = ?";

                cdr::db::PreparedStatement stmt = conn.prepareStatement (qry);
                stmt.setInt (1, cdr::InvalidStatus);
                stmt.setInt (2, docId);
                stmt.executeQuery();
            }
#endif

            ++total_errs;
        }
    }

    return total_errs;

} // checkMissedFrags()


/**
 * Update link_fragment table in the database.
 *
 *  @param      conn        Reference to database connection.
 *  @param      docId       CDR document ID for XML document containing node.
 *  @param      fragSet     Reference to cdr::StringSet of fragment texts.
 *                           These are identifiers stored in cdr:ref
 *                           attributes in a document.
 *  @return                 Void.
 */

static void updateFragList (
    cdr::db::Connection& conn,
    int                  docId,
    cdr::StringSet&      fragSet
) {
    MAKE_TIMER(fragTimer);

    // Build a set of the rows that are current in the table for this document.
    cdr::db::PreparedStatement fragSelStmt = conn.prepareStatement(
            " SELECT fragment FROM link_fragment WHERE doc_id = ?");
    fragSelStmt.setInt(1, docId);
    cdr::StringSet oldFrags;
    cdr::db::ResultSet fragSelResults = fragSelStmt.executeQuery();
    while (fragSelResults.next()) {
        cdr::String frag = fragSelResults.getString(1);
        oldFrags.insert(frag);
    }

    // Build sets for the rows that need to be inserted or deleted.
    cdr::StringSet::const_iterator i;
    cdr::StringSet wantedRows, unwantedRows;
    for (i = fragSet.begin(); i != fragSet.end(); ++i)
        if (oldFrags.find(*i) == oldFrags.end())
            wantedRows.insert(*i);
    for (i = oldFrags.begin(); i != oldFrags.end(); ++i)
        if (fragSet.find(*i) == fragSet.end())
            unwantedRows.insert(*i);
    SHOW_ELAPSED("link_fragment delta sets built", fragTimer);

    // If selective deletion would be too expensive, start from scratch.
    cdr::StringSet* rowsToInsert = NULL;
    if (unwantedRows.size() > 500 &&
        unwantedRows.size() > fragSet.size() / 2) {

        const char* query = "DELETE FROM link_fragment WHERE doc_id = ?";
        cdr::db::PreparedStatement delStmt = conn.prepareStatement(query);
        delStmt.setInt(1, docId);
        delStmt.executeUpdate();
        rowsToInsert = &fragSet;
    }
    else {
        // Otherwise, just remove the rows we no long need.
        const char* query = " DELETE FROM link_fragment "
                            "       WHERE doc_id = ?    "
                            "         AND fragment = ?  ";
        cdr::db::PreparedStatement delStmt = conn.prepareStatement(query);
        for (i = unwantedRows.begin(); i != unwantedRows.end(); ++i) {
            delStmt.setInt(1, docId);
            delStmt.setString(2, *i);
            delStmt.executeUpdate();
        }
        rowsToInsert = &wantedRows;
    }
    SHOW_ELAPSED("link_fragment rows deleted", fragTimer);

    // Insert the rows we need.
    cdr::db::PreparedStatement insStmt = conn.prepareStatement(
            "INSERT INTO link_fragment(doc_id, fragment) "
            "     VALUES (?, ?)                          ");
    for (i = rowsToInsert->begin(); i != rowsToInsert->end(); ++i) {
        insStmt.setInt(1, docId);
        insStmt.setString(2, *i);
        insStmt.executeUpdate();
    }
    SHOW_ELAPSED("link_fragment rows inserted", fragTimer);

} // updateFragList()


void cdr::link::findTargetDocTypes (
        cdr::db::Connection&    conn,
        const cdr::String&      srcElem,
        const cdr::String&      srcDocType,
        std::vector<int>&       typeList)
{
    std::string qry = "SELECT lt.target_doc_type"
                      "  FROM link_target lt,"
                      "       link_xml lx,"
                      "       doc_type dt"
                      " WHERE lx.doc_type = dt.id"
                      "   AND lx.element = ?"
                      "   AND dt.name = ?"
                      "   AND lt.source_link_type = lx.link_id";
    typeList.clear();
    cdr::db::PreparedStatement stmt = conn.prepareStatement(qry);
    stmt.setString(1, srcElem);
    stmt.setString(2, srcDocType);
    cdr::db::ResultSet rs = stmt.executeQuery();
    while (rs.next())
        typeList.push_back(rs.getInt(1));

} // findTargetDocTypes()


/**
 * findDocType
 *
 * Find the document type id and name corresponding to a document id.
 *
 *  @param  dbConn      Database connection.
 *  @param  docId       Find doc type info for this document.
 *  @param  typeIdp     Pointer to int to receive type id.
 *  @param  typeStrp    Pointer to string receive type name.
 *
 *  @return true        Success, false=document or type not found.
 */

static bool findDocType (
    cdr::db::Connection& dbConn,
    int                  docId,
    int                  *typeIdp,
    cdr::String          *typeStrp
) {
    std::string qry = "SELECT t.id, t.name FROM doc_type t JOIN document d "
                      "    ON t.id = d.doc_type "
                      " WHERE d.id = ?";
    cdr::db::PreparedStatement stmt = dbConn.prepareStatement (qry);
    stmt.setInt (1, docId);
    cdr::db::ResultSet rs = stmt.executeQuery();
    if (!rs.next())
        return false;

    // Get info
    *typeIdp  = rs.getInt (1);
    *typeStrp = rs.getString (2);

    return true;

} // findDocType()

cdr::String cdr::link::getSearchLinksResp (
        cdr::db::Connection&    conn,
        const cdr::String&      srcElem,
        const cdr::String&      srcDocType,
        const cdr::String&      titlePattern,
        int                     maxRows)
{
    // Get link_id first. We will get one most of the time.
    int link_id = 0;
    std::string qry = "SELECT lx.link_id"
                      "  FROM link_xml lx,"
                      "       doc_type dt"
                      " WHERE lx.doc_type = dt.id"
                      "   AND lx.element = ?"
                      "   AND dt.name = ?";
    cdr::db::PreparedStatement stmt = conn.prepareStatement(qry);
    stmt.setString(1, srcElem);
    stmt.setString(2, srcDocType);
    cdr::db::ResultSet rs = stmt.executeQuery();
    while (rs.next()) {
        link_id = rs.getInt(1);
    }
    if (link_id == 0)
        throw cdr::Exception(L"No links permitted from this element");

    // Get link_property ids and values if exist.
    qry = "SELECT lp.property_id, lp.value, lpt.name"
                      "  FROM link_properties lp,"
                      "       link_prop_type lpt"
                      " WHERE lpt.id = lp.property_id"
                      "   AND lp.link_id = ?";
    std::vector<int>         prop_ids;
    std::vector<cdr::String> prop_values;
    cdr::db::PreparedStatement stmt2 = conn.prepareStatement(qry);
    stmt2.setInt(1, link_id);
    cdr::db::ResultSet rs2 = stmt2.executeQuery();
    while (rs2.next()) {
        int         prop_id    = rs2.getInt(1);
        cdr::String prop_value = rs2.getString(2);
        cdr::String prop_name  = rs2.getString(3);

        // We can only handle one property type at the moment.
        if (prop_name != L"LinkTargetContains")
            throw cdr::Exception (L"Not supporting link property type: "
                                  + prop_name);

        // We can only handle property ids with values.
        if (prop_value == L"")
            throw cdr::Exception (L"Not supporting null property value");

        prop_ids.push_back(prop_id);
        prop_values.push_back(prop_value);
    }

    // There is at least one link property for this link.
    if (prop_ids.size() > 0)
        return getSearchLinksRespWithProp(conn, link_id, prop_ids,
                                          prop_values, titlePattern,
                                          maxRows);

    // There is no link property for this link.
    qry = "SELECT ";
    if (maxRows > 0) {
        char buf[40];
        sprintf(buf, "TOP %d ", maxRows);
        qry += buf;
    }
    qry += "          d.id, d.title"
           "  FROM    document d, link_target lt"
           " WHERE    d.doc_type = lt.target_doc_type"
           "   AND    d.title LIKE ?"
           "   AND    lt.source_link_type = ?"
           " ORDER BY d.title";

    // Submit the query to the DBMS.
    cdr::db::PreparedStatement stmt3 = conn.prepareStatement(qry);
    stmt3.setString(1, titlePattern);
    stmt3.setInt(2, link_id);
    cdr::db::ResultSet rs3 = stmt3.executeQuery();

    // Construct the response.
    cdr::String response = L"<CdrSearchLinksResp>";
    int rows = 0;
    while (rs3.next()) {
        int         id      = rs3.getInt(1);
        cdr::String title   = rs3.getString(2);

        if (rows++ == 0)
            response += L"<QueryResults>";
        wchar_t tmp[1000];
        swprintf(tmp, L"<QueryResult><DocId>CDR%010ld</DocId>"
                      L"<DocTitle>%.500s</DocTitle>"
                      L"</QueryResult>",
                 id, title.c_str());
        response += tmp;
    }
    if (rows > 0)
        response += L"</QueryResults></CdrSearchLinksResp>";
    else
        response += L"<QueryResults/></CdrSearchLinksResp>";
    return response;

} // getSearchLinksResp()

/**
 * Finds the denormalized text for a CDR link if the link can be pasted
 * given the current context.
 *
 * XXX This is Bob's stub version.  Alan will supply the real version which
 * knows about fragment links.
 *
 * Takes standard parameters for a CdrCommand.
 */

cdr::String cdr::pasteLink (
    cdr::Session&         session,
    const cdr::dom::Node& commandNode,
    cdr::db::Connection&  dbConnection
) {
    String sourceDocType;
    String sourceElemName;
    String targetDocId;
    String targetFragId; // Temporary version ignores this.

    // Extract parameters from command.
    cdr::dom::Node node = commandNode.getFirstChild();
    while (node != 0) {

        if (node.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
            String name = node.getNodeName();
            if (name == L"SourceDocType")
                sourceDocType = dom::getTextContent(node);
            else if (name == L"SourceElementType")
                sourceElemName = dom::getTextContent(node);
            else if (name == L"TargetDocId")
                targetDocId = dom::getTextContent(node);
            else if (name == L"TargetFragmentId")
                targetFragId = dom::getTextContent(node);
            else
                throw Exception(L"pasteLink: Element '" +
                                name +
                                L"' in CdrPasteLink request is "
                                L"currently unsupported");
        }
        node = node.getNextSibling();
    }

    // Check for required elements.
    if (sourceDocType.empty())
        throw Exception(L"Required SourceDocType element missing");
    if (sourceElemName.empty())
        throw Exception(L"Required SourceDocType element missing");
    if (targetDocId.empty())
        throw Exception(L"Required TargetDocId element missing");

    // Make sure the proposed linking combination is valid.
    int docId = targetDocId.extractDocId();
    const char* query = "SELECT COUNT(*)                       "
                        "  FROM doc_type sdt                   "
                        "  JOIN link_xml lx                    "
                        "    ON sdt.id = lx.doc_type           "
                        "  JOIN link_type lt                   "
                        "    ON lt.id = lx.link_id             "
                        "  JOIN link_target ltarg              "
                        "    ON ltarg.source_link_type = lt.id "
                        "  JOIN doc_type tdt                   "
                        "    ON tdt.id = ltarg.target_doc_type "
                        "  JOIN document d                     "
                        "    ON d.doc_type = tdt.id            "
                        " WHERE sdt.name = ?                   "
                        "   AND lx.element = ?                 "
                        "   AND d.id = ?                       ";
    db::PreparedStatement stmt1 = dbConnection.prepareStatement(query);
    stmt1.setString(1, sourceDocType);
    stmt1.setString(2, sourceElemName);
    stmt1.setInt(3, docId);
    db::ResultSet rs1 = stmt1.executeQuery();
    if (!rs1.next())
        throw Exception(L"Failure checking link paste validity");
    if (rs1.getInt(1) < 1) {
        String err = L"Link from "
                   + sourceElemName
                   + L" elements of "
                   + sourceDocType
                   + L" documents to document "
                   + targetDocId
                   + L" not permitted.";
        throw Exception(err);
    }

    // Get the denormalized data.
    query = "SELECT title FROM document WHERE id = ?";
    db::PreparedStatement stmt2 = dbConnection.prepareStatement(query);
    stmt2.setInt(1, docId);
    db::ResultSet rs2 = stmt2.executeQuery();
    if (!rs2.next())
        throw Exception(L"Failure fetching denormalized data");
    String denormalizedData = rs2.getString(1);
    if (denormalizedData.empty())
        throw Exception(L"Denormalized data empty");

    // Send back the response.
    String rsp = L"<CdrPasteLinkResp><DenormalizedContent>"
               + denormalizedData
               + L"</DenormalizedContent></CdrPasteLinkResp>";
    return rsp;

} // pasteLink()