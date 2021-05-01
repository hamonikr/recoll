/* Copyright (C) 2005 J.F.Dockes
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
#ifndef _CANCELCHECK_H_INCLUDED_
#define _CANCELCHECK_H_INCLUDED_


/**
 * Common cancel checking mechanism
 *
 * The CancelCheck class is used as a singleton objet (private constructor).
 * The single instance can be accessed as CancelCheck::instance.
 * It is used as follows, in an asynchronous program where there is an
 *  interactive (or otherwise controlling) task and a long-working one:
 *  - The control task calls setCancel(), usually as a result of user 
 *    interaction, if the worker takes too long.
 *  - The worker task calls checkCancel() at regular intervals, possibly as
 *    a side-effect of some other progress-reporting call. If cancellation has
 *    been requested, this will raise an exception, to be catched and processed
 *    wherever the worker was invoked.
 * The worker side must be exception-clean, but this otherwise avoids
 * having to set-up code to handle a special cancellation error along
 * the whole worker call stack.
 */
class CancelExcept {};

class CancelCheck {
 public:
    static CancelCheck& instance();
    void setCancel(bool on = true) {
	cancelRequested = on;
    }
    void checkCancel() {
	if (cancelRequested) {
	    throw CancelExcept();
	}
    }
    bool cancelState() {return cancelRequested;}
 private:
    bool cancelRequested;

    CancelCheck() : cancelRequested(false) {}
    CancelCheck& operator=(CancelCheck&);
    CancelCheck(const CancelCheck&);
};

#endif /* _CANCELCHECK_H_INCLUDED_ */
