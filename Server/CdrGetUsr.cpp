/*
 * Retrieves information about requested user.
 */

#include "CdrCommand.h"
#include "CdrDbPreparedStatement.h"
#include "CdrDbResultSet.h"
#include "CdrString.h"

/**
 * Load the row from the usr table belonging to the specified user, as well as
 * the rows from the grp_usr table joined to the usr row.
 */
cdr::String cdr::getUsr(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& conn)
{
    // Extract the group name from the command.
    cdr::String usrName;
    cdr::dom::Node child = commandNode.getFirstChild();
    while (child != 0) {
        if (child.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
            cdr::String name = child.getNodeName();
            if (name == L"UserName")
                usrName = cdr::dom::getTextContent(child);
        }
        child = child.getNextSibling();
    }
    if (usrName.size() == 0)
        throw cdr::Exception(L"Missing user name");

    // Look up the base information for the user.
    std::string query =
        "SELECT id, name, fullname, office, email, phone, comment, authmode = "
        "  CASE"
        "      WHEN hashedpw IS NULL THEN 'network'"
        "      WHEN hashedpw = HASHBYTES('SHA1', '') THEN 'network'"
        "      ELSE 'local'"
        "   END"
        "  FROM usr"
        " WHERE name = ?";
    cdr::db::PreparedStatement usrQuery = conn.prepareStatement(query);
    usrQuery.setString(1, usrName);
    cdr::db::ResultSet usrRs = usrQuery.executeQuery();
    if (!usrRs.next())
        throw cdr::Exception(L"User not found", usrName);
    int         uid      = usrRs.getInt(1);
    cdr::String uName    = usrRs.getString(2);
    cdr::String fullname = usrRs.getString(3);
    cdr::String office   = usrRs.getString(4);
    cdr::String email    = usrRs.getString(5);
    cdr::String phone    = usrRs.getString(6);
    cdr::String comment  = usrRs.getString(7);
    cdr::String authMode = usrRs.getString(8);

    // Make sure our user is authorized to retrieve this information.
    // Any user can access his own info, but special authorization is
    //   required to access other users' info.
    // Note that user name info is case sensitive
    cdr::String sessionUsr = session.getUserName();
    if (uName != sessionUsr)
        if (!session.canDo(conn, L"GET USER", L""))
            throw cdr::Exception(
                    L"GET USER action not authorized for this user");

    // Initialize a successful response.
    cdr::String response = cdr::String(L"  <CdrGetUsrResp>\n"
                                       L"   <UserName>")
                                     + cdr::entConvert(uName)
                                     + L"</UserName>\n"
                                     + L"   <AuthenticationMode>"
                                     + authMode
                                     + L"</AuthenticationMode>\n";
    if (fullname.size() > 0)
        response += L"   <FullName>"
                 +  cdr::entConvert(fullname)
                 +  L"</FullName>\n";
    if (office.size() > 0)
        response += L"   <Office>"
                 +  cdr::entConvert(office)
                 +  L"</Office>\n";
    if (email.size() > 0)
        response += L"   <Email>"
                 +  cdr::entConvert(email)
                 +  L"</Email>\n";
    if (phone.size() > 0)
        response += L"   <Phone>"
                 +  cdr::entConvert(phone)
                 +  L"</Phone>\n";

    // Find the groups to which this user is assigned.
    query = "SELECT g.name"
            "  FROM grp g,"
            "       grp_usr gu"
            " WHERE gu.usr = ?"
            "   AND g.id   = gu.grp";
    cdr::db::PreparedStatement grpSelect = conn.prepareStatement(query);
    grpSelect.setInt(1, uid);
    cdr::db::ResultSet grpRs = grpSelect.executeQuery();
    while (grpRs.next())
        response += L"   <GrpName>" + cdr::entConvert(grpRs.getString(1))
                 +  L"</GrpName>\n";

    // Add the comment if present.
    if (!comment.isNull() && comment.size() > 0)
        response += L"   <Comment>" + cdr::entConvert(comment)+L"</Comment>\n";

    // Report success.
    return response + L"  </CdrGetUsrResp>\n";
}
