/*
 * $Id: CdrLink.h,v 1.1 2000-09-26 19:05:00 ameyer Exp $
 *
 * Header for Link Module software - to maintain the link_net
 * table describing the link relationships among CDR documents.
 *
 * $Log: not supported by cvs2svn $
 */

#ifndef CDR_LINK_
#define CDR_LINK_

#include "CdrString.h"
#include "CdrValidateDoc.h"
#include "CdrDbConnection.h"

/**@#-*/

namespace cdr {
  namespace link {

/**@#+*/

    /** @pkg cdr */

    /**
     * Max number of errors we report for one missing fragment id.
     * This is so that if a fragment identifier is deleted from a
     *   document and 1000 other documents reference it, we don't
     *   report 1000 errors.
     */
    const int MaxFragMissErrors = 5;

    /**
     * Possible linking styles
     */
    typedef enum LinkStyle {
        not_set,        // Don't yet know what it is
        ref,            // Link via cdr:ref attribute
        href,           // cdr:href - retains element content
        xhref           // xlink:href - no validation
    } LinkStyle;

    /**
     * List of CdrLink objects.
     * Requires forward declaration of CdrLink class.
     */
    class CdrLink;
    typedef std::list<CdrLink> LnkList;

    /**
     * Class for manipulating link_net entries.
     */

    class CdrLink {
        public:
            /**
             * Constructor.
             *
             * The main purpose of the constructor is to capture
             * a description of the link.  However some validation
             * is also done here.  Errors generated by the link
             * capture itself are produced here instead of delayed
             * for later re-checking.
             *
             * @param   conn        Connection to the database.
             * @param   elemDom     DOM node for element containing link.
             * @param   docId       ID of source document containing link.
             * @param   docType     Document type identifier for source doc.
             * @param   docTypeStr  String form, to save getting it again.
             * @param   refValue    The reference itself, e.g., "CDR1234..."
             * @param   lnkStyle    Type of attribute containing link.
             */
            CdrLink (cdr::db::Connection&, cdr::dom::Node&, int, int,
                     cdr::String, cdr::String, LinkStyle);

            /**
             * Default copy constructor from compiler is fine.
             * Default destructor also fine.
             */

            /**
             * validateLink
             *
             * Performs all validations which can be performed on a
             * free-standing link.  Other routines than this one
             * perform any link validations which must be performed
             * on the document as a whole (e.g., does it have any
             * duplicate fragment ids, and contain any fragment ids
             * referred to by other documents.)
             *
             * @param  dbConn       Reference to database connection.
             * @return              Count of errors for this link so far.
             */
            int validateLink (cdr::db::Connection&);

            /**
             * dumpXML
             *
             * Retrieves all of the information we have about a link
             * and serializes it into a string of xml.
             *
             * @param  dbConn       Reference to database connection.
             * @return              XML string containing link info,
             *                      including any errors.
             */
            cdr::String CdrLink::dumpXML (cdr::db::Connection&);

            /**
             * dumpString
             *
             * Retrieves all of the information we have about a link
             * and serializes it into a human readable string.
             *
             * This is like dumpXML(), but does not contain XML tags,
             * using human readable formatting instead - more suitable
             * for human readable error messages.
             *
             * @param  dbConn       Reference to database connection.
             * @return              XML string containing link info,
             *                      including any errors.
             */
            cdr::String CdrLink::dumpString (cdr::db::Connection&);

            /**
             * Add an error message to a link object
             *
             * @param  msg          Error message to add.
             */
            void CdrLink::addLinkErr (cdr::String);

            /**
             * Set the link to not be saved in the event of certain errors
             *
             * @param  s            New saveLink value.
             */
            void CdrLink::setSaveLink (bool s) { saveLink = s; };

            /**
             * Accessors.
             * XXXX - Could modify a few of these to generate data
             *        when called, if it doesn't exist.
             */
            int         getType          (void) { return type; }
            cdr::String getTypeStr       (void) { return typeStr; }
            int         getSrcId         (void) { return srcId; }
            cdr::String getSrcField      (void) { return srcField; }
            int         getSrcDocType    (void) { return srcDocType; }
            cdr::String getSrcDocTypeStr (void) { return srcDocTypeStr; }
            int         getTrgId         (void) { return trgId; }
            cdr::String getTrgIdStr      (void) { return trgIdStr; }
            int         getTrgDocType    (void) { return trgDocType; }
            cdr::String getTrgDocTypeStr (void) { return trgDocTypeStr; }
            cdr::String getRef           (void) { return ref; }
            cdr::String getTrgFrag       (void) { return trgFrag; }
            cdr::dom::Node& getFldNode   (void) { return fldNode; }
            bool        getHasData       (void) { return hasData; }
            StringList  getErrList       (void) { return errList; }
            int         getErrCount      (void) { return errCount; }
            LinkStyle   getStyle         (void) { return style; }
            bool        getSaveLink      (void) { return saveLink; }


        private:
            int         type;           // From link_type table
            cdr::String typeStr;        // Human readable form of same
            int         srcId;          // Doc ID of source of link
            cdr::String srcField;       // Tag of element containing link
            int         srcDocType;     // From doc_type table
            cdr::String srcDocTypeStr;  // Human readable form
            int         trgId;          // Doc ID of target doc
            cdr::String trgIdStr;       // String format of target id
            int         trgDocType;     // Doc_type of target doc
            cdr::String trgDocTypeStr;  // String format
            cdr::String ref;            // Full reference as string
            cdr::String trgFrag;        // #Fragment part of ref, if any
            cdr::dom::Node& fldNode;    // Reference to DOM node in parsed xml
            bool        hasData;        // True=denormalized data in node
            LinkStyle   style;          // cdr:ref, cdr:href, xlink:href
            bool        saveLink;       // True=Save to link_net
            StringList  errList;        // List of error messages
            int         errCount;       // Number in errList
    };

    /**
     * Update the link net from a list of link objects
     */
    extern void updateLinkNet (cdr::db::Connection&, int, cdr::link::LnkList&);

    /**
     * Validate links in a document and, optionally, update the link_net
     * database by calling updateLinkNet().
     *
     * Called by execValidateDoc() for validation of new or modified docs.
     *
     *  @param      node        Reference to a DOM parse tree for the XML
     *                           of the document to be written to the database.
     *  @param      conn        Reference to the connection object for the
     *                           CDR database.
     *  @param      ui          Document UI, as an integer.
     *  @param      docTypeStr  Document type name from the doc_type table
     *                           for this document.  No checking is done
     *                           either to insure that it's a valid type or
     *                           that it's the right one for this document.
     *  @param      validRule   Relationship between validation and link_net
     *                           update.  Values are:
     *                             ValidateOnly
     *                             UpdateIfValid
     *                             UpdateUnconditionally
     *  @param      errlist     Reference to string to receive errors.
     *  @return                 Count of errors.
     *                           0 = complete success.
     *  @exception  cdr::Exception if a database or processing error occurs.
     */
    extern int CdrSetLinks (cdr::dom::Node&, cdr::db::Connection&,
                   int, cdr::String, cdr::ValidRule, cdr::StringList&);
  }
}

#endif // CDR_LINK_
