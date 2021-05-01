#ifndef _DATERANGE_H_INCLUDED_
#define _DATERANGE_H_INCLUDED_

#include <xapian.h>

namespace Rcl {
extern Xapian::Query date_range_filter(int y1, int m1, int d1, 
				       int y2, int m2, int d2);
}
#endif /* _DATERANGE_H_INCLUDED_ */
