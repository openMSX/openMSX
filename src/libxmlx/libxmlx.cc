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
static void dump(xmlNodePtr node, int recursion=0)
{
	if (node==0) return;
	for (int i=0; i< recursion; i++) std::cout << "--";
	std::cout << " type: " << node->type;
	std::cout << " name: " << (const char*)node->name;
	std::cout << std::endl;
	for (xmlNodePtr c=node->children; c!=0 ; c=c->next)
	{
		dump(c,recursion+1);
	}
	if (node->type==1)for (xmlAttrPtr c=node->properties; c!=0 ; c=c->next)
	{
		dump((xmlNodePtr)c,recursion+1);
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

Attribute::Attribute(xmlNodePtr node)
{
	name  = std::string((const char*)node->name);
	value = std::string((const char*)node->doc);
}

Attribute::~Attribute()
{
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
	// skip all non-element root-nodes and find the one
	// root node
	xmlNodePtr findRoot = doc->children;
	while (findRoot != 0)
	{
		if (findRoot->type==XML_ELEMENT_NODE) break;
		findRoot=findRoot->next;
	}
	if (findRoot == 0) assert(false);
	root = new Element(findRoot);
#ifdef XMLX_DEBUG
	dump(findRoot);
#endif
	xmlFreeDoc(doc);
}

Document::~Document()
{
	if (root != 0) delete root;
}

Element::Element(xmlNodePtr node)
{
	name = std::string((const char*)node->name);
	for (xmlNodePtr x = node->children; (node->children != 0 && x->next != 0) ; x=x->next)
	{
		switch (x->type)
		{
			case XML_TEXT_NODE:
			pcdata = std::string((const char*)x->content);
			break;

			case XML_ATTRIBUTE_NODE:
			attributes.push_back(new Attribute(x));
			break;

			case XML_ELEMENT_NODE:
			children.push_back(new Element(x));
			break;

			default:
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

}; // end namespace XML
