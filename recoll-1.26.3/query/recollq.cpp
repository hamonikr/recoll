/* Copyright (C) 2006 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
// Takes a query and run it, no gui, results to stdout

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include <iostream>
#include <list>
#include <string>

#include "rcldb.h"
#include "rclquery.h"
#include "rclconfig.h"
#include "pathut.h"
#include "rclinit.h"
#include "log.h"
#include "wasatorcl.h"
#include "internfile.h"
#include "wipedir.h"
#include "transcode.h"
#include "textsplit.h"
#include "smallut.h"
#include "chrono.h"
#include "base64.h"

using namespace std;

bool dump_contents(RclConfig *rclconfig, Rcl::Doc& idoc)
{
    FileInterner interner(idoc, rclconfig, FileInterner::FIF_forPreview);
    Rcl::Doc fdoc;
    string ipath = idoc.ipath;
    if (interner.internfile(fdoc, ipath)) {
	cout << fdoc.text << endl;
    } else {
	cout << "Cant turn to text:" << idoc.url << " | " << idoc.ipath << endl;
    }
    return true;
}

void output_fields(vector<string> fields, Rcl::Doc& doc,
		   Rcl::Query& query, Rcl::Db& rcldb, bool printnames)
{
    if (fields.empty()) {
        map<string,string>::const_iterator it;
        for (const auto& entry : doc.meta) {
            fields.push_back(entry.first);
        }
    }
    for (vector<string>::const_iterator it = fields.begin();
	 it != fields.end(); it++) {
	string out;
	if (!it->compare("abstract")) {
	    string abstract;
	    query.makeDocAbstract(doc, abstract);
	    base64_encode(abstract, out);
        } else if (!it->compare("xdocid")) {
            char cdocid[30];
            sprintf(cdocid, "%lu", (unsigned long)doc.xdocid);
            base64_encode(cdocid, out);
	} else {
	    base64_encode(doc.meta[*it], out);
	}
        // Before printnames existed, recollq printed a single blank for empty
        // fields. This is a problem when printing names and using strtok, but
        // have to keep the old behaviour when printnames is not set.
        if (!(out.empty() && printnames)) {
            if (printnames)
                cout << *it << " ";
            cout << out << " ";
        }
    }
    cout << endl;
}

static char *thisprog;
static char usage [] =
" -P: Show the date span for all the documents present in the index.\n"
" [-o|-a|-f] [-q] <query string>\n"
" Runs a recoll query and displays result lines. \n"
"  Default: will interpret the argument(s) as a xesam query string.\n"
"  Query elements: \n"
"   * Implicit AND, exclusion, field spec:  t1 -t2 title:t3\n"
"   * OR has priority: t1 OR t2 t3 OR t4 means (t1 OR t2) AND (t3 OR t4)\n"
"   * Phrase: \"t1 t2\" (needs additional quoting on cmd line)\n"
" -o Emulate the GUI simple search in ANY TERM mode.\n"
" -a Emulate the GUI simple search in ALL TERMS mode.\n"
" -f Emulate the GUI simple search in filename mode.\n"
" -q is just ignored (compatibility with the recoll GUI command line).\n"
"Common options:\n"
" -c <configdir> : specify config directory, overriding $RECOLL_CONFDIR.\n"
" -C : collapse duplicates\n"            
" -d also dump file contents.\n"
" -n [first-]<cnt> define the result slice. The default value for [first]\n"
"    is 0. Without the option, the default max count is 2000.\n"
"    Use n=0 for no limit.\n"
" -b : basic. Just output urls, no mime types or titles.\n"
" -Q : no result lines, just the processed query and result count.\n"
" -m : dump the whole document meta[] array for each result.\n"
" -A : output the document abstracts.\n"
" -S fld : sort by field <fld>.\n"
"   -D : sort descending.\n"
" -s stemlang : set stemming language to use (must exist in index...).\n"
"    Use -s \"\" to turn off stem expansion.\n"
" -T <synonyms file>: use the parameter (Thesaurus) for word expansion.\n"
" -i <dbdir> : additional index, several can be given.\n"
" -e use url encoding (%xx) for urls.\n"
" -E use exact result count instead of lower bound estimate"
" -F <field name list> : output exactly these fields for each result.\n"
"    The field values are encoded in base64, output in one line and \n"
"    separated by one space character. This is the recommended format \n"
"    for use by other programs. Use a normal query with option -m to \n"
"    see the field names. Use -F '' to output all fields, but you probably\n"
"    also want option -N in this case.\n"
"  -N : with -F, print the (plain text) field names before the field values.\n"
;
static void
Usage(void)
{
    cerr << thisprog <<  ": usage:" << endl << usage;
    exit(1);
}

// BEWARE COMPATIBILITY WITH recoll OPTIONS letters
static int     op_flags;

#define OPT_A     0x1
// GUI: -a same
#define OPT_a     0x2
#define OPT_b     0x4
#define OPT_C     0x8
// GUI: -c same
#define OPT_c     0x10 
#define OPT_D     0x20 
#define OPT_d     0x40 
#define OPT_e     0x80 
#define OPT_F     0x100
// GUI: -f same
#define OPT_f     0x200
// GUI uses -h for help. us: usage
#define OPT_i     0x400
// GUI uses -L to set language of messages
// GUI: -l same
#define OPT_l     0x800
#define OPT_m     0x1000
#define OPT_N     0x2000
#define OPT_n     0x4000
// GUI: -o same
#define OPT_o     0x8000
#define OPT_P     0x10000
#define OPT_Q     0x20000
// GUI: -q same
#define OPT_q     0x40000
#define OPT_S     0x80000
#define OPT_s     0x100000
#define OPT_T     0x200000
// GUI: -t use command line, us: ignored
#define OPT_t     0x400000
// GUI uses -v : show version. Us: usage
// GUI uses -w : open minimized
#define OPT_E     0x800000

int recollq(RclConfig **cfp, int argc, char **argv)
{
    string a_config;
    string sortfield;
    string stemlang("english");
    list<string> extra_dbs;
    string sf;
    vector<string> fields;
    string syngroupsfn;
    
    int firstres = 0;
    int maxcount = 2000;
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
	    case '-': 
		// -- : end of options
		if (*(*argv) != 0)
		    Usage();
		goto endopts;
            case 'A':   op_flags |= OPT_A; break;
            case 'a':   op_flags |= OPT_a; break;
            case 'b':   op_flags |= OPT_b; break;
            case 'C':   op_flags |= OPT_C; break;
	    case 'c':	op_flags |= OPT_c; if (argc < 2)  Usage();
		a_config = *(++argv);
		argc--; goto b1;
            case 'd':   op_flags |= OPT_d; break;
            case 'D':   op_flags |= OPT_D; break;
            case 'E':   op_flags |= OPT_E; break;
            case 'e':   op_flags |= OPT_e; break;
            case 'f':   op_flags |= OPT_f; break;
	    case 'F':	op_flags |= OPT_F; if (argc < 2)  Usage();
		sf = *(++argv);
		argc--; goto b1;
	    case 'i':	op_flags |= OPT_i; if (argc < 2)  Usage();
		extra_dbs.push_back(*(++argv));
		argc--; goto b1;
            case 'l':   op_flags |= OPT_l; break;
            case 'm':   op_flags |= OPT_m; break;
            case 'N':   op_flags |= OPT_N; break;
	    case 'n':	op_flags |= OPT_n; if (argc < 2)  Usage();
	    {
		string rescnt = *(++argv);
		string::size_type dash = rescnt.find("-");
		if (dash != string::npos) {
		    firstres = atoi(rescnt.substr(0, dash).c_str());
		    if (dash < rescnt.size()-1) {
			maxcount = atoi(rescnt.substr(dash+1).c_str());
		    }
		} else {
		    maxcount = atoi(rescnt.c_str());
		}
		if (maxcount <= 0) maxcount = INT_MAX;
	    }
	    argc--; goto b1;
            case 'o':   op_flags |= OPT_o; break;
            case 'P':   op_flags |= OPT_P; break;
            case 'q':   op_flags |= OPT_q; break;
            case 'Q':   op_flags |= OPT_Q; break;
	    case 'S':	op_flags |= OPT_S; if (argc < 2)  Usage();
		sortfield = *(++argv);
		argc--; goto b1;
	    case 's':	op_flags |= OPT_s; if (argc < 2)  Usage();
		stemlang = *(++argv);
		argc--; goto b1;
            case 't':   op_flags |= OPT_t; break;
	    case 'T':	op_flags |= OPT_T; if (argc < 2)  Usage();
		syngroupsfn = *(++argv);
		argc--; goto b1;
            default: Usage();   break;
            }
    b1: argc--; argv++;
    }
endopts:

    string reason;
    *cfp = recollinit(0, 0, 0, reason, &a_config);
    RclConfig *rclconfig = *cfp;
    if (!rclconfig || !rclconfig->ok()) {
	fprintf(stderr, "Recoll init failed: %s\n", reason.c_str());
	exit(1);
    }

    if (argc < 1 && !(op_flags & OPT_P)) {
	Usage();
    }
    if (op_flags & OPT_F) {
	if (op_flags & (OPT_b|OPT_d|OPT_b|OPT_Q|OPT_m|OPT_A))
	    Usage();
	stringToStrings(sf, fields);
    }
    Rcl::Db rcldb(rclconfig);
    if (!extra_dbs.empty()) {
        for (list<string>::iterator it = extra_dbs.begin();
             it != extra_dbs.end(); it++) {
            if (!rcldb.addQueryDb(*it)) {
                cerr << "Can't add index: " << *it << endl;
                exit(1);
            }
        }
    }
    if (!syngroupsfn.empty()) {
        if (!rcldb.setSynGroupsFile(syngroupsfn)) {
            cerr << "Can't use synonyms file: " << syngroupsfn << endl;
            exit(1);
        }
    }
    
    if (!rcldb.open(Rcl::Db::DbRO)) {
	cerr << "Cant open database in " << rclconfig->getDbDir() << 
	    " reason: " << rcldb.getReason() << endl;
	exit(1);
    }

    if (op_flags & OPT_P) {
        int minyear, maxyear;
        if (!rcldb.maxYearSpan(&minyear, &maxyear)) {
            cerr << "maxYearSpan failed: " << rcldb.getReason() << endl;
            exit(1);
        } else {
            cout << "Min year " << minyear << " Max year " << maxyear << endl;
            exit(0);
        }
    }

    if (argc < 1) {
	Usage();
    }
    string qs = *argv++;argc--;
    while (argc > 0) {
	qs += string(" ") + *argv++;argc--;
    }

    {
	string uq;
	string charset = rclconfig->getDefCharset(true);
	int ercnt;
	if (!transcode(qs, uq, charset, "UTF-8", &ercnt)) {
	    fprintf(stderr, "Can't convert command line args to utf-8\n");
	    exit(1);
	} else if (ercnt) {
	    fprintf(stderr, "%d errors while converting arguments from %s "
		    "to utf-8\n", ercnt, charset.c_str());
	}
	qs = uq;
    }

    Rcl::SearchData *sd = 0;

    if (op_flags & (OPT_a|OPT_o|OPT_f)) {
	sd = new Rcl::SearchData(Rcl::SCLT_OR, stemlang);
	Rcl::SearchDataClause *clp = 0;
	if (op_flags & OPT_f) {
	    clp = new Rcl::SearchDataClauseFilename(qs);
	} else {
	    clp = new Rcl::SearchDataClauseSimple((op_flags & OPT_o)?
                                                  Rcl::SCLT_OR : Rcl::SCLT_AND, 
                                                  qs);
	}
	if (sd)
	    sd->addClause(clp);
    } else {
	sd = wasaStringToRcl(rclconfig, stemlang, qs, reason);
    }

    if (!sd) {
	cerr << "Query string interpretation failed: " << reason << endl;
	return 1;
    }

    std::shared_ptr<Rcl::SearchData> rq(sd);
    Rcl::Query query(&rcldb);
    if (op_flags & OPT_C) {
        query.setCollapseDuplicates(true);
    }
    if (op_flags & OPT_S) {
	query.setSortBy(sortfield, (op_flags & OPT_D) ? false : true);
    }
    Chrono chron;
    if (!query.setQuery(rq)) {
	cerr << "Query setup failed: " << query.getReason() << endl;
	return(1);
    }
    int cnt;
    if (op_flags & OPT_E) {
        cnt = query.getResCnt(-1, true);
    } else {
        cnt = query.getResCnt();
    }
    if (!(op_flags & OPT_b)) {
	cout << "Recoll query: " << rq->getDescription() << endl;
	if (firstres == 0) {
	    if (cnt <= maxcount)
		cout << cnt << " results" << endl;
	    else
		cout << cnt << " results (printing  " << maxcount << " max):" 
		     << endl;
	} else {
	    cout << "Printing at most " << cnt - (firstres+maxcount) <<
		" results from first " << firstres << endl;
	}
    }
    if (op_flags & OPT_Q)
	cout << "Query setup took " << chron.millis() << " mS" << endl;

    if (op_flags & OPT_Q)
	return(0);

    for (int i = firstres; i < firstres + maxcount; i++) {
	Rcl::Doc doc;
	if (!query.getDoc(i, doc))
	    break;

	if (op_flags & OPT_F) {
	    output_fields(fields, doc, query, rcldb, op_flags & OPT_N);
	    continue;
	}

	if (op_flags & OPT_e) 
	    doc.url = url_encode(doc.url);

	if (op_flags & OPT_b) {
		cout << doc.url << endl;
	} else {
	    string titleorfn = doc.meta[Rcl::Doc::keytt];
	    if (titleorfn.empty())
		titleorfn = doc.meta[Rcl::Doc::keyfn];
	    if (titleorfn.empty()) {
                string url;
                printableUrl(rclconfig->getDefCharset(), doc.url, url);
                titleorfn = path_getsimple(url);
            }

	    char cpc[20];
	    sprintf(cpc, "%d", doc.pc);
	    cout 
		<< doc.mimetype << "\t"
		<< "[" << doc.url << "]" << "\t" 
		<< "[" << titleorfn << "]" << "\t"
		<< doc.fbytes << "\tbytes" << "\t"
		<<  endl;
	    if (op_flags & OPT_m) {
		for (const auto ent : doc.meta) {
		    cout << ent.first << " = " << ent.second << endl;
		}
	    }
            if (op_flags & OPT_A) {
                string abstract;
                if (query.makeDocAbstract(doc, abstract)) {
                    cout << "ABSTRACT" << endl;
                    cout << abstract << endl;
                    cout << "/ABSTRACT" << endl;
                }
            }
        }
        if (op_flags & OPT_d) {
            dump_contents(rclconfig, doc);
        }	
    }

    return 0;
}

