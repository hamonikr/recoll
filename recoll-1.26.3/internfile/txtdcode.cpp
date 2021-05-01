/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "autoconfig.h"

#include <sstream>

#include "cstr.h"
#include "transcode.h"
#include "mimehandler.h"
#include "log.h"
#include "smallut.h"
#include "listmem.h"

using std::string;

// Called after decoding from utf-8 failed. Handle the common case
// where this is a good old 8bit-encoded text document left-over when
// the locale was switched to utf-8. We try to guess a charset
// according to the locale language and use it. This is a very rough
// heuristic, but may be better than discarding the data. 
// If we still get a significant number of decode errors, the doc is
// quite probably binary, so just fail.
// Note that we could very well get a wrong transcoding (e.g. between
// iso-8859 variations), there is no way to detect it.
static bool alternate_decode(const string& in, string& out, string& ocs)
{
    int ecnt;
    if (samecharset(ocs, cstr_utf8)) {
        string lang = localelang();
        string code = langtocode(lang);
        LOGDEB("RecollFilter::txtdcode: trying alternate decode from " <<
               code << "\n");
        bool ret = transcode(in, out, code, cstr_utf8, &ecnt);
        if (ecnt > 5)
            ret = false;
        if (ret) {
            ocs = code;
        }
        return ret;
    } else {
        // Give a try to utf-8 anyway, as this is self-detecting. This
        // handles UTF-8 docs in a non-utf-8 environment. Note that
        // this will almost never be called, as most encodings are
        // unable to detect errors so that the first try at
        // transcoding will have succeeded and alternate_decode() will
        // not be called at all.
        // 
        // To avoid this, we would have to attempt an utf-8 decode
        // first, but this is a costly proposition as we don't know
        // how much data to test, so need to test all (the beginning
        // of the text could be ascii even if there are 8-bit chars
        // later).
        bool ret = transcode(in, out, cstr_utf8, cstr_utf8, &ecnt);
        return ecnt > 5 ? false : ret;
    }
}

static string bomtocode(const string& itext)
{
#if 0
    std::ostringstream strm;
    listmem(strm, itext.c_str(), MIN(itext.size(), 8));
    LOGDEB("txtdcode:bomtocode: input " << strm.str() << "\n");
#endif

    const unsigned char *utxt = (const unsigned char *)itext.c_str();
    if (itext.size() >= 3 && utxt[0] == 0xEF && utxt[1] == 0xBB &&
        utxt[2] == 0xBF) {
        LOGDEB("txtdcode:bomtocode: UTF-8\n");
        return "UTF-8";
    } else if (itext.size() >= 2 && utxt[0] == 0xFE && utxt[1] == 0xFF) {
        return "UTF-16BE";
    } else if (itext.size() >= 2 && utxt[0] == 0xFF && utxt[1] == 0xFE) {
        return "UTF-16LE";
    } else if (itext.size() >= 4 && utxt[0] == 0 && utxt[1] == 0 &&
               utxt[2] == 0xFE && utxt[3] == 0xFF) {
        return "UTF-32BE";
    } else if (itext.size() >= 4 && utxt[3] == 0 && utxt[2] == 0 &&
               utxt[1] == 0xFE && utxt[0] == 0xFF) {
        return "UTF-32LE";
    } else {
        return string();
    }
}

bool RecollFilter::txtdcode(const string& who)
{
    if (m_metaData[cstr_dj_keymt].compare(cstr_textplain)) {
	LOGERR(who << "::txtdcode: called on non txt/plain: " <<
               m_metaData[cstr_dj_keymt] << "\n");
	return false;
    }

    string& ocs = m_metaData[cstr_dj_keyorigcharset];
    string& itext = m_metaData[cstr_dj_keycontent];
    LOGDEB(who << "::txtdcode: "  << itext.size() << " bytes from ["  <<
           ocs << "] to UTF-8\n");
    int ecnt;
    string otext;

    string bomfromcode = bomtocode(itext);
    if (!bomfromcode.empty()) {
        LOGDEB(who << "::txtdcode: " << " input charset changed from " <<
               ocs << " to " << bomfromcode << " from BOM detection\n");
        ocs = bomfromcode;
    }
    
    bool ret = transcode(itext, otext, ocs, cstr_utf8, &ecnt);
    if (!ret || ecnt > int(itext.size() / 100)) {
	LOGERR(who << "::txtdcode: transcode " << itext.size() <<
               " bytes to UTF-8 failed for input charset [" << ocs <<
               "] ret " << ret << " ecnt "  << ecnt << "\n");

        ret = alternate_decode(itext, otext, ocs);

	if (!ret) {
	    LOGDEB("txtdcode: failed. Doc is not text?\n" );
	    itext.erase();
	    return false;
	}
    }

    itext.swap(otext);
    m_metaData[cstr_dj_keycharset] = cstr_utf8;
    return true;
}
