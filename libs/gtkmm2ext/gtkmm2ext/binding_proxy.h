/*
    Copyright (C) 2006 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#ifndef __binding_proxy__
#define __binding_proxy__

#include <string>
#include <boost/shared_ptr.hpp>

#include "pbd/signals.h"

#include "gtkmm2ext/visibility.h"
#include "gtkmm2ext/popup.h"

namespace PBD {
	class Controllable;
}

class LIBGTKMM2EXT_API BindingProxy : public sigc::trackable
{
   public:
	BindingProxy (boost::shared_ptr<PBD::Controllable>);
	BindingProxy ();
	virtual ~BindingProxy();

	void set_bind_button_state (guint button, guint statemask);

	static bool is_bind_action (GdkEventButton *);
	bool button_press_handler (GdkEventButton *);

	boost::shared_ptr<PBD::Controllable> get_controllable() const { return controllable; }
 	void set_controllable (boost::shared_ptr<PBD::Controllable>);

  protected:
	Gtkmm2ext::PopUp*  prompter;
	boost::shared_ptr<PBD::Controllable> controllable;

	static guint bind_button;
	static guint bind_statemask;

	PBD::ScopedConnection learning_connection;
	void learning_finished ();
	bool prompter_hiding (GdkEventAny *);
};

#endif
