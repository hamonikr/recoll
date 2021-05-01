/* Copyright (C) 2004 J.F.Dockes
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
#ifndef _ECRONTAB_H_INCLUDED_
#define _ECRONTAB_H_INCLUDED_

/** Utility function to manage lines inside a user crontab 
 * 
 * Lines managed by this routine are marked with a hopefuly unique marker
 * and discriminated by a selector, both environment variable settings. 
 * Example:
 *  30 8 * * * RCLCRONTAB_RCLINDEX= RECOLL_CONFDIR=/path/to/dir recollindex ...
 * RCLCRONTAB_RCLINDEX is the line marker, and the RECOLL_CONFDIR value
 * allows selecting the affected line. 
 *
 * This approach allows leaving alone lines which do have a
 * RECOLL_CONFDIR value but not managed by us. The marker and selector
 * values are chosen by the caller, which should apply some thought to
 * chosing sane values.
 */

#include <string>
#include <vector>
using std::string;
using std::vector;

/** Add, replace or delete a command inside a crontab file
 *
 * @param marker selects lines managed by this module and should take the form
 *  of a (possibly empty) environment variable assignement.
 * @param id selects the appropriate line to affect and will usually be an 
 *   actual variable assignment (see above)
 * @param sched is a standard cron schedule spec (ie: 30 8 * * *)
 * @param cmd is the command to execute (the last part of the line). 
 *    Set it to an empty string to delete the line from the crontab
 * @param reason error message
 *
 * "marker" and "id" should look like reasonable env variable assignements. 
 * Only ascii capital letters, numbers and _ before the '='
 */
bool editCrontab(const string& marker, const string& id, 
		 const string& sched, const string& cmd,
		 string& reason
    );

/**
 * check crontab for unmanaged lines
 * @param marker same as above, typically RCLCRONTAB_RCLINDEX=
 * @param data string to look for on lines NOT marked, typically "recollindex"
 * @return true if unmanaged lines exist, false else.
 */
bool checkCrontabUnmanaged(const string& marker, const string& data);

/** Retrieve the scheduling for a crontab entry */
bool getCrontabSched(const string& marker, const string& id, 
		     vector<string>& sched);

#endif /* _ECRONTAB_H_INCLUDED_ */
