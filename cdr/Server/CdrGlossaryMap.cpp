/*
 * $Id: CdrGlossaryMap.cpp,v 1.1 2004-07-08 00:32:38 bkline Exp $
 *
 * Returns a document identifying which glossary terms should be used
 * for marking up phrases found in a CDR document.
 *
 * $Log: not supported by cvs2svn $
 */

#include <set>
#include <map>
#include <vector>
#include <sstream>
#include <wchar.h>

#include "CdrCommand.h"
#include "CdrDbPreparedStatement.h"
#include "CdrDbResultSet.h"


static void mapPreferredTerms(cdr::StringSet& phrases, 
                              std::map<int, cdr::StringList>& mappings,
                              cdr::db::Connection& conn);
static void mapExternalPhrases(cdr::StringSet& phrases, 
                              std::map<int, cdr::StringList>& mappings,
                              cdr::db::Connection& conn);
static void addPhrase(cdr::StringSet& phrases,
                      std::map<int, cdr::StringList>& mappings,
                      int id,
                      const cdr::String& phrase);
static cdr::String normalizePhrase(const cdr::String& phrase);

/**
 * Returns a document identifying which glossary terms should be used
 * for marking up phrases found in a CDR document.
 *
 *  @param      session     contains information about the current user.
 *  @param      node        contains the XML for the command.
 *  @param      conn        reference to the connection object for the
 *                          CDR database.
 *  @return                 String object containing the XML for the
 *                          command response.
 *  @exception  cdr::Exception if a database or processing error occurs.
 */
cdr::String cdr::getGlossaryMap(cdr::Session&,
                                const cdr::dom::Node&,
                                cdr::db::Connection& connection)
{
    // Start the response document.
    std::wostringstream response;
    response << L"<CdrGetGlossaryMapResp>";

    // Remember phrases we've already taken care of.
    StringSet phrases;

    // Collect the phrases which belong to each term.
    std::map<int, StringList> mappings;

    // Find the term's preferred terms first (these take precedence).
    mapPreferredTerms(phrases, mappings, connection);

    // Collect all the other phrases we can map.
    mapExternalPhrases(phrases, mappings, connection);

    // Fill out the response document.
    std::map<int, StringList>::const_iterator term = mappings.begin();
    while (term != mappings.end()) {
        response << L"<Term id='" << term->first << "'>";
        StringList::const_iterator phrase = term->second.begin();
        while (phrase != term->second.end()) {
            response << L"<Phrase>" << cdr::entConvert(*phrase)
                     << L"</Phrase>";
            ++phrase;
        }
        response << L"</Term>";
        ++term;
    }

    // Wrap it up and ship it out.
    response << L"</CdrGetGlossaryMapResp>";
    return response.str();
}

/*
 * Get the preferred names for the glossary terms.
 */
void mapPreferredTerms(cdr::StringSet& phrases, 
                       std::map<int, cdr::StringList>& mappings,
                       cdr::db::Connection& conn)
{
    cdr::db::Statement stmt = conn.createStatement();
    cdr::db::ResultSet rs = stmt.executeQuery(
        "SELECT doc_id, value                  "
        "  FROM query_term                     "
        " WHERE path = '/GlossaryTerm/TermName'");
    while (rs.next()) {
        int         id   = rs.getInt(1);
        cdr::String name = rs.getString(2);
        addPhrase(phrases, mappings, id, name);
    }
    stmt.close();
}

/*
 * Get the alternate phrases from the external_map table.
 */
void mapExternalPhrases(cdr::StringSet& phrases, 
                        std::map<int, cdr::StringList>& mappings,
                        cdr::db::Connection& conn)
{
    cdr::db::Statement stmt = conn.createStatement();
    cdr::db::ResultSet rs = stmt.executeQuery(
        "SELECT m.doc_id, m.value              "
        "  FROM external_map m                 "
        "  JOIN external_map_usage u           "
        "    ON u.id   = m.usage               "
        " WHERE u.name = 'GlossaryTerm Phrases'");
    while (rs.next()) {
        int         id   = rs.getInt(1);
        cdr::String name = rs.getString(2);
        addPhrase(phrases, mappings, id, name);
    }
    stmt.close();
}

/*
 * Add a phrase to our mapping object if we haven't already handled the
 * phrase.
 */
void addPhrase(cdr::StringSet& phrases,
               std::map<int, cdr::StringList>& mappings,
               int id,
               const cdr::String& phrase)
{
    // Check to see if we have already seen this phrase.
    cdr::String normalizedPhrase = normalizePhrase(phrase);
    if (phrases.find(normalizedPhrase) != phrases.end()) 
        return;

    // Plug the phrase into the mapping table.
    if (mappings.find(id) == mappings.end())
        mappings[id] = cdr::StringList(); //stringList;
    mappings[id].push_back(normalizedPhrase);

    // Remember that we already have this phrase.
    phrases.insert(normalizedPhrase);
}

cdr::String normalizePhrase(const cdr::String& phrase)
{
    cdr::String p = phrase;
    size_t i = 0;
    bool justSawSpace = false;
    while (i < p.size()) {
        if (wcschr(L"'\".,?!:;()[]{}<>\x201C\x201D", p[i]))
            p.erase(i, 1);
        else if (wcschr(L"\n\r\t -", p[i])) {
            if (justSawSpace || i == 0 || i == p.size() - 1)
                p.erase(i, 1);
            else {
                p[i++] = ' ';
                justSawSpace = true;
            }
        }
        else {
            p[i] = towupper(p[i]);
            ++i;
            justSawSpace = false;
        }
    }
    return p;
}