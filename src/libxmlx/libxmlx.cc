// $Id$
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

#include "xmlx.hh"

namespace XML {

#ifdef XMLX_DEBUG
static void s_dump(xmlNodePtr node, int recursion=0)
{
	if (node==0) return;
	for (int i=0; i< recursion; i++) std::cout << "--";
	std::cout << " type: " << node->type;
	std::cout << " name: " << (const char*)node->name;
	if (node->type==XML_TEXT_NODE)
	{
		std::cout << " pcdata: " << node->content;
	}
	std::cout << std::endl;
	for (xmlNodePtr c=node->children; c!=0 ; c=c->next)
	{
		s_dump(c,recursion+1);
	}
	if (node->type==XML_ELEMENT_NODE)
	{
		for (xmlAttrPtr c=node->properties; c!=0 ; c=c->next)
		{
			s_dump((xmlNodePtr)c,recursion+1);
		}
	}
}
#endif
		
Exception::Exception(const std::string &msg)
:runtime_error(msg)
{
}

Exception::Exception(const Exception &e)
:runtime_error(e)
{
}

Attribute::Attribute(xmlAttrPtr node)
{
	name  = std::string((const char*)node->name);
	value = std::string((const char*)node->children->content);
}

Attribute::~Attribute()
{
}

void Attribute::dump(int recursion=0)
{
	for (int i=0; i < recursion; i++) std::cout << "--";
	std::cout << "Attribute " << name << " value:" << value << std::endl;
}

Document::Document(const std::string &filename_)
:root(0), filename(filename_)
{
	xmlDocPtr doc = xmlParseFile(filename.c_str());
	if (!doc->children || !doc->children->name)
	{
		xmlFreeDoc(doc);
		throw Exception("Document doesn't contain mandatory root Element");
	}
	root = new Element(xmlDocGetRootElement(doc));
#ifdef XMLX_DEBUG
	s_dump(xmlDocGetRootElement(doc));
#endif
	xmlFreeDoc(doc);
}

void Document::dump()
{
	std::cout << "File: " << filename << std::endl;
	root->dump(0);
}

Document::~Document()
{
	if (root != 0) delete root;
}

Element::Element(xmlNodePtr node)
:pcdata("")
{
	name = std::string((const char*)node->name);
	for (xmlNodePtr x = node->children; x != 0 ; x=x->next)
	{
		if (x==0) break;
		switch (x->type)
		{
			case XML_TEXT_NODE:
			pcdata += std::string((const char*)x->content);
			break;

			case XML_ELEMENT_NODE:
			children.push_back(new Element(x));
			break;

			default:
#ifdef XMLX_DEBUG
			std::cout << "skipping node with type: " << x->type << std::endl;
#endif
			break;
		}
	}
	for (xmlAttrPtr x = node->properties; x != 0 ; x=x->next)
	{
		if (x==0) break;
		switch (x->type)
		{
			case XML_ATTRIBUTE_NODE:
			attributes.push_back(new Attribute(x));
			break;

			default:
#ifdef XMLX_DEBUG
			std::cout << "skipping node with type: " << x->type << std::endl;
#endif
			break;
		}
	}
}

Element::~Element()
{
	for (std::list<Attribute*>::iterator i = attributes.begin(); i != attributes.end(); i++)
	{
		delete (*i);
	}
	for (std::list<Element*>::iterator i = children.begin(); i != children.end(); i++)
	{
		delete (*i);
	}
}

void Element::dump(int recursion=0)
{
	for (int i=0; i < recursion; i++) std::cout << "--";
	std::cout << "Element " << name << " pcdata:" << pcdata << std::endl;
	for (std::list<Attribute*>::iterator i = attributes.begin(); i != attributes.end(); i++)
	{
		(*i)->dump(recursion+1);
	}
	for (std::list<Element*>::iterator i = children.begin(); i != children.end(); i++)
	{
		(*i)->dump(recursion+1);
	}
}

}; // end namespace XML
