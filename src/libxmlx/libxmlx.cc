// $Id$
//
// libxmlx - A simple C++ interface for libxml
//

// Copyright (C) 2001 Joost Yervante Damad
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version, or the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// and the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
//

#include "xmlx.hh"

#include <libxml/entities.h>

namespace XML {

#ifdef XMLX_DEBUG
static void s_dump(xmlNodePtr node, int recursion=0)
{
	if (node == 0) {
		return;
	}
	for (int i = 0; i < recursion; ++i) {
		cout << "--";
	}
	cout << " type: " << node->type;
	cout << " name: " << (const char*)node->name;
	if (node->type==XML_TEXT_NODE)
	{
		cout << " pcdata: " << node->content;
	}
	cout << endl;
	for (xmlNodePtr c = node->children; c != 0; c = c->next) {
		s_dump(c,recursion+1);
	}
	if (node->type==XML_ELEMENT_NODE) {
		for (xmlAttrPtr c = node->properties; c != 0; c = c->next) {
			s_dump((xmlNodePtr)c,recursion+1);
		}
	}
}
#endif

static string empty("");

Exception::Exception(const string &msg)
	: runtime_error(msg)
{
}

Exception::Exception(const Exception &e)
:runtime_error(e)
{
}

Attribute::Attribute(xmlAttrPtr node)
{
	name  = string((const char*)node->name);
	value = string((const char*)node->children->content);
}

Attribute::~Attribute()
{
}

void Attribute::dump(int recursion)
{
	for (int i = 0; i < recursion; ++i) {
		cout << "--";
	}
	cout << "Attribute " << name << " value:" << value << endl;
}

Document::Document(const string &filename_)
	: root(0), filename(filename_)
{
	xmlDocPtr doc = xmlParseFile(filename.c_str());
	if (!doc) {
		throw Exception("Document parsing failed");
	}
	handleDoc(doc);
}

void Document::handleDoc(xmlDocPtr doc)
{
	if (!doc->children || !doc->children->name) {
		xmlFreeDoc(doc);
		throw Exception("Document doesn't contain mandatory root Element");
	}
	root = new Element(xmlDocGetRootElement(doc));
#ifdef XMLX_DEBUG
	s_dump(xmlDocGetRootElement(doc));
#endif
	xmlFreeDoc(doc);
}

Document::Document(const ostringstream &stream)
	: root(0), filename("stringstream")
{
	xmlDocPtr doc = xmlParseMemory(stream.str().c_str(), stream.str().length());
	handleDoc(doc);
}

void Document::dump()
{
	cout << "File: " << filename << endl;
	root->dump(0);
}

Document::~Document()
{
	delete root;
}

Element::Element(xmlNodePtr node)
:pcdata("")
{
	name = string((const char*)node->name);
	for (xmlNodePtr x = node->children; x != 0 ; x = x->next) {
		if (x==0) break;
		switch (x->type) {
			case XML_TEXT_NODE:
			pcdata += string((const char*)x->content);
			break;

			case XML_ELEMENT_NODE:
			children.push_back(new Element(x));
			break;

			default:
#ifdef XMLX_DEBUG
			cout << "skipping node with type: " << x->type << endl;
#endif
			break;
		}
	}
	for (xmlAttrPtr x = node->properties; x != 0 ; x = x->next) {
		if (x == 0) {
			break;
		}
		switch (x->type) {
			case XML_ATTRIBUTE_NODE:
			attributes.push_back(new Attribute(x));
			break;

			default:
#ifdef XMLX_DEBUG
			cout << "skipping node with type: " << x->type << endl;
#endif
			break;
		}
	}
}

Element::~Element()
{
	for (list<Attribute*>::iterator i = attributes.begin();
	     i != attributes.end();
	     ++i) {
		delete (*i);
	}
	for (list<Element*>::iterator i = children.begin();
	     i != children.end();
	     ++i) {
		delete (*i);
	}
}

void Element::dump(int recursion)
{
	for (int i = 0; i < recursion; ++i) {
		cout << "--";
	}
	cout << "Element " << name << " pcdata:" << pcdata << endl;
	for (list<Attribute*>::iterator i = attributes.begin();
	     i != attributes.end();
	     ++i) {
		(*i)->dump(recursion + 1);
	}
	for (list<Element*>::iterator i = children.begin();
	     i != children.end();
	     ++i) {
		(*i)->dump(recursion + 1);
	}
}

const string &Element::getAttribute(const string &attName)
{
	for (list<Attribute*>::iterator i = attributes.begin();
	     i != attributes.end();
	     ++i) {
		if ((*i)->name == attName) {
			return (*i)->value;
		}
	}
	return empty;
}

const string &Element::getElementPcdata(const string &childName)
{
	for (list<Element*>::iterator i = children.begin();
	     i != children.end();
	     ++i) {
		if ((*i)->name == childName) {
			return (*i)->pcdata;
		}
	}
	return empty;
}

const string &Element::getElementAttribute(const string &childName,
                                           const string &attName)
{
	for (list<Element*>::iterator i = children.begin();
	     i != children.end();
	     ++i) {
		if ((*i)->name == attName) {
			return (*i)->getAttribute(childName);
		}
	}
	return empty;
}

xmlDocPtr Document::convertToXmlDoc()
{
	//xmlDocPtr d = xmlNewDoc("1.0");
	// xmlNewDtd(d, root->name, const xmlChar *ExternalID, const xmlChar *SystemID);
	return 0;
}

const string &Escape(string &str)
{
	xmlChar* buffer = NULL;
	buffer = xmlEncodeEntitiesReentrant(NULL, (const xmlChar*)str.c_str());
	str = (const char*)buffer;
	// buffer is allocated in C code, soo we free it the C-way:
	if (buffer != NULL) {
		free(buffer);
	}
	return str;
}

}; // end namespace XML
