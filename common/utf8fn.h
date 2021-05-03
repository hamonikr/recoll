#ifndef _UTF8FN_H_
#define _UTF8FN_H_

#include <string>

class RclConfig;

// Translate file name/path to utf8 for indexing.
// 
// @param simple If true we extract and process only the simple file name
// (ignore the path)
std::string compute_utf8fn(const RclConfig *config, const std::string& ifn,
                           bool simple);

#endif // _UTF8FN_H_

