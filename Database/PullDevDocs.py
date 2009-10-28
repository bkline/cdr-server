#----------------------------------------------------------------------
#
# $Id: PullDevDocs.py,v 1.5 2007-11-14 01:00:09 venglisc Exp $
#
# Pulls control document which need to be preserved from the
# development server in preparation for refreshing the CDR
# database from the production server.  Used in conjunction
# with PushDevDocs.py.
#
# $Log: not supported by cvs2svn $
# Revision 1.4  2007/11/06 19:19:17  venglisc
# Modified program to extract any documents of new document types in addition
# to the already saved new and updated filter, css, etc. documents. (Bug 3418)
#
# Revision 1.3  2007/07/24 20:48:22  venglisc
# Modified to preserve new document types created on the development server
# only. (Bug 3418)
#
# Revision 1.2  2003/06/10 17:30:02  bkline
# Added code to preserve filter set information on the development server.
#
# Revision 1.1  2002/09/07 14:12:49  bkline
# Tools for preserving development versions of control docs.
#
#----------------------------------------------------------------------
import cdr, cdrdb, os, re, sys, time

#----------------------------------------------------------------------
# Capture processing information.  Optional argument indicates whether
# information should be displayed as well (default is no).
#----------------------------------------------------------------------
def log(msg, show = 0):
    logFile.write("%s\n" % msg)
    if show:
        sys.stderr.write("%s\n" % msg)

#----------------------------------------------------------------------
# Object representing a CDR control document.
#----------------------------------------------------------------------
class Doc:
    def __init__(self, id, title, data):
        self.id     = id
        self.title  = title
        self.data   = data

#----------------------------------------------------------------------
# Load a set of CDR control documents of a particular document type
# from one of the servers (development or production).  Returns a
# dictionary for the set, indexed by the uppercase versions of the
# document titles.
#----------------------------------------------------------------------
def loadDocs(cursor, server, docType):
    docs = {}
    if docType == 'css':
        cursor.execute("""\
            SELECT d.id, d.title, b.data
              FROM document d
              JOIN doc_type t
                ON t.id = d.doc_type
              JOIN doc_blob b
                on b.id = d.id
             WHERE t.name = ?""", docType)
    else:
        cursor.execute("""\
            SELECT d.id, d.title, d.xml
              FROM document d
              JOIN doc_type t
                ON t.id = d.doc_type
             WHERE t.name = ?""", docType)
    for row in cursor.fetchall():
        id    = row[0]
        title = row[1]
        data  = row[2]
        key   = title.upper().encode('utf-8')
        if docs.has_key(key):
            log("duplicate found for %s doc '%s' on %s: %d and %d" %
                    (docType, title, server, docs[key].id, id), 1)
            sys.exit(1)
        else:
            try:
                if docType == 'css':
                    docs[key] = Doc(id, title, 
                                str(data).replace('\r', '').strip())
                else:
                    docs[key] = Doc(id, title, 
                                data.replace('\r', '').strip().encode('utf-8'))
            except:
                print 'Error processing CDR-ID: %d' % id
                raise
    return docs

#----------------------------------------------------------------------
# Create a new output directory based on the current date/time.  This
# avoids the risk of having a job clobber the results of an earlier run.
#----------------------------------------------------------------------
def makeOutputDir():
    dir = time.strftime('DevDocs-%Y%m%d%H%M%S')
    os.mkdir(dir)
    os.mkdir(dir + '/RepDocs')
    os.mkdir(dir + '/AddDocs')
    return dir

#----------------------------------------------------------------------
# Encode data delimiters.
#----------------------------------------------------------------------
def fixUp(s):
    if not s: return ""
    return s.replace("\t", "@@TAB@@").replace("\n", "@@NL@@").replace("\r", "")

#----------------------------------------------------------------------
# Initialize parameters for the job.
#----------------------------------------------------------------------
docTypes  = ('Filter', 'css', 'PublishingSystem', 'Schema')
if len(sys.argv) != 3:
    sys.stderr.write('usage: PullDevDocs prod-machine dev-machine\n')
    sys.stderr.write(' e.g.: PullDevDocs BACH MAHLER\n')
    sys.exit(1)
outputDir = makeOutputDir()
logFile   = open('%s/PullDevDocs.log' % outputDir, 'w')
prdServer = sys.argv[1]
devServer = sys.argv[2]
prdConn   = cdrdb.connect(user = 'CdrGuest', dataSource = prdServer)
devConn   = cdrdb.connect(user = 'CdrGuest', dataSource = devServer)
prdCursor = prdConn.cursor()
devCursor = devConn.cursor()
pattern   = re.compile(r"""\s+Id\s*=\s*['"]CDR\d+['"]""")

# ---------------------------------------------------------------------
# Check if any new docTypes exist on the development server that need
# to be preserved as well.
# We select the ID of the docType last created on the production server
# and preserve all docTypes on MAHLER that have an ID greater then 
# this lastDocTypeId.
# ---------------------------------------------------------------------
prdCursor.execute("""\
    SELECT max(id)
      FROM doc_type
     WHERE active = 'Y'""")
lastDocTypeId = prdCursor.fetchone()

devCursor.execute("""\
    select id, name
      FROM doc_type
     WHERE id > ?""", lastDocTypeId)

rows = devCursor.fetchall()

for id, newDocType in rows:
    docTypes += str(newDocType),
    log('Preserving new docType: %s' % newDocType)

# ---------------------------------------------------------------------
# Preserve the information from the doc_type table for the new
# doc_types
# Note:  I probably should have used the cdr.getDoctype() module to 
#        extract this information but I learned after the fact of its
#        existense.  That's left for the next version. VE.
# ---------------------------------------------------------------------
log('Preserving doc_type information from %s' % devServer)
devCursor.execute("""\
    SELECT dt.id, dt.name, dt.format, dt.created, dt.versioning, 
           dt.xml_schema, dt.schema_date, dt.title_filter, 
           dt.active, dt.comment, d.title
      FROM doc_type dt
      JOIN document d
        ON d.id = dt.title_filter
     WHERE dt.id > ?""", lastDocTypeId)

newDocTypes = open('%s/NewDocTypes.tab' % outputDir, 'w')
for row in devCursor.fetchall():
    comment = fixUp(row[9])
    newDocTypes.write("%d\t%s\t%d\t%s\t%s\t%d\t%s\t%d\t%s\t%s\t%s\t\n" % (
                       row[0],
                       row[1], row[2], row[3], row[4], row[5] or "",
                       row[6], row[7] or "", row[8], comment, row[10] or ""))
newDocTypes.close()

#----------------------------------------------------------------------
# Preserve the filter sets from the development server.
#----------------------------------------------------------------------
log('Preserving filter sets from %s' % devServer)

devCursor.execute("""\
    SELECT id, name, description, notes
      FROM filter_set""")
filterSets = open('%s/FilterSets.tab' % outputDir, 'w')
for row in devCursor.fetchall():
    name = fixUp(row[1])
    desc = fixUp(row[2])
    note = fixUp(row[3])
    filterSets.write("%d\t%s\t%s\t%s\n" % (row[0], name, desc, note))
filterSets.close()

devCursor.execute("""\
         SELECT m.filter_set, m.position, d.title, m.subset
           FROM filter_set_member m
LEFT OUTER JOIN document d
             ON d.id = m.filter""")
filterSetMembers = open('%s/FilterSetMembers.tab' % outputDir, 'w')
for row in devCursor.fetchall():
    filterSetMembers.write("%d\t%d\t%s\t%s\n" % (row[0], row[1],
                                                 fixUp(row[2]),
                                                 row[3] and str(row[3]) or ""))
filterSetMembers.close()

      
#----------------------------------------------------------------------
# Walk through each of the control document types.  For each type,
# gather all documents of that type from the production and the
# development servers.  For each document which does not yet exist
# on the production machine, drop a copy of the document in a
# subdirectory for new documents to be added (after stripping out
# the document ID left over from the production machine; the added
# document will get a new ID).  For each document which exists on
# both servers but with different data, add a copy of the document
# in a subdirectory for documents to be replaced (after ensuring
# that the document ID is the one for the production server, not the
# development server).
#----------------------------------------------------------------------
log('Comparing docs between %s and %s' % (prdServer, devServer), 1)
for docType in docTypes:
    print 'Processing %s ...' % docType
    prdDocs = loadDocs(prdCursor, prdServer, docType)
    devDocs = loadDocs(devCursor, devServer, docType)
    for key in devDocs.keys():
        devId = devDocs[key].id
        if prdDocs.has_key(key):

            # The document exists on both servers.  Are they identical?
            prdId = prdDocs[key].id
            if devDocs[key].data == prdDocs[key].data:
                log("%s doc %d on %s matches %d on %s" % (docType,
                            prdId, prdServer, devId, devServer))
            else:

                # The documents don't match.  Prepare replacement.
                log("%s doc %d on %s differs from %d on %s" % (docType,
                            prdId, prdServer, devId, devServer))
                resp = cdr.getDoc('guest', 'CDR%010d' % devId, host = devServer)
                if resp.find('<Err') != -1:
                    log('Failure retrieving CDR%010d from %s: %s' % (
                                devId, devServer, resp.strip()), 1)
                    sys.exit(1)
                else:
                    idAttr = " Id='CDR%010d'" % prdId
                    doc = pattern.sub(idAttr, resp.replace('\r', ''), 1)
                    name = "%s/RepDocs/%d.xml" % (outputDir, prdId)
                    open(name, "wb").write(doc)
                
        else:

            # The document hasn't yet been added to the production server.
            log("%s doc %d on %s not found on %s" % (docType,
                        devId, devServer, prdServer))
            resp = cdr.getDoc('guest', 'CDR%010d' % devId, host = devServer)
            if resp.find('<Err') != -1:
                log('Failure retrieving CDR%010d from %s: %s' % (
                            devId, devServer, resp), 1)
                sys.exit(1)
            else:
                doc = pattern.sub('', resp.replace('\r', ''), 1)
                name = "%s/AddDocs/%d.xml" % (outputDir, devId)
                open(name, "wb").write(doc)

    # Log documents found on the production but not the development server.
    for key in prdDocs.keys():
        prdId = prdDocs[key].id
        if not devDocs.has_key(key):
            log("%s doc %d on %s not found on %s" % (docType,
                        prdId, prdServer, devServer))
    print 'Processing %s Done' % docType