/*
 * $Id: CdrDbResultSet.h,v 1.6 2000-05-03 23:39:22 bkline Exp $
 *
 * Wrapper for ODBC result fetching.  Modeled after JDBC interface.
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.5  2000/05/03 15:39:34  bkline
 * Added copy constructor which bypasses copying of the column vector.
 * Blocked use of assignment operator.  Added getBytes() method.
 *
 * Revision 1.4  2000/04/22 18:57:38  bkline
 * Added ccdoc comment markers for namespaces and @pkg directives.
 *
 * Revision 1.3  2000/04/22 15:36:02  bkline
 * Filled out documentation comments.
 *
 * Revision 1.2  2000/04/17 21:27:40  bkline
 * Added nulability for ints and strings.
 *
 * Revision 1.1  2000/04/15 12:16:59  bkline
 * Initial revision
 */

#ifndef CDR_DB_RESULT_SET_
#define CDR_DB_RESULT_SET_

#include <vector>
#include <string>
#include "CdrDbStatement.h"
#include "CdrDbPreparedStatement.h"
#include "CdrBlob.h"
#include "CdrString.h"

/**@#-*/

namespace cdr {
    namespace db {

/**@#+*/

        /** @pkg cdr.db */

        /**
         * Implements JDBC-like interface for class of the same name for
         * extracting result rows from queries of the CDR database.
         */
        class ResultSet {
        public:

            /**
             * These classes are the factory classes for <code>ResultSet</code>
             * objects.  Most constructors are private.
             */
            friend Statement;
            friend PreparedStatement;

            /**
             * Cleans up resources allocated by the object, including the
             * vector of information about the result set columns.  Uses
             * reference counting to find out when the cleanup really needs to
             * happen.
             */
            ~ResultSet();

            /**
             * Copy constructor.  Uses reference counting so we can do a
             * shallow instead of a deep copy, which would involve an
             * expensive re-allocation of the column information.  Doesn't
             * seem to matter much, though, as the compiler seems to optimize
             * the calls away.  This is the only constructor which is public.
             *
             *  @param   rs         reference to result set to be copied.
             */
            ResultSet(const ResultSet& rs);

            /**
             * Moves the <code>ResultSet</code> to the next row.  The first 
             * call makes the first row the current row.  
             *
             *  @return             <code>true</code> as long as there is 
             *                      a next row to move to; if there are no
             *                      further rows to process,
             *                      <code>false</code>.
             */
            bool        next();

            /**
             * Retrieves a string value from the specified column of the
             * result set.
             *
             *  @param  pos         integer identifying the column of the
             *                      result set from which the value is to be
             *                      retrieved.
             *  @return             string containing requested value.
             */
            cdr::String getString(int pos);

            /**
             * Retrieves an integer value from the specified column of the
             * result set.
             *
             *  @param  pos         integer identifying the column of the
             *                      result set from which the value is to be
             *                      retrieved.
             *  @return             integer containing requested value.
             */
            cdr::Int    getInt(int pos);

            /**
             * Retrieves a binary value from the specified column of the
             * result set.
             *
             *  @param  pos         integer identifying the column of the
             *                      result set from which the value is to be
             *                      retrieved.
             *  @return             byte string containing requested value.
             */
            cdr::Blob   getBytes(int pos);

        private:

            /**
             * Unimplemented (blocked) default constructor.  Only way to
             * create a <code>ResultSet</code> object is by invoking the
             * appropriate factory methods of the <code>Statement</code> or
             * <code>PreparedStatement</code> classes.
             *
             *  @see    Statement
             *  @see    PreparedStatement
             */
            ResultSet();

            /**
             * Creates a <code>ResultSet</code> object for the specified
             * <code>Statement</code>.  Can only be invoked by one of the two
             * factory classes for result sets (<code>Statement</code> or
             * <code>PreparedStatement</code>).
             *
             *  @param  s               reference to statement object for
             *                          which the this object will retrieve
             *                          results.
             */
            ResultSet(Statement& s);

            /**
             * Reference to the statement for which this object will retrieve
             * results.
             */
            Statement&  st;
            
            /**
             * Number of copies of this objects in existence.  Used to make it
             * possible to perform only shallow copies of the object.  The
             * <code>refCount</code> member itself is not shared.  Instead the
             * original copy of the object shares a pointer to its counter.
             */
            int         refCount;

            /**
             * Pointer to the reference count shared by all copies of the
             * object.  When a new copy is made the count is incremented.
             * When a copy is destroyed the count is decremented.  When the
             * count drops to zero the resources for the object are released.
             */
            int*        pRefCount;

            /**
             * Information for one of the columns in a result set.
             */
            struct Column {

                /**
                 * Name of the column.  Extracted but not used for this
                 * version.
                 */
                std::string name;

                /**
                 * The type of the result value.
                 */
                SQLSMALLINT type;

                /**
                 * The maximum size of the result value.  Note that for TEXT
                 * columns this can be very large, and the actual size of the
                 * result must be determined when the value is retrieved.
                 */
                SQLUINTEGER size;

                /**
                 * Scale of the result.
                 */
                SQLSMALLINT digits;

                /**
                 * Flag indicating whether the column can hold NULL values.
                 */
                SQLSMALLINT nullable;
            };

            /**
             * For convenience and as a workaround for MSVC++ template bugs.
             */
            typedef std::vector<Column*> ColumnVector;;

            /**
             * Information collected from the <code>Statement</code> object
             * about the columns of the result set.
             */
            ColumnVector    columnVector;

            /**
             * Unimplemented (blocked) assignment operator.
             *
             *  @param  rs      reference to object to be copied.
             *  @return         reference to modified object.
             */
            ResultSet& operator=(const ResultSet& rs);
        };
    }
}

#endif
