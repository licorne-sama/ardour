/*
	Copyright (C) 2014 Waves Audio Ltd.

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

*/

#include "waves_ui.h"
#include "pbd/file_utils.h"
#include "pbd/failed_constructor.h"
#include "ardour/filesystem_paths.h"
#include "utils.h"

#include "pbd/convert.h"
#include "dbg_msg.h"

using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

std::map<std::string, const XMLTree*> WavesUI::__xml_tree_cache;

WavesUI::WavesUI (const std::string& layout_script_file, Gtk::Container& root)
	: _xml_tree (NULL)
	, _scrip_file_name (layout_script_file)
	, _root_container (root)
{
	// To avoid a need of reading the same file many times:
	std::map<std::string, const XMLTree*>::const_iterator it = __xml_tree_cache.find(layout_script_file);
	if (it != __xml_tree_cache.end()) {
		_xml_tree = (*it).second;
	} else {
		std::string layout_file; 
		Searchpath spath (ardour_data_search_path());
		spath.add_subdirectory_to_paths("ui");

		if (!find_file (spath, layout_script_file, layout_file)) {
			dbg_msg("File not found: " + layout_script_file);
			throw failed_constructor ();
		}

		_xml_tree = new XMLTree (layout_file, false);
		__xml_tree_cache[layout_script_file] = _xml_tree;
	}

	create_ui(_xml_tree, root);
}

WavesUI::~WavesUI ()
{
	for (std::list<Gtk::Object*>::iterator i = _orphan_objects.begin (); i != _orphan_objects.end(); ++i) {
		delete *i;
	}
}

Gtk::Widget*
WavesUI::create_widget (const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Object* child = NULL;
	std::string widget_type = definition.name();
	std::string widget_id = xml_property (definition, "id", styles, "");

	std::string text = xml_property (definition, "text", styles, "");
	boost::replace_all(text, "\\n", "\n");

	std::transform(widget_type.begin(), widget_type.end(), widget_type.begin(), ::toupper);

	if (widget_type == "BUTTON") {
		child = manage (new WavesButton(text));
	} else if (widget_type == "ICONBUTTON") {
		child = manage (new WavesIconButton);
	} else if (widget_type == "ICON") {
		std::string image_path;
		Searchpath spath(ARDOUR::ardour_data_search_path());

		spath.add_subdirectory_to_paths("icons");
    
		if (find_file (spath, 
									  xml_property (definition, "source", styles, ""),
									  image_path)) {
			Gtk::Image& icon = *manage (new Gtk::Image(image_path));
			child = &icon;
		} else {
			dbg_msg(xml_property (definition, "source", styles, "") + " NOT FOUND");
		}
	} else if (widget_type == "COMBOBOXTEXT") {
		child = manage (new Gtk::ComboBoxText);
	} else if (widget_type == "LABEL") {
		child = manage (new Gtk::Label (text));
	} else if (widget_type == "ENTRY") {
		child = manage (new Gtk::Entry ());
	} else if (widget_type == "FOCUSENTRY") {
		child = manage (new Gtkmm2ext::FocusEntry ());
	} else if (widget_type == "SPINBUTTON") {
		child = manage (new Gtk::SpinButton ());
    } else if (widget_type == "LAYOUT") {
		std::string hadjustment_id = xml_property (definition, "hadjustment", styles, "");
		std::string vadjustment_id = xml_property (definition, "vadjustment", styles, "");
		if (hadjustment_id.empty() && vadjustment_id.empty()) {
			child = manage (new Gtk::Layout);
		} else {
			if (hadjustment_id.empty()) {
				dbg_msg("Layout's hadjustment is NOT SPECIFIED!");
				abort ();
			}
			if (vadjustment_id.empty()) {
				dbg_msg("Layout's vadjustment is NOT SPECIFIED!");
				abort ();
			}
			child = manage (new Gtk::Layout (get_adjustment(hadjustment_id.c_str()), 
											 get_adjustment(vadjustment_id.c_str())));
		}
#ifdef ARDOUR_CANVAS_HAS_XMLUI                
	} else if (widget_type == "CANVAS") {
		std::map<std::string, ArdourCanvas::Item*> named_items;
		child = manage (new ArdourCanvas::GtkCanvas (definition, styles, named_items));
#endif
	} else if (widget_type == "SCROLLEDWINDOW") {
		child = manage (new Gtk::ScrolledWindow);
	} else if (widget_type == "FIXED") {
		child = manage (new Gtk::Fixed);
	} else if (widget_type == "WAVESGRID") {
		child = manage (new WavesGrid);
	} else if (widget_type == "VBOX") {
		child = manage (new Gtk::VBox);
	} else if (widget_type == "HBOX") {
		child = manage (new Gtk::HBox);
	} else if (widget_type == "EVENTBOX") {
		child = manage (new Gtk::EventBox);
	} else if (widget_type == "HPANED") {
		child = manage (new Gtk::HPaned);
	} else if (widget_type == "VPANED") {
		child = manage (new Gtk::VPaned);
	} else if (widget_type == "TABLE") {
		child = manage (new Gtk::Table (xml_property (definition, "rows", styles, 1),
									    xml_property (definition, "columns", styles, 1),
									    xml_property (definition, "homogeneous", styles, false)));
	} else if (widget_type == "FADER") {
		std::string face_image = xml_property (definition, "facesource", styles, "");
		if (face_image.empty()) {
			dbg_msg("Fader's facesource NOT SPECIFIED!");
			abort ();
		}

		std::string underlay_image = xml_property (definition, "underlaysource", styles, "");
		std::string active_face_image = xml_property (definition, "activefacesource", styles, "");

		std::string handle_image = xml_property (definition, "handlesource", styles, "");
		if (handle_image.empty()) {
			dbg_msg("Fader's handlesource NOT SPECIFIED!");
			abort ();
		}
		std::string active_handle_image = xml_property (definition, "activehandlesource", styles, handle_image);
		if (handle_image.empty()) {
			dbg_msg("Fader's handlesource NOT SPECIFIED!");
			abort ();
		}
		std::string adjustment_id = xml_property (definition, "adjustment", styles, "");
		if (adjustment_id.empty()) {
			dbg_msg("Fader's adjustment NOT SPECIFIED!");
			abort ();
		}
		int minposx = xml_property (definition, "minposx", styles, -1);
		int minposy = xml_property (definition, "minposy", styles, -1);
		int maxposx = xml_property (definition, "maxposx", styles, minposx);
		int maxposy = xml_property (definition, "maxposy", styles, minposy);
		Gtk::Adjustment& adjustment = get_adjustment(adjustment_id.c_str());
		bool read_only = xml_property (definition, "readonly", styles, false);

		child = manage (new Gtkmm2ext::Fader(adjustment, 
											 face_image,
											 active_face_image,
											 underlay_image,
											 handle_image,
											 active_handle_image,
											 minposx,
											 minposy,
											 maxposx,
											 maxposy,
											 read_only));
	} else if (widget_type == "ADJUSTMENT") {
		double min_value = xml_property (definition, "minvalue", styles, 0.0);
		double max_value = xml_property (definition, "maxvalue", styles, 100.0);
		double initial_value = xml_property (definition, "initialvalue", styles, min_value);
		double step = xml_property (definition, "step", styles, (max_value-min_value)/100.0);
		double page_increment = xml_property (definition, "pageincrement", styles, (max_value-min_value)/10);

		child = manage (new Gtk::Adjustment (initial_value,
											 min_value,
											 max_value,
											 step,
											 page_increment));
	} else if (widget_type != "STYLE") {
		dbg_msg (std::string("Illegal object type (" + 
							  definition.name() +
							  ") occurred in " +
							  _scrip_file_name +
							  "!"));
		abort ();
	}

	if (child != NULL) {
		if (!widget_id.empty()) {
			(*this)[widget_id] = child;
		}
		if (dynamic_cast<Gtk::Widget*> (child)) {
			set_attributes(*dynamic_cast<Gtk::Widget*> (child), definition, styles);
		}
	}
	
	Gtk::Widget* widget = (xml_property (definition, "ui.orphan", styles, false) ? 0 : dynamic_cast<Gtk::Widget*> (child));
	
	if (child && !widget) {
		_orphan_objects.push_back (child);
	}

	return widget;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Box& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		std::string property = xml_property (definition, "box.pack", styles, "start");
		bool expand = xml_property (definition, "box.expand", styles, false);
		bool fill = xml_property (definition, "box.fill", styles, false);
		uint32_t padding = xml_property (definition, "box.padding", styles, 0);
		
		if (property == "start") {
			parent.pack_start(*child, expand, fill, padding);
		} else {
			parent.pack_end(*child, expand, fill, padding);
		}

	}
	return child;
}

Gtk::Widget*
WavesUI::add_widget (Gtk::Fixed& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		parent.put (*child,
					xml_property (definition, "x", styles, 0), 
					xml_property (definition, "y", styles, 0));
	}
	return child;
}

Gtk::Widget*
WavesUI::add_widget (WavesGrid& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		parent.pack (*child);
	}
	return child;
}

Gtk::Widget*
WavesUI::add_widget (Gtk::Paned& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		unsigned pane = xml_property (definition, "paned.pack", styles, 1);
		bool resize = xml_property (definition, "paned.resize", styles, false);
		bool shrink = xml_property (definition, "paned.shrink", styles, false);
		
		switch (pane) {
			case 1:
				parent.pack1(*child, resize, shrink);
			break;
			case 2:
				parent.pack2(*child, resize, shrink);
			break;
			default:
				dbg_msg (std::string("Illegal panned.pack property used in " + _scrip_file_name + "!"));
				abort ();
			break;
		}

	}
	return child;
}

Gtk::Widget*
WavesUI::add_widget (Gtk::Table& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		unsigned left_attach = xml_property (definition, "table.leftattach", styles, 0);
		unsigned right_attach = xml_property (definition, "table.rightattach", styles, 0);
		unsigned top_attach = xml_property (definition, "table.topattach", styles, 0);
		unsigned bottom_attach = xml_property (definition, "table.bottomattach", styles, 0);
		Gtk::AttachOptions xoptions = (xml_property (definition, "table.xfill", styles, false) ? Gtk::FILL : Gtk::AttachOptions(0)) |
									  (xml_property (definition, "table.xexpand", styles, false) ? Gtk::EXPAND : Gtk::AttachOptions(0)) |
									  (xml_property (definition, "table.xshrink", styles, false) ? Gtk::SHRINK : Gtk::AttachOptions(0));
		Gtk::AttachOptions yoptions = (xml_property (definition, "table.yfill", styles, false) ? Gtk::FILL : Gtk::AttachOptions(0)) |
									  (xml_property (definition, "table.yexpand", styles, false) ? Gtk::EXPAND : Gtk::AttachOptions(0)) |
									  (xml_property (definition, "table.yshrink", styles, false) ? Gtk::SHRINK : Gtk::AttachOptions(0));
		unsigned xpadding =  xml_property (definition, "table.xpadding", styles, 0);
	    unsigned ypadding =  xml_property (definition, "table.ypadding", styles, 0);
		parent.attach (*child, 
					   left_attach,
					   right_attach,
					   top_attach,
					   bottom_attach,
					   xoptions,
					   yoptions,
					   xpadding,
					   ypadding);
	}
	return child;
}

Gtk::Widget*
WavesUI::add_widget (Gtk::ScrolledWindow& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		parent.add(*child);
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Window& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		parent.add(*child);
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::EventBox& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		parent.add(*child);
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Layout& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		parent.put (*child, 
					xml_property (definition, "x", styles, 0), 
					xml_property (definition, "y", styles, 0));
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Container& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = NULL;
	if(dynamic_cast<Gtk::Layout*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::Layout*> (&parent), definition, styles);
	} else if(dynamic_cast<Gtk::Box*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::Box*> (&parent), definition, styles);
	} else if(dynamic_cast<Gtk::Fixed*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::Fixed*> (&parent), definition, styles);
	} else if(dynamic_cast<Gtk::Paned*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::Paned*> (&parent), definition, styles);
	} else if(dynamic_cast<Gtk::Table*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::Table*> (&parent), definition, styles);
	} else if(dynamic_cast<Gtk::ScrolledWindow*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::ScrolledWindow*> (&parent), definition, styles);
	} else if(dynamic_cast<Gtk::Window*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::Window*> (&parent), definition, styles);
	} else if(dynamic_cast<Gtk::EventBox*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::EventBox*> (&parent), definition, styles);
	}

	Gtk::Container* container = dynamic_cast<Gtk::Container*> (child);

	if (container != NULL) {
		WavesUI::create_ui (definition.children(), styles, *container);
		Gtk::ScrolledWindow* sw = dynamic_cast<Gtk::ScrolledWindow*> (child);
		if (sw != NULL) {
			Gtk::Viewport* vp = (Gtk::Viewport*)sw->get_child();
			if (vp != NULL) {
				set_attributes(*(Gtk::Widget*)vp, definition, styles);
				vp->set_shadow_type(Gtk::SHADOW_NONE);
			}
		}
	}
	return child;
}

void
WavesUI::create_ui (const XMLNodeList& definition, const XMLNodeMap& styles, Gtk::Container& root)
{
	for (XMLNodeList::const_iterator i = definition.begin (); i != definition.end (); ++i) {
        if (!(**i).is_content()) {
			WavesUI::add_widget (root, **i, styles);
		}
	}
}

void
WavesUI::create_ui (const XMLTree& layout, Gtk::Container& root)
{
	XMLNodeMap styles;
	get_styles(layout, styles);
	const XMLNodeList& definition = layout.root()->children();
	WavesUI::create_ui (definition, styles, root);
}

const XMLTree*
WavesUI::load_layout (const std::string& xml_file_name)
{
	std::map<std::string, const XMLTree*>::const_iterator it = __xml_tree_cache.find(xml_file_name);
	if (it != __xml_tree_cache.end()) {
		return (*it).second;
	}

	std::string layout_file; 
	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths("ui");

	if (!find_file (spath, xml_file_name, layout_file)) {
		dbg_msg("File not found: " + xml_file_name);
		return NULL;
	}

	const XMLTree* tree = 0;
	try {
		tree = new XMLTree (layout_file, false);
		__xml_tree_cache[xml_file_name] = tree;
	} catch (...) {
		dbg_msg("Failure to load UI script!\nUI Script: \n\n\t" + layout_file);
		abort ();
	}
	return tree;
}

void 
WavesUI::set_attributes (Gtk::Widget& widget, const XMLNode& definition, const XMLNodeMap& styles)
{
	std::string property = xml_property (definition, "cssname", styles, "");
	if (!property.empty ()) {
		widget.set_name (property);
	} else {
		widget.unset_name ();
	}

	int height = xml_property (definition, "height", styles, -1);
	int width = xml_property (definition, "width", styles, -1);
	widget.set_size_request (width, height);
		
	property = xml_property (definition, "textcolornormal", styles, "");
	if (!property.empty ()) {
		widget.unset_text(Gtk::STATE_NORMAL);
		widget.modify_text(Gtk::STATE_NORMAL, Gdk::Color(property));
	}

	property = xml_property (definition, "textcoloractive", styles, "");
	if (!property.empty ()) {
		widget.unset_text(Gtk::STATE_ACTIVE);
		widget.modify_text(Gtk::STATE_ACTIVE, Gdk::Color(property));
	}

	property = xml_property (definition, "textcolorselected", styles, "");
	if (!property.empty ()) {
		widget.unset_text(Gtk::STATE_SELECTED);
		widget.modify_text(Gtk::STATE_SELECTED, Gdk::Color(property));
	}

	property = xml_property (definition, "basecolornormal", styles, "");
	if (!property.empty ()) {
		widget.unset_base(Gtk::STATE_NORMAL);
		widget.modify_base(Gtk::STATE_NORMAL, Gdk::Color(property));
	}

	property = xml_property (definition, "basecoloractive", styles, "");
	if (!property.empty ()) {
		widget.unset_base(Gtk::STATE_ACTIVE);
		widget.modify_base(Gtk::STATE_ACTIVE, Gdk::Color(property));
	}

	property = xml_property (definition, "basecolorselected", styles, "");
	if (!property.empty ()) {
		widget.unset_base(Gtk::STATE_SELECTED);
		widget.modify_base(Gtk::STATE_SELECTED, Gdk::Color(property));
	}

	property = xml_property (definition, "bgnormal", styles, "");
	if (!property.empty ()) {
		widget.unset_bg(Gtk::STATE_NORMAL);
		widget.modify_bg(Gtk::STATE_NORMAL, Gdk::Color(property));
	}

	property = xml_property (definition, "bgdisabled", styles, property);
	if (!property.empty ()) {
		widget.unset_bg(Gtk::STATE_INSENSITIVE);
		widget.modify_bg(Gtk::STATE_INSENSITIVE, Gdk::Color(property));
	}

	property = xml_property (definition, "bgactive", styles, "");
	if (!property.empty ()) {
		widget.unset_bg(Gtk::STATE_ACTIVE);
		widget.modify_bg(Gtk::STATE_ACTIVE, Gdk::Color(property));
	}

	property = xml_property (definition, "bghover", styles, "");
	if (!property.empty ()) {
		widget.unset_bg(Gtk::STATE_PRELIGHT);
		widget.modify_bg(Gtk::STATE_PRELIGHT, Gdk::Color(property));
	}

	property = xml_property (definition, "fgnormal", styles, "");
	if (!property.empty ()) {
		widget.modify_fg(Gtk::STATE_NORMAL, Gdk::Color(property));
	}

	property = xml_property (definition, "fgdisabled", styles, property);
	if (!property.empty ()) {
		widget.modify_fg(Gtk::STATE_INSENSITIVE, Gdk::Color(property));
	}

	property = xml_property (definition, "fgactive", styles, "");
	if (!property.empty ()) {
		widget.modify_fg(Gtk::STATE_ACTIVE, Gdk::Color(property));
	}

	property = xml_property (definition, "fghover", styles, "");
	if (!property.empty ()) {
		widget.modify_fg(Gtk::STATE_PRELIGHT, Gdk::Color(property));
	}

	property = xml_property (definition, "font", styles, "");
	if (!property.empty ()) {
		widget.modify_font(Pango::FontDescription(property));
	}

	widget.set_visible (xml_property (definition, "visible", styles, true));
	widget.set_no_show_all (xml_property (definition, "noshowall", styles, false));

	property = xml_property (definition, "tooltip", styles, "");
	if (!property.empty ()) {
		widget.set_tooltip_text (property);
	}

	Gtk::EventBox* event_box = dynamic_cast<Gtk::EventBox*> (&widget);
	if (event_box) {
		bool visible_window = xml_property (definition, "visiblewindow", styles, true);
		event_box->set_visible_window (visible_window);
	}

	Gtk::Label* label = dynamic_cast<Gtk::Label*> (&widget);
	if (label) {
		property = xml_property (definition, "justify", styles, "left");
		std::transform(property.begin(), property.end(), property.begin(), ::tolower);
		Gtk::Justification justification = Gtk::JUSTIFY_LEFT;
		if (property == "left") {
			justification = Gtk::JUSTIFY_LEFT;
		} else if (property == "right") {
			justification = Gtk::JUSTIFY_RIGHT;
		} else if (property == "center") {
			justification = Gtk::JUSTIFY_CENTER;
		} else if (property == "fill") {
			justification = Gtk::JUSTIFY_FILL;
		} else {
			dbg_msg ("Invalid justification for Gtk::Label !");
		}
		label->set_justify (justification);
		
		property = xml_property (definition, "ellipsize", styles, "none");
		std::transform(property.begin(), property.end(), property.begin(), ::tolower);
		Pango::EllipsizeMode ellipsize_mode = Pango::ELLIPSIZE_NONE;
		if (property == "none") {
			ellipsize_mode = Pango::ELLIPSIZE_NONE;
		} else if (property == "start") {
			ellipsize_mode = Pango::ELLIPSIZE_START;
		} else if (property == "middle") {
			ellipsize_mode = Pango::ELLIPSIZE_MIDDLE;
		} else if (property == "end") {
			ellipsize_mode = Pango::ELLIPSIZE_END;
		} else {
			dbg_msg ("Invalid ellipsize mode for Gtk::Label !");
		}
		label->set_ellipsize (ellipsize_mode);
	}

	Gtk::Box* box = dynamic_cast<Gtk::Box*> (&widget);
	if (box) {
		box->set_spacing(xml_property (definition, "spacing", styles, 0));
	}

	Gtk::Container* container = dynamic_cast<Gtk::Container*> (&widget);
	if (container && !dynamic_cast<WavesButton*> (&widget)) {
		container->set_border_width (xml_property (definition, "borderwidth", styles, 0));
	}

	WavesButton* button = dynamic_cast<WavesButton*> (&widget);
	if (button) {
		button->set_border_width (xml_property (definition, "borderwidth", styles, "0").c_str());
		button->set_border_color (xml_property (definition, "bordercolor", styles, "#000000").c_str());
		button->set_active (xml_property (definition, "active", styles, false));
	}

	WavesIconButton* iconbutton = dynamic_cast<WavesIconButton*> (&widget);
	if (iconbutton) {
		property = xml_property (definition, "normalicon", styles, "");
		if (!property.empty ()) {
			iconbutton->set_normal_image (get_icon (property.c_str ()));
		}
		property = xml_property (definition, "activeicon", styles, "");
		if (!property.empty ()) {
			iconbutton->set_active_image (get_icon (property.c_str ()));
		}
		property = xml_property (definition, "prelighticon", styles, "");
		if (!property.empty ()) {
			iconbutton->set_prelight_image (get_icon (property.c_str ()));
		}
		property = xml_property (definition, "inactiveicon", styles, "");
		if (!property.empty ()) {
			iconbutton->set_inactive_image (get_icon (property.c_str ()));
		}
		property = xml_property (definition, "implicitactiveicon", styles, "");
		if (!property.empty ()) {
			iconbutton->set_implicit_active_image (get_icon (property.c_str ()));
		}
	}

	Gtk::Table* table = dynamic_cast<Gtk::Table*> (&widget);
	if (table) {
		table->set_col_spacings (xml_property (definition, "columnspacing", styles, 0));
		table->set_row_spacings (xml_property (definition, "rowspacing", styles, 0));
	}

	Gtk::ScrolledWindow* scrolled_window = dynamic_cast<Gtk::ScrolledWindow*> (&widget);
	if (scrolled_window) {
		Gtk::PolicyType hscrollbar_policy = Gtk::POLICY_AUTOMATIC; 
		Gtk::PolicyType vscrollbar_policy = Gtk::POLICY_AUTOMATIC;
		property = xml_property (definition, "hscroll", styles, "");
		if (property == "never") {
			hscrollbar_policy = Gtk::POLICY_NEVER;
		} else if (property == "always") {
			hscrollbar_policy = Gtk::POLICY_ALWAYS;
		} else if (property == "auto") {
			hscrollbar_policy = Gtk::POLICY_AUTOMATIC; 
		}
		property = xml_property (definition, "vscroll", styles, "");
		if (property == "never") {
			vscrollbar_policy = Gtk::POLICY_NEVER;
		} else if (property == "always") {
			vscrollbar_policy = Gtk::POLICY_ALWAYS;
		} else if (property == "auto") {
			vscrollbar_policy = Gtk::POLICY_AUTOMATIC; 
		}
		scrolled_window->set_policy(hscrollbar_policy, vscrollbar_policy);
	}
}

Gtk::Object* 
WavesUI::get_object(const char *id)
{
	Gtk::Object* object = NULL;
	WavesUI::iterator it = find(id);
	if(it != end())
		object = it->second;

	return object;
}

Gtk::Adjustment&
WavesUI::get_adjustment(const char* id)
{
	Gtk::Adjustment* child = dynamic_cast<Gtk::Adjustment*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Adjustment ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

Gtk::Container&
WavesUI::get_container (const char* id)
{
	Gtk::Container* child = dynamic_cast<Gtk::Container*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::Container ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}


Gtk::EventBox&
WavesUI::get_event_box (const char* id)
{
	Gtk::EventBox* child = dynamic_cast<Gtk::EventBox*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::EventBox ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}


Gtk::Box&
WavesUI::get_box (const char* id)
{
	Gtk::Box* child = dynamic_cast<Gtk::Box*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::Box ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}


Gtk::VBox&
WavesUI::get_v_box (const char* id)
{
	Gtk::VBox* child = dynamic_cast<Gtk::VBox*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::VBox ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}


Gtk::HBox&
WavesUI::get_h_box (const char* id)
{
	Gtk::HBox* child = dynamic_cast<Gtk::HBox*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::HBox ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}


Gtk::Fixed&
WavesUI::get_fixed (const char* id)
{
	Gtk::Fixed* child = dynamic_cast<Gtk::Fixed*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::Fixed ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

WavesGrid&
WavesUI::get_waves_grid (const char* id)
{
	WavesGrid* child = dynamic_cast<WavesGrid*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("WavesGrid ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

Gtk::Paned&
WavesUI::get_paned (const char* id)
{
	Gtk::Paned* child = dynamic_cast<Gtk::Paned*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::Paned ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

Gtk::HPaned&
WavesUI::get_h_paned (const char* id)
{
	Gtk::HPaned* child = dynamic_cast<Gtk::HPaned*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::HPaned ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

Gtk::VPaned&
WavesUI::get_v_paned (const char* id)
{
	Gtk::VPaned* child = dynamic_cast<Gtk::VPaned*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::VPaned ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

Gtk::Table& 
WavesUI::get_table (const char* id)
{
	Gtk::Table* child = dynamic_cast<Gtk::Table*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::Table ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

Gtk::Layout&
WavesUI::get_layout (const char* id)
{
	Gtk::Layout* child = dynamic_cast<Gtk::Layout*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::Layout ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}


Gtk::Label&
WavesUI::get_label (const char* id)
{
	Gtk::Label* child = dynamic_cast<Gtk::Label*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::Label ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

Gtk::Image&
WavesUI::get_image (const char* id)
{
	Gtk::Image* child = dynamic_cast<Gtk::Image*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::Image ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}


Gtk::ComboBoxText&
WavesUI::get_combo_box_text (const char* id)
{
	Gtk::ComboBoxText* child = dynamic_cast<Gtk::ComboBoxText*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::ComboBoxText ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}


Gtk::Entry&
WavesUI::get_entry(const char* id)
{
	Gtk::Entry* child = dynamic_cast<Gtk::Entry*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::Entry ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

Gtkmm2ext::FocusEntry&
WavesUI::get_focus_entry(const char* id)
{
	Gtkmm2ext::FocusEntry* child = dynamic_cast<Gtkmm2ext::FocusEntry*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtkmm2ext::FocusEntry ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

Gtk::SpinButton&
WavesUI::get_spin_button(const char* id)
{
	Gtk::SpinButton* child = dynamic_cast<Gtk::SpinButton*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::SpinButton ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

WavesButton&
WavesUI::get_waves_button (const char* id)
{
	WavesButton* child = dynamic_cast<WavesButton*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("WavesButton ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}

Gtkmm2ext::Fader&
WavesUI::get_fader (const char* id)
{
	Gtkmm2ext::Fader* child = dynamic_cast<Gtkmm2ext::Fader*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtkmm2ext::Fader ") + id + " not found in " + _scrip_file_name + "!");
		abort ();
	}
	return *child;
}
