/*
 * $Id: xCdrCommand.cpp,v 1.18 2001-02-26 16:09:53 mruben Exp $
 *
 * Lookup facility for CDR commands.  Also contains stubs right now.
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.17  2001/01/17 21:49:18  bkline
 * Replaced CdrGetSchema with CdrAddDocType, CdrModDocType, CdrDelDocType,
 * and CdrGetDocType.
 *
 * Revision 1.16  2000/12/28 13:29:26  bkline
 * Added entry for CdrGetSchema command.
 *
 * Revision 1.15  2000/10/27 02:31:55  ameyer
 * ifdef'd out the delDoc stub.
 *
 * Revision 1.14  2000/10/24 13:13:16  mruben
 * added support for CdrReport
 *
 * Revision 1.13  2000/10/23 14:08:06  mruben
 * added version control commands
 *
 * Revision 1.12  2000/10/04 18:24:20  bkline
 * Added CdrSearchLinks and CdrListDocTypes to command map.
 *
 * Revision 1.11  2000/05/23 15:55:55  mruben
 * removed stub for CdrGetDoc
 *
 * Revision 1.10  2000/05/21 00:48:23  bkline
 * Added CdrShutdown command.
 *
 * Revision 1.9  2000/05/17 13:01:59  bkline
 * Added code to addDoc stub to verify that document is well-formed, including
 * that in the CDATA section.
 * Replaced addDoc and repDoc stubs with new code from Alan.
 *
 * Revision 1.8  2000/05/09 19:27:45  mruben
 * modified for CdrFilter
 *
 * Revision 1.7  2000/04/26 01:26:14  bkline
 * Replaced stub for cdr::validateDoc().
 *
 * Revision 1.6  2000/04/22 09:24:26  bkline
 * Added user and group commands.
 *
 * Revision 1.5  2000/04/21 13:57:11  bkline
 * Replaced stub for cdr::search().
 *
 * Revision 1.4  2000/04/17 21:25:28  bkline
 * Replaced stub for cdr::checkAuth() function.
 *
 * Revision 1.3  2000/04/16 21:42:25  bkline
 * Replaced stub for cdr::logoff().
 *
 * Revision 1.2  2000/04/15 14:08:44  bkline
 * Changed cdr::DbConnection* to cdr::db::Connection&.  Replaced stub
 * for cdr::logon with real implementation.
 *
 * Revision 1.1  2000/04/13 17:08:10  bkline
 * Initial revision
 */

#include <iostream>
#include "CdrCommand.h"

/**
 * Finds the function which implements the specified command.
 */
cdr::Command cdr::lookupCommand(const cdr::String& name)
{
    struct CommandMap {
        CommandMap(const cdr::String& n, cdr::Command c) : name(n), cmd(c) {}
        cdr::String name;
        cdr::Command cmd;
    };
    static CommandMap map[] = {
        CommandMap(L"CdrLogon",         cdr::logon),
        CommandMap(L"CdrLogoff",        cdr::logoff),
        CommandMap(L"CdrCheckAuth",     cdr::checkAuth),
        CommandMap(L"CdrAddGrp",        cdr::addGrp),
        CommandMap(L"CdrGetGrp",        cdr::getGrp),
        CommandMap(L"CdrModGrp",        cdr::modGrp),
        CommandMap(L"CdrDelGrp",        cdr::delGrp),
        CommandMap(L"CdrListGrps",      cdr::listGrps),
        CommandMap(L"CdrAddUsr",        cdr::addUsr),
        CommandMap(L"CdrGetUsr",        cdr::getUsr),
        CommandMap(L"CdrModUsr",        cdr::modUsr),
        CommandMap(L"CdrDelUsr",        cdr::delUsr),
        CommandMap(L"CdrListUsrs",      cdr::listUsrs),
        CommandMap(L"CdrAddDoc",        cdr::addDoc),
        CommandMap(L"CdrRepDoc",        cdr::repDoc),
        CommandMap(L"CdrDelDoc",        cdr::delDoc),
        CommandMap(L"CdrValidateDoc",   cdr::validateDoc),
        CommandMap(L"CdrSearch",        cdr::search),
        CommandMap(L"CdrSearchLinks",   cdr::searchLinks),
        CommandMap(L"CdrGetDoc",        cdr::getDoc),
        CommandMap(L"CdrCheckOut",      cdr::checkVerOut),
        CommandMap(L"CdrCheckIn",       cdr::checkVerIn),
        CommandMap(L"CdrCreateLabel",   cdr::createLabel),
        CommandMap(L"CdrDeleteLabel",   cdr::deleteLabel),
        CommandMap(L"CdrLabelDocument", cdr::labelDocument),
        CommandMap(L"CdrUnlabelDocument", cdr::unlabelDocument),
        CommandMap(L"CdrReport",        cdr::report),
        CommandMap(L"CdrFilter",        cdr::filter),
        CommandMap(L"CdrGetLinks",      cdr::getLinks),
        CommandMap(L"CdrListDocTypes",  cdr::listDocTypes),
        CommandMap(L"CdrAddDocType",    cdr::addDocType),
        CommandMap(L"CdrModDocType",    cdr::modDocType),
        CommandMap(L"CdrDelDocType",    cdr::delDocType),
        CommandMap(L"CdrGetDocType",    cdr::getDocType),
        CommandMap(L"CdrShutdown",      cdr::shutdown)
    };
    for (size_t i = 0; i < sizeof map / sizeof *map; ++i) {
        if (name == map[i].name)
            return map[i].cmd;
    }
    return 0;
}

/**
 * Temporary function to pump out a stub response element for a
 * CDR command which has not yet been implemented.
 */
static cdr::String stub(const cdr::String name)
{
    return L"  <" + name + L"Resp>\n   <Stub>Got your Cdr" + name +
           L" command ... can't process it yet.</Stub>\n  </"
           + name + L"Resp>\n";
}

// Stubs.  Replace these as they are implemented.
#if 0
cdr::String cdr::logon(cdr::Session& session,
                       const cdr::dom::Node& commandNode,
                       cdr::db::Connection& dbConnection) {
    return stub(L"Logon");
}

cdr::String cdr::logoff(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"Logoff");
}

cdr::String cdr::checkAuth(cdr::Session& session,
                           const cdr::dom::Node& commandNode,
                           cdr::db::Connection& dbConnection) {
    return stub(L"CheckAuth");
}

cdr::String cdr::addGrp(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"AddGrp");
}

cdr::String cdr::getGrp(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"GetGrp");
}

cdr::String cdr::modGrp(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"ModGrp");
}

cdr::String cdr::delGrp(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"DelGrp");
}

cdr::String cdr::listGrps(cdr::Session& session,
                          const cdr::dom::Node& commandNode,
                          cdr::db::Connection& dbConnection) {
    return stub(L"ListGrps");
}

cdr::String cdr::addUsr(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"AddUsr");
}

cdr::String cdr::getUsr(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"GetUsr");
}

cdr::String cdr::modUsr(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"ModUsr");
}

cdr::String cdr::delUsr(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"DelUsr");
}

cdr::String cdr::listUsrs(cdr::Session& session,
                          const cdr::dom::Node& commandNode,
                          cdr::db::Connection& dbConnection) {
    return stub(L"ListUsrs");
}

cdr::String cdr::addDoc(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    // Make sure the document is well-formed.
    cdr::dom::Node  child = commandNode.getFirstChild();
    while (child != 0) {
        if (child.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
            cdr::String name = child.getNodeName();
            if (name == L"CdrDoc") {
                child = child.getFirstChild();
                while (child != 0) {
                    if (child.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
                        cdr::String name = child.getNodeName();
                        if (name == L"CdrDocXml") {
                            child = child.getFirstChild();
                            while (child != 0) {
                                if (child.getNodeType() ==
                                        cdr::dom::Node::CDATA_SECTION_NODE) {

                                    cdr::dom::Parser parser;
                                    parser.parse(child.getNodeValue());
                                    cdr::dom::Document document =
                                        parser.getDocument();
                                    std::cerr << "CdrDocXml is well-formed\n";
                                    return stub(L"AddDoc");
                                }
                                child = child.getNextSibling();
                            }
                            break;
                        }
                    }
                    child = child.getNextSibling();
                }
                break;
            }
        }
        child = child.getNextSibling();
    }
    return stub(L"AddDoc");
}

cdr::String cdr::repDoc(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"RepDoc");
}
#endif

#if 0
cdr::String cdr::delDoc(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"DelDoc");
}
#endif

#if 0
cdr::String cdr::validateDoc(cdr::Session& session,
                             const cdr::dom::Node& commandNode,
                             cdr::db::Connection& dbConnection) {
    return stub(L"ValidateDoc");
}

cdr::String cdr::search(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"Search");
}

cdr::String cdr::getDoc(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"GetDoc");
}

#if 0
cdr::String cdr::checkVerOut(cdr::Session& session,
                          const cdr::dom::Node& commandNode,
                          cdr::db::Connection& dbConnection) {
    return stub(L"CheckOut");
}

cdr::String cdr::checkVerIn(cdr::Session& session,
                            const cdr::dom::Node& commandNode,
                            cdr::db::Connection& dbConnection) {
    return stub(L"CheckIn");
}
#endif

cdr::String cdr::report(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"Report");
}

cdr::String cdr::filter(cdr::Session& session,
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& dbConnection) {
    return stub(L"Filter");
}

cdr::String cdr::addDocType(cdr::Session& session,
                            const cdr::dom::Node& commandNode,
                            cdr::db::Connection& dbConnection) {
    return stub(L"Filter");
}

cdr::String cdr::modDocType(cdr::Session& session,
                            const cdr::dom::Node& commandNode,
                            cdr::db::Connection& dbConnection) {
    return stub(L"Filter");
}

cdr::String cdr::delDocType(cdr::Session& session,
                            const cdr::dom::Node& commandNode,
                            cdr::db::Connection& dbConnection) {
    return stub(L"Filter");
}

cdr::String cdr::getDocType(cdr::Session& session,
                            const cdr::dom::Node& commandNode,
                            cdr::db::Connection& dbConnection) {
    return stub(L"Filter");
}
#endif

cdr::String cdr::getLinks(cdr::Session& session,
                          const cdr::dom::Node& commandNode,
                          cdr::db::Connection& dbConnection) {
    return stub(L"GetLinks");
}
