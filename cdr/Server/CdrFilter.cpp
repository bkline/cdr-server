/*
 * $Id: CdrFilter.cpp,v 1.8 2001-05-04 17:00:49 mruben Exp $
 *
 * Applies XSLT scripts to a document
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.7  2001/04/05 23:10:02  mruben
 * added specifying filter by name
 *
 * Revision 1.6  2001/03/19 17:17:43  mruben
 * added support for xsl:message
 *
 * Revision 1.5  2001/03/13 22:15:09  mruben
 * added ability to use CdrDoc element for filter
 *
 * Revision 1.4  2001/02/26 16:09:27  mruben
 * fixed error in exception safety
 *
 * Revision 1.3  2000/09/25 14:00:14  mruben
 * added cdrx protocol for XSLT URIs
 *
 * Revision 1.2  2000/08/24 13:43:11  mruben
 * added support for cdr: URIs
 *
 * Revision 1.1  2000/08/23 14:19:03  mruben
 * Initial revision
 *
 */

#if defined _MSC_VER
#pragma warning(disable : 4786)
#endif

#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "sablot.h"

#include "CdrCommand.h"
#include "CdrDbPreparedStatement.h"
#include "CdrDbResultSet.h"
#include "CdrException.h"
#include "CdrGetDoc.h"
#include "CdrString.h"

using std::pair;
using std::string;
using std::vector;
using std::wostringstream;

namespace
{
  // unique type used to get CdrDocCtl
  const wchar_t CdrDocType[] = L"";
  string filter_messages;
  
  /***************************************************************************/
  /* get document from the repository.  Error if type not NULL and document  */
  /* does not have this type                                                 */
  /***************************************************************************/
  cdr::String getDocument(cdr::String ui, 
                          cdr::db::Connection& connection,
                          const wchar_t* type = NULL)
  {
    if (type == CdrDocType)
      return cdr::getDocString(ui, connection, false);
    
    cdr::String docstring(cdr::getDocString(ui, connection));
    
    cdr::dom::Parser parser;
    parser.parse(docstring);
    cdr::dom::Document doc = parser.getDocument();
    for (cdr::dom::Node node = doc.getFirstChild().getFirstChild();
         node != NULL;
         node = node.getNextSibling())
    {
      cdr::String name = node.getNodeName();
      if (type != NULL && name == L"CdrDocCtl")
      {
        for (cdr::dom::Node child = node.getFirstChild();
             child != NULL;
             child = child.getNextSibling())
          if (child.getNodeName() == L"DocType"
                  && child.getNodeValue() != type)
            throw cdr::Exception(cdr::String(
                          L"Invalid DocType in filter. Type is ")
                             + child.getNodeValue()
                             + L", expected " + type);
      }
      else
      if (name == L"CdrDocXml"
              && node.getNodeType() == cdr::dom::Node::ELEMENT_NODE)
      {
        cdr::dom::Node xml = node.getFirstChild();
        if (xml.getNodeType() != cdr::dom::Node::CDATA_SECTION_NODE)
          break;
        
        return xml.getNodeValue();
      }
    }
    throw cdr::Exception(L"dryrot: invalid return from getDocString\n");
  }

  /***************************************************************************/
  /* get filter using title.  Replace @@DOCTYPE@@ with document type of      */
  /* document to which it will be applied.  Error if more than one document  */
  /* with requested title                                                    */
  /***************************************************************************/
  cdr::String getFilterByName(cdr::String name,
                              cdr::db::Connection& connection,
                              cdr::String* docui)
  {
    cdr::String::size_type idx = name.find(L"@@DOCTYPE@@");
    if (idx != cdr::String::npos)
    {
      cdr::String doctype;

      if (docui != NULL)
      {
        string tquery = "SELECT name "
                        "FROM doc_type dt "
                        "JOIN document d ON dt.id = d.doc_type "
                        "WHERE d.id=?";
        cdr::db::PreparedStatement tselect
              = connection.prepareStatement(tquery);
        tselect.setInt(1, docui->extractDocId());
        cdr::db::ResultSet trs = tselect.executeQuery();
        if (trs.next())
          doctype = trs.getString(1);
      }
      name.replace(idx, 11, doctype);
    }
    
    string query = "SELECT id "
                   "FROM document "
                   "WHERE title=? "
                   "GROUP BY id";
    
    cdr::db::PreparedStatement select = connection.prepareStatement(query);
    select.setString(1, name);
    cdr::db::ResultSet rs = select.executeQuery();
    if (!rs.next())
      throw cdr::Exception(L"No document with title: " + name);

    int id = rs.getInt(1);

    if (rs.next())
      throw cdr::Exception(L"Multiple documents with title: " + name);
    
    return getDocument(cdr::stringDocId(id), connection);
  }

  
  /***************************************************************************/
  /* get document from document spec (XML)                                   */
  /* If type is not null and document is retrieved from CDR, its DocType     */
  /* must match type.  If ui is not null, the ui will be returned in it.     */
  /* docui is used when getting a filter -- it contains the ui of the        */
  /* document to which the filter will be applied                            */
  /***************************************************************************/
  cdr::String getDocument(cdr::dom::Node docspec,
                          cdr::db::Connection& connection,
                          const wchar_t* type = NULL,
                          cdr::String* ui = NULL, cdr::String* docui = NULL)
  {
    cdr::dom::NamedNodeMap attributes = docspec.getAttributes();
    
    cdr::dom::Node name = attributes.getNamedItem("Name");
    if (name != NULL)
      return getFilterByName(name.getNodeValue(), connection, docui);

    cdr::dom::Node href = attributes.getNamedItem("href");
    if (href != NULL)
    {
      cdr::String u = href.getNodeValue();
      if (ui != NULL)
        *ui = u;
      
      return getDocument(u, connection, type);
    }

    cdr::dom::Node f = docspec.getFirstChild();
    if (f.getNodeType() != cdr::dom::Node::CDATA_SECTION_NODE)
      throw cdr::Exception(L"Invalid filter specification");

    return f.getNodeValue();
  }

  /***************************************************************************/
  /* Sablotron error/message handling callbacks                              */
  /***************************************************************************/
  MH_ERROR makeCode(void*, SablotHandle, int,
                    unsigned short, unsigned short code)
  {
    return code;
  }

  MH_ERROR messageHandler(void*, SablotHandle, MH_ERROR code, MH_LEVEL level,
                          char** fields)
  {
    switch (level)
    {
      case MH_LEVEL_CRITICAL:
      case MH_LEVEL_ERROR:
      {
        wostringstream msg;
        msg << "XSLT error: code: " << code;
        if (fields != NULL)
          while (*fields != NULL)
            msg << cdr::String(*(fields++)) << L"\n";

        throw cdr::Exception(msg.str());
      }

      case MH_LEVEL_WARN:
        if (fields != NULL)
        {
          char** p;
          string msg;
          for (p = fields; p != NULL && strncmp(*p, "msg:", 4) != 0; ++p)
            ;
          if (p != NULL)
          {
            static const char xsl_msgheader[] = "xsl:message (";
            char* q = *p + 4;
            if (strncmp(q, xsl_msgheader, sizeof xsl_msgheader - 1) == 0)
            {
              msg = string(q + sizeof xsl_msgheader - 1);
              if (msg.length() != 0)
                msg = msg.substr(0, msg.length() - 1);
                
              filter_messages += "<message>"
                               + msg
                               + "</message>\n";
            }
          }

        }
        break;
        
      case MH_LEVEL_INFO:
        break;
    }
    
    return 0;
  }
  
  /***************************************************************************/
  /* Sablotron URI handling callbacks                                        */
  /***************************************************************************/
  class UriInfo
  {
    public:
      UriInfo() : inuse(false), pos(0) {}
      
      bool inuse;
      string doc;
      int pos;
  };

  vector<UriInfo> uri_list;

  int uri_open(void* data, SablotHandle, const char* scheme, const char* rest,
               int* handle)
  {
    if (scheme != NULL && scheme[0] != '\0' && strcmp(scheme, "cdr") != 0
            && strcmp(scheme, "cdrx") != 0)
      return SH_ERR_UNSUPPORTED_SCHEME;

    bool conditional = (strcmp(scheme, "cdrx") == 0);

    UriInfo u;
    cdr::db::Connection& connection = *static_cast<cdr::db::Connection*>(data);

    while (*rest == '/')
      ++rest;
    if (*rest == '\0')
      return 1;

    // we don't let an exception go back through Sablotron since we're not
    // sure if it's exception safe.  Instead we just return error if we can't
    // get the document
    try
    {
      // ***TODO***
      // for now we only implement simple UI to get the document XML and
      // ui/CdrCtl to get the entire doc control string
      cdr::String uid(rest);
      cdr::String type;
      cdr::String::size_type sep = uid.find(L'/');
      if (sep != cdr::String::npos)
      {
        type = uid.substr(sep + 1);
        uid = uid.substr(0, sep);
        sep = type.find(L'/');
        if (sep != cdr::String::npos)
          type = type.substr(0, sep);
      }

      if (type == L"CdrCtl")
        u.doc = cdr::getDocCtlString(uid, connection,
                                     cdr::DocCtlComponents::DocTitle).toUtf8();
      else
        u.doc = getDocument(uid, connection).toUtf8();
    }
    catch (...)
    {
      if (!conditional)
        return 1;

      u.doc = "<CdrDocCtl><NotFound>Y</NotFound></CdrDocCtl>";
    }

    u.inuse = true;
    int i;
    int n = uri_list.size();
    for (i = 0; i < n; ++i)
      if (!uri_list[i].inuse)
      {
        uri_list[i] = u;
        *handle = i;
        return 0;
      }

    *handle = uri_list.size();
    uri_list.push_back(u);
    return 0;
  }

  int uri_close(void*, SablotHandle, int handle)
  {
    uri_list[handle].inuse = false;
    return 0;
  }

  int uri_getall(void* data, SablotHandle processor, const char* scheme,
                 const char* rest, char** buffer, int* count)
  {
    int handle;
    
    *buffer = NULL;

    try
    {
      int rc = uri_open(data, processor, scheme, rest, &handle);
      if (rc)
        return 1;

      *count = uri_list[handle].doc.size();
      *buffer = new char[*count + 1];
      strcpy(*buffer, uri_list[handle].doc.c_str());
    }
    catch (...)
    {
      delete[] *buffer;
      throw;
    }
      
    return 0;
  }

  int uri_free(void*, SablotHandle, char* buffer)
  {
    delete[] buffer;
    return 0;
  }

  int uri_get(void*, SablotHandle, int handle, char* buffer, int* count)
  {
    int n = uri_list[handle].doc.size() - uri_list[handle].pos;
    if (*count > n)
      *count = n;

    if (*count < 0)
      *count = 0;

    if (*count > 0)
      memcpy(buffer, uri_list[handle].doc.data() + uri_list[handle].pos,
             *count);

    uri_list[handle].pos += *count;

    return 0;
  }

  int uri_put(void*, SablotHandle, int, const char*, int*)
  {
    return 1;  // unsupported
  }
  
  /***************************************************************************/
  /* Process filter on document                                              */
  /***************************************************************************/
  int processStrings(string script, string document,
                     string& result, cdr::db::Connection& connection)
  {
    char* s = NULL;
    char* d = NULL;
    char* r = NULL;

    try
    {
      // We have to go through some contortions here to avoid sending a const
      // char* to sablotron.  This probably is not necessary, but the function
      // is not declared as taking a const char* so it seems safer.
      
      s = new char[script.length() + 1];
      strcpy(s, script.c_str());
      d = new char[document.length() + 1];
      strcpy(d, document.c_str());
      
      char *arguments[] = { "/_stylesheet", s,
                            "/_xmlinput", d,
                            "/_output", NULL,
                            NULL
                          };
      void* proc;
      int rc;
      try
      {
        if ((rc = SablotCreateProcessor(&proc)))
          throw cdr::Exception(L"Cannot create XSLT processor");

        MessageHandler mh = { makeCode, messageHandler, messageHandler };
        if (SablotRegHandler(proc, HLR_MESSAGE, &mh, NULL))
          throw cdr::Exception(L"cannot register Sablotron message handler");

        SchemeHandler sh = { uri_getall, uri_free, uri_open,
                             uri_get, uri_put,  uri_close
        };
        if (SablotRegHandler(proc, HLR_SCHEME, &sh, &connection))
          throw cdr::Exception(L"cannot register Sablotron scheme handler");
      
        if ((rc = SablotRunProcessor(proc, "arg:/_stylesheet",
                                     "arg:/_xmlinput", "arg:/_output",
                                     NULL, arguments)))
          throw cdr::Exception(L"XSLT error");

        if ((rc = SablotGetResultArg(proc, "arg:/_output", &r)))
          throw cdr::Exception(L"Cannot get XSLT result");
      }
      catch (...)
      {
        SablotDestroyProcessor(proc);
        throw;
      }

      if ((rc = SablotDestroyProcessor(proc)))
        throw cdr::Exception(L"Cannot destroy XSLT processor");

      result = r;
      delete[] s;
      delete[] d;
      SablotFree(r);
      return rc;
    }
    catch (...)
    {
      delete[] s;
      delete[] d;
      SablotFree(r);
      throw;
    }
  }
}

cdr::String cdr::filter(cdr::Session& session, 
                        const cdr::dom::Node& commandNode,
                        cdr::db::Connection& connection)
{
  vector<cdr::String> filters;
  cdr::String document;
  cdr::String ui;

  // we need to serialize.  Save current autocommit state so if the
  // caller has already done so, we don't mess him up
  bool autocommitted = connection.getAutoCommit();
  connection.setAutoCommit(false);

  // check if we're to do output
  bool output = true;
  cdr::dom::NamedNodeMap cmdattr = commandNode.getAttributes();
  cdr::dom::Node attr = cmdattr.getNamedItem("Output");

  cdr::String foo;
  
  if (attr != NULL && cdr::String(attr.getNodeValue()) == L"N")
    output = false;

  // we need to get the document first so we'll have it's type if needed
  // to determine the filter name
  
  // extract document from the command
  {
    for (cdr::dom::Node child = commandNode.getFirstChild();
         child != NULL;
         child = child.getNextSibling())
    {
      if (child.getNodeType() == cdr::dom::Node::ELEMENT_NODE)
      {
        cdr::String name = child.getNodeName();
        if (name == L"Document")
        {
          const wchar_t* type = NULL;
          cdr::dom::NamedNodeMap attributes = child.getAttributes();
          cdr::dom::Node ctl = attributes.getNamedItem("ctl");
          if (ctl != NULL)
          {
            cdr::String val = ctl.getNodeValue();
            if (val == L"y")
              type = CdrDocType;
          }

          document = getDocument(child, connection, type, &ui);

          break; // only one document allowed
        }
      }
    }
  }
  
  // extract filters from the command
  {
    for (cdr::dom::Node child = commandNode.getFirstChild();
         child != NULL;
         child = child.getNextSibling())
    {
      if (child.getNodeType() == cdr::dom::Node::ELEMENT_NODE)
      {
        cdr::String name = child.getNodeName();
        if (name == L"Filter")
          filters.push_back(getDocument(child, connection, NULL, NULL, &ui));
      }
    }
  }

  string doc(document.toUtf8());
  string result(doc);
  for (std::vector<cdr::String>::iterator i = filters.begin();
       i != filters.end();
       ++i)
  {
    filter_messages = "";
    if (processStrings(i->toUtf8(), doc, result, connection))
      throw cdr::Exception(L"error in XSLT processing");

    doc = result;
  }

  // we're done with the database, so we can let others at it
  connection.setAutoCommit(autocommitted);

  result = output ? "<Document><![CDATA[" + result + "]]></Document>\n" : "";
  if (filter_messages.length() != 0)
    result += "<Messages>" + filter_messages + "</Messages>\n";
  
  return L"<CdrFilterResp>"
       + result
       + "</CdrFilterResp>\n";
}
