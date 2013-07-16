/*
 * $Id$
 *
 * CDR wrapper for regular expression processing.
 */

#ifndef CDR_REGEX_
#define CDR_REGEX_

#ifndef UNICODE
#define UNICODE
#define CDR_UNICODE_DEFINED_
#endif

// After consulting with John Maddock and the Boost project team, it was
// decided that this warning (that a derived class (boost::bad_pattern)
// is exported by the regex++ DLL when its base class (std::runtime_error)
// is not exported).  One of the reasons this is harmless is that all
// of the methods in std::runtime_error are defined inline.
#pragma warning(disable : 4275)

#include <regex>
#include "CdrString.h"

/**@#-*/

namespace cdr {

/**@#+*/

    /** @pkg cdr */

    /**
     * Wrapper class for Microsoft's regular expression package (in case we
     * need to replace that package at some future time without modifying code
     * throughout the rest of the system).  First version exposes only the
     * functionality needed by the Document Validation module.  Additional
     * features will be added as needed.  In particular, this first version
     * does not yet support retrieval of the matched substrings.
     */
    class RegEx : private std::wregex {

    public:

        /**
         * Default constructor with optional flag for turning off recognition
         * of differences in case.  Use one of the <code>set()</code> methods
         * to plug in a regular expression to be used for matching.
         *
         *  @param  ignoreCase      <code>true</code> if comparisons should 
         *                          disregard differences in case for letters; 
         *                          default is <code>false</code>.
         */
        RegEx(bool ignoreCase = false) : caseFlag(ignoreCase) {}

        /**
         * Creates a new regular expression object based on a null-terminated
         * wide-character string.
         *
         *  @param  pattern         address of string containing regular
         *                          expression, encoded as UTF-16.
         *  @param  ignoreCase      <code>true</code> if comparisons should 
         *                          disregard differences in case for letters; 
         *                          default is <code>false</code>.
         */
        RegEx(const wchar_t* pattern, bool ignoreCase = false);

        /**
         * Creates a new regular expression object based on a null-terminated
         * wide-character string.
         *
         *  @param  pattern         reference to string object containing 
         *                          regular expression, encoded as UTF-16.
         *  @param  ignoreCase      <code>true</code> if comparisons should 
         *                          disregard differences in case for letters; 
         *                          default is <code>false</code>.
         */
        RegEx(const String& pattern, bool ignoreCase = false);

        /**
         * Creates a new regular expression object based on a null-terminated
         * narrow-character string.  String is expected to be encoded as
         * UTF-8, and is converted internally to UTF-16 encoding by the
         * constructor.
         *
         *  @param  pattern         address of string containing regular
         *                          expression, encoded as UTF-8.
         *  @param  ignoreCase      <code>true</code> if comparisons should 
         *                          disregard differences in case for letters; 
         *                          default is <code>false</code>.
         */
        RegEx(const char* pattern, bool ignoreCase = false);

        /**
         * Creates a new regular expression object based on a standard
         * narrow-character string object.  String is expected to be encoded
         * as UTF-8, and is converted internally to UTF-16 encoding.
         *
         *  @param  pattern         reference to standard <code>string</code> 
         *                          object containing regular expression
         *                          encoded as UTF-8.
         *  @param  ignoreCase      <code>true</code> if comparisons should 
         *                          disregard differences in case for letters; 
         *                          default is <code>false</code>.
         */
        RegEx(const std::string& pattern, bool ignoreCase = false);

        /**
         * Reports whether the specified string matches the object's regular
         * expression pattern.
         *
         *  @param  s               address of null-terminated string
         *                          containing string to be tested, encoded as
         *                          UTF-16.
         *  @return                 <code>true</code> if <code>s</code>
         *                          matches the object's regular expression
         *                          pattern; otherwise <code>false</code>.
         */
        bool    match(const wchar_t* s);

        /**
         * Reports whether the specified string matches the object's regular
         * expression pattern.
         *
         *  @param  s               reference to <code>String</code> object
         *                          containing string to be tested, encoded as
         *                          UTF-16.
         *  @return                 <code>true</code> if <code>s</code>
         *                          matches the object's regular expression
         *                          pattern; otherwise <code>false</code>.
         */
        bool    match(const String& s);

        /**
         * Reports whether the specified string matches the object's regular
         * expression pattern.
         *
         *  @param  s               address of null-terminated string
         *                          containing string to be tested, encoded as
         *                          UTF-8.
         *  @return                 <code>true</code> if <code>s</code>
         *                          matches the object's regular expression
         *                          pattern; otherwise <code>false</code>.
         */
        bool    match(const char* s);

        /**
         * Reports whether the specified string matches the object's regular
         * expression pattern.
         *
         *  @param  s               reference to <code>string</code> object
         *                          containing string to be tested, encoded as
         *                          UTF-8.
         *  @return                 <code>true</code> if <code>s</code>
         *                          matches the object's regular expression
         *                          pattern; otherwise <code>false</code>.
         */
        bool    match(const std::string& s);

        /**
         * Sets the object's regular expression pattern to a new string.
         *
         *  @param  pattern         address of null-terminated string
         *                          containing new regular expression pattern
         *                          for the object, encoded as UTF-16.
         */
        void    set(const wchar_t* pattern);

        /**
         * Sets the object's regular expression pattern to a new string.
         *
         *  @param  pattern         reference to <code>String</code> object
         *                          containing new regular expression pattern
         *                          for the object, encoded as UTF-16.
         */
        void    set(const String& pattern);

        /**
         * Sets the object's regular expression pattern to a new string.
         *
         *  @param  pattern         address of null-terminated string
         *                          containing new regular expression pattern
         *                          for the object, encoded as UTF-8.
         */
        void    set(const char* pattern);

        /**
         * Sets the object's regular expression pattern to a new string.
         *
         *  @param  pattern         reference to <code>string</code> object
         *                          containing new regular expression pattern
         *                          for the object, encoded as UTF-8.
         */
        void    set(const std::string& pattern);

    private:

        /**
         * Flag remembering whether case should be disregarded for
         * pattern matching.
         */
        bool    caseFlag;

        /**
         * Unimplemented (blocked) copy constructor.
         *
         *  @param  re              reference to object to be copied.
         */
        RegEx(const RegEx&);

        /**
         * Unimplemented (blocked) assignment operator.
         *
         *  @param  re              reference to object to be copied.
         *  @return                 reference to modified object.
         */
        RegEx operator=(const RegEx&);
    };

}

#ifdef CDR_UNICODE_DEFINED_
#undef UNICODE
#undef CDR_UNICODE_DEFINED_
#endif

#endif
