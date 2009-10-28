/*
 * $Id: CdrString.h,v 1.30 2006-05-25 23:02:00 ameyer Exp $
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.29  2005/03/04 02:58:18  ameyer
 * Changed namespace names for new Xerces DOM parser.
 *
 * Revision 1.28  2004/11/05 05:57:36  ameyer
 * Added hashBytes().  Not currently used, but there it is.
 *
 * Revision 1.27  2004/05/12 02:45:50  ameyer
 * Added trimWhiteSpace().
 *
 * Revision 1.26  2004/03/23 16:26:48  bkline
 * Upgraded to version 5.4.0 of xml4c (but still using deprecated APIs).
 *
 * Revision 1.25  2003/08/04 17:03:26  bkline
 * Fixed breakage caused by upgrade to latest version of Microsoft's
 * C++ compiler.
 *
 * Revision 1.24  2002/11/25 21:15:48  bkline
 * Added optional doQuotes boolean argument to entConvert() function.
 *
 * Revision 1.23  2002/08/21 04:16:43  ameyer
 * Added normalizeWhiteSpace().
 *
 * Revision 1.22  2002/04/04 01:07:25  bkline
 * Added typedef for NamedValues.
 *
 * Revision 1.21  2001/04/05 22:34:21  ameyer
 * Added ynCheck() prototype.
 *
 * Revision 1.20  2001/02/28 02:37:16  bkline
 * Added explicit assignment operators.
 *
 * Revision 1.19  2000/12/26 23:22:59  ameyer
 * Fixed bad html in comment string.
 *
 * Revision 1.18  2000/10/25 19:07:08  mruben
 * added functions to put dates in correct format
 *
 * Revision 1.17  2000/10/18 03:20:00  ameyer
 * Added optional parameter to tagWrap allowing attributes to be included.
 *
 * Revision 1.16  2000/10/05 17:14:44  ameyer
 * Declaration of entConvert()
 *
 * Revision 1.15  2000/07/21 21:08:08  ameyer
 * Added tagWrap().
 *
 * Revision 1.14  2000/07/11 22:42:03  ameyer
 * Added stringDocId() helper function.
 *
 * Revision 1.13  2000/06/09 04:01:22  ameyer
 * Added toString template function.
 *
 * Revision 1.12  2000/05/16 21:19:26  bkline
 * Added packErrors() function.
 *
 * Revision 1.11  2000/05/09 19:21:59  mruben
 * added operator+= and operator+
 *
 * Revision 1.10  2000/05/04 12:39:43  bkline
 * More ccdoc comments.
 *
 * Revision 1.9  2000/05/03 15:42:31  bkline
 * Added greater-than and less-than comparison operators.  Added getFloat()
 * method.
 *
 * Revision 1.8  2000/04/26 01:40:12  bkline
 * Added copy constructor, != operator, and extractDocId() method.
 *
 * Revision 1.7  2000/04/22 18:57:38  bkline
 * Added ccdoc comment markers for namespaces and @pkg directives.
 *
 * Revision 1.6  2000/04/22 18:01:26  bkline
 * Fleshed out documentation comments.
 *
 * Revision 1.5  2000/04/21 14:01:17  bkline
 * Added String(size_t, wchar_t) constructor.
 *
 * Revision 1.4  2000/04/19 18:29:52  bkline
 * Added StringList typedef.
 *
 * Revision 1.3  2000/04/17 21:28:01  bkline
 * Made cdr::String nullable.
 *
 * Revision 1.2  2000/04/14 16:02:00  bkline
 * Added support for converting UTF-8 to UTF-16.
 *
 * Revision 1.1  2000/04/11 14:18:24  bkline
 * Initial revision
 */

#ifndef CDR_STRING_
#define CDR_STRING_

// System headers.
#include <string>
#include <sstream>
#include <set>
#include <vector>
#include <list>
#include <map>

// Apache Xerces definition of text
#include <xercesc/dom/DOMtext.hpp>

/**@#-*/

namespace cdr {

/**@#+*/

    /** @pkg cdr */

    /**
     * This typedef is required to work around MSVC++ bugs in the handling of
     * parameterized types.
     */
    typedef std::wstring StdWstring;

    /**
     * Wrapper class for wide-character string.  Used to store Unicode
     * as UTF-16.
     */
    class String : public StdWstring {
    public:

        /**
         * Creates an empty string.  The object represents a <code>NULL</code>
         * string if the optional argument is set to <code>true</code>.
         *
         *  @param  n           <code>true</code> if the object represents
         *                      a NULL value; defaults to <code>false</code>.
         */
        String(bool n = false) : null(n) {}

        /**
         * Creates a non-NULL <code>String</code> object from a
         * null-terminated buffer of wide characters.
         *
         *  @param  s           address of null-terminated array of characters
         *                      representing a string encoded as UTF-16.
         */
        String(const wchar_t* s) : StdWstring(s), null(false) {}

        /**
         * Creates a non-NULL <code>String</code> object from at most the
         * first <code>n</code> wide characters in buffer <code>s</code>.
         *
         *  @param  s           address of array of characters representing
         *                      a string encoded as UTF-16.
         *  @param  n           limit to the number of characters in the
         *                      array to be used in constructing the new
         *                      object.
         */
        String(const wchar_t* s, size_t n) : StdWstring(s, n), null(false) {}

        /**
         * Creates a non-NULL <code>String</code> object from a standard
         * <code>wstring</code>.
         *
         *  @param  s           reference to standard string object to be
         *                      copied.
         */
        String(const StdWstring& s) : StdWstring(s), null(false) {}

        /**
         * Creates a non-NULL <code>String</code> object from a
         * null-terminated buffer of characters encoded using
         * <code>UTF-8</code>.
         *
         *  @param  s           address of null-terminated array of characters
         *                      representing a string encoded as UTF-8.
         */
        String(const char* s) : null(false) { utf8ToUtf16(s); }

        /**
         * Creates a non-NULL <code>String</code> object from a standard
         * <code>string</code> encoded using <code>UTF-8</code>.
         *
         *  @param  s           reference to standard <code>string</code>
         *                      object containing UTF-8 encoded value for
         *                      object.
         */
        String(const std::string& s) : null(false) { utf8ToUtf16(s.c_str()); }

        /**
         * Initializes a new non-NULL <code>String</code> object consisting of
         * <code>n</code> repetitions of wide character <code>c</code>.
         * Useful for creating a <code>String</code> which is known in advance
         * to require at least <code>n</code> characters, but whose contents
         * must be determined after creation of the object.
         *
         *  @param  n           size of new object (in characters).
         *  @param  c           character to be used to initialize the object.
         */
        String(size_t n, wchar_t c) : StdWstring(n, c), null(false) {}

        /**
         * Creates a non-NULL <code>String</code> object from a string used by
         * the DOM interface.
         *
         *  @param  s           reference to DOM string to be copied.
         */
        String(const XERCES_CPP_NAMESPACE::DOMText& s)
            : StdWstring(s.getData(), s.getLength()), null(false) {}

        /**
         * Copy constructor.
         *
         *  @param  s           reference to object to be copied.
         */
        String(const String& s) : StdWstring(s), null(s.null) {}

        /**
         * Assignment operators.  Need to be explicit so that null state is
         * handled properly.
         *
         *  @param  w           reference to object to be copied.
         *  @return             reference to modified String object.
         */
        String& operator=(const String& s)
            { assign(s); null = s.null; return *this; }
        String& operator=(const StdWstring& s)
            { assign(s); null = false; return *this; }
        String& operator=(const wchar_t* s)
            { assign(s); null = false; return *this; }
        String& operator=(const XERCES_CPP_NAMESPACE::DOMText& s)
            { assign(s.getData(), s.getLength()); null = false; return *this; }
        String& operator=(const std::string& s)
            { utf8ToUtf16(s.c_str()); null = false; return *this; }
        String& operator=(const char* s)
            { utf8ToUtf16(s); null = false; return *this; }

        /**
         * Concatentate something else to this cdr::String
         *
         *  @param  s           const reference to obejct to be concatentated
         *
         *  @return             reference to this cdr::String
         */
        String& operator+=(const String& s)
            { *this = *this + s; return *this; }
        String& operator+=(const wchar_t* s)
            { *this = *this + s; return *this; }
        String& operator+=(wchar_t s)
            { *this = *this + s; return *this; }

        /*
         * These are needed by older versions of Microsoft's compiler,
         * but they don't work with a standards-compliant compiler.
         */
#if defined(_MSC_VER) && _MSC_VER < 1310

        /**
         * Compares another cdr::String to this one.  Case is significant for
         * the purposes of this comparison.
         *
         *  @param  s           reference to object to be compared with this
         *                      object.
         *  @return             <code>true</code> iff the contents of this
         *                      object differ from those of <code>s</code>.
         */
        bool operator!=(const String& s) const
            { return *this != static_cast<const StdWstring&>(s); }

        /**
         * Compares another cdr::String to this one.  Case is significant for
         * the purposes of this comparison.
         *
         *  @param  s           reference to object to be compared with this
         *                      object.
         *  @return             <code>true</code> iff the contents of this
         *                      object sort lexically before those of
         *                      <code>s</code>.
         */
        bool operator<(const String& s) const
            { return *this < static_cast<const StdWstring&>(s); }

        /**
         * Compares another cdr::String to this one.  Case is significant for
         * the purposes of this comparison.
         *
         *  @param  s           reference to object to be compared with this
         *                      object.
         *  @return             <code>true</code> iff the contents of this
         *                      object sort lexically after those of
         *                      <code>s</code>.
         */
        bool operator>(const String& s) const
            { return *this > static_cast<const StdWstring&>(s); }
#endif

        /**
         * Accessor method for determining whether this object is NULL.
         *
         *  @return             <code>true</code> iff the object represents
         *                      a <code>NULL</code> string.
         */
        bool isNull() const { return null; }

        /**
         * Parses the integer value represented by the string value of the
         * object.
         *
         *  @return             integer value represented by the string
         *                      value of the object; <code>0</code> if
         *                      the value of the object does not represent
         *                      a number.
         */
        int getInt() const;

        /**
         * Parses the floating-point value represented by the string value of
         * the object.
         *
         *  @return             floating-point value represented by the string
         *                      value of the object; <code>0.0</code> if
         *                      the value of the object does not represent
         *                      a number.
         */
        double getFloat() const;

        /**
         * Template function to convert any object for which a stringstream
         * conversion exists to a String.
         */
        template <class T> String static toString (const T& in)
        {
            std::wostringstream os;
            os << in;
            return os.str();
        }

        /**
         * Creates a standard <code>string</code> copy of the value of the
         * object, encoded using <code>UTF-8</code>.
         *
         *  @return             new <code>string</code> object containing
         *                      UTF-8 encoding of the object's value.
         */
        std::string toUtf8() const;

        /**
         * Return a new string as an upper cased version of this string.
         *
         *  @return             Upper cased version.
         */
        cdr::String toUpperCase() const;

        /**
         * Return a new string as a lower cased version of this string.
         *
         *  @return             Lowercased version.
         */
        String toLowerCase() const;

        /**
         * Skips past the 'CDR' prefix and extracts the integer id for the
         * document.
         *
         *  @return             integer for the document's primary key.
         *  @exception          <code>cdr::Exception</code> if the first
         *                      characters of the string are not "CDR".
         */
        int extractDocId() const;

    private:
        void utf8ToUtf16(const char*);
        bool null;
    };

#if defined(_MSC_VER) && _MSC_VER < 1310
    /**
     * Concatenate two Strings.  Needed by older versions of MS compiler.
     * Not sure what Mike was doing here anyway (notice that the first
     * string is being modified).
     */
    inline String operator+(String s, const String& t)
    {
      return s += t;
    }
#endif

    /**
     * Containers of <code>String</code> objects.  Typedefs given here for
     * convenience, and as workarounds for MSVC++ template bugs.
     */
    typedef std::set<String>             StringSet;
    typedef std::vector<String>          StringVector;
    typedef std::list<String>            StringList;
    typedef std::map<String, String>     NamedValues;

    /**
     * Convert an integer to a CDR doc id, with 'CDR' prefix and
     * zero padding.
     *
     *  @param  id          integer form of a document id.
     *  @return             string form of the doc id.
     */
    extern String stringDocId (const int);

    /**
     * Convert reserved XML characters to character entities in a string
     *
     *  @param  inStr       reference to string to be converted.
     *  @param  doQuotes    flag indicating " and ' need conversion also.
     *  @return             string with &...; in place of reserved chars
     */
    extern String entConvert (const String&, bool = false);

    /**
     * Normalize all whitespace characters in a string to single space.
     * Uses iswspace to define what is whitespace.
     *
     *  @param inStr        reference to string to normalize.
     *  @return             copy of string, with space normalized.
     */
    extern String normalizeWhiteSpace (const String&);

    /**
     * Trim all whitespace characters from the beginning and/or end
     * of a string.
     * Uses iswspace to define what is whitespace.
     *
     *  @param inStr        reference to string to normalize.
     *  @param leading      True=trim leading whitespace, default.
     *  @param trailing     True=trim trailing whitespace, default.
     *  @return             copy of string, with space normalized.
     */
    extern String trimWhiteSpace (const String&, bool=true, bool=true);

    /**
     * Wrap an xml tag around a string, returning the wrapped string.
     *
     *  @param  data        reference to string to be wrapped as element data.
     *  @param  tag         reference to tag to wrap around data.
     *  @param  attrs       optional attribute info as a single string.
     *  @return             string with &lt;tag&gt;data&lt;/tag&gt;
     */
    extern String tagWrap (const String& data, const String& tag,
                           const String& = L"");

    /**
     * Packs the error messages contained in the caller's list into a
     * single string suitable for embedding within the command response.
     *
     *  @param  errors      list of strings containing error information
     *                      gathered by the command processor.
     *  @return             marked-up &lt;errors&gt; element containing
     *                      one &lt;err&gt; element for each member of
     *                      the <code>errors</code> list.
     */
    extern String packErrors(const StringList& errors);

    /**
     * Puts a date in format suitable for use in database
     *
     *  @param  date        string date in format suitable for either database
     *                      or XML
     *  @return             string date in format suitable for database
     */
     extern String toDbDate(String date);

    /**
     * Puts a date in format suitable for use in XML
     *
     *  @param  date        string date in format suitable for either database
     *                      or XML
     *  @return             string date in format suitable for XML
     */
     extern String toXmlDate(String date);

    /**
     * Determine if a string contains "Y" or "N" or "y" or "n".
     *
     *  @param  ynString    String to check.
     *  @param  defaultVal  Value to return if neither Y nor N found.
     *  @param  forceMsg    If non-empty, throw exception if Y or N not found
     *                      and use this string to tell a user what data object
     *                      did not contain Y or N but should have.
     *  @return             True or false.
     *  @throws cdr::Exception if neither Y nor N found and caller specified
     *                      force.
     */

    bool ynCheck (cdr::String ynString, bool defaultVal,
                  cdr::String forceMsg = L"");

    /**
     * Compute a hash value for a byte stream.
     *
     *  @param  bytes       Pointer to 8 bit bytes.
     *  @param  len         Length in bytes (not UTF-16 chars!)
     *  @return             32 bit hash value
     *                      0 if zero length byte stream.
     */
    unsigned long hashBytes(const unsigned char *bytes, size_t len);
}

#endif