//
// libxmlx - A simple C++ interface for libxml
//

// Copyright (C) 2001 Joost Yervante Damad
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
//

#include "libxmlx.hh"

namespace XML {

Exception::Exception(const std::string &msg)
:runtime_error(msg)
{
}

Exception::Exception(const Exception &e)
:runtime_error(e)
{
}

Attribute::Attribute(const std::string &name_, const std::string &value_)
:name(name_), value(value_)
{
}

Element::Element(const std::string &name_)
:name(name_),pcdata("")
{
}

Document::Document(const std::string &filename_)
:filename(filename_),doc(0)
{
	doc = xmlParseFile(filename.c_str());
	if (!doc->children || !doc->children->name)
	{
		xmlFreeDoc(doc);
		throw Exception("Document doesn't contain mandatory root Element");
	}
	libxml_to_tree();
}

Document::~Document()
{
	if (doc != 0) ::xmlFreeDoc(doc);
}

void Document::libxml_to_tree()
{
}

}; // end namespace XML
