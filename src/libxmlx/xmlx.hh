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

#ifndef __XMLX_HH__
#define __XMLX_HH__


// libxml
#include <libxml/tree.h>
#include <libxml/parser.h>

// libstdc++
#include <iostream>
#include <string>
#include <list>
#include <stdexcept>
#include <sstream>

using namespace std;

/// libxmlx namespace
/**
 * all libxmlx classes are contained within the XML:: namespace.
 *
 * libxmlx - A simple C++ interface for libxml
 *
 *
 * Copyright (C) 2001 Joost Yervante Damad
 *
 * Licence: GPL
 *
 */
namespace XML
{

/**
 * Exception used by the libxmlx code to signal errors back to
 * users of the library
 */
class Exception: public runtime_error
{
public:
	Exception(const string &msg);
	Exception(const Exception &foo);
	Exception &operator=(const Exception &foo);

private:
	Exception();                                // block usage
	//Exception(const Exception &foo);            // block usage
	//Exception &operator=(const Exception &foo); // block usage
};

class Document;
class Element;

/**
 * An XML attribute, also known as a property.
 * Example: <node attr="56">foo</node>
 * here attr is an XML attribute, with name 'attr' and
 * value '56'
 */
class Attribute
{
friend class Element;
friend class Document;
public:
	~Attribute();

	/// name of the attribute
	string name;
	/// value of the attribute
	string value;

	void dump(int recursion=0);

private:
	/// construct attribute from libxml xmlAttrPtr node
	Attribute(xmlAttrPtr node);
	Attribute();                                // block usage
	Attribute(const Attribute &foo);            // block usage
	Attribute &operator=(const Attribute &foo); // block usage
};

/// An XML Element
/**
 * An XML Element.
 * Example: <node attr="56" bar="qw">foo<node>
 * here attr and bar are the elements' attributes,
 * and 'foo' is it's pcdata
 * Example2: <node><child1/><child2/></node>
 * here child1 and child2 are the node's children
 */
class Element
{
friend class Document;
public:
	~Element();

	/// name of the element
	string name;

	/// content of the element
	string pcdata;
	
	/// contained elements
	list<Element*> children;

	/// attributes of this element
	list<Attribute*> attributes;

	void dump(int recursion=0);
	
	/**
	 * get attribute's value from attribute with name 'attName' from
	 * attributelist
	 */
	const string &getAttribute(const string &attName);

	/**
	 * get pcdata from element from children with name 'childName'
	 */
	const string &getElementPcdata(const string &childName);

	/**
	 * get attribute with name 'attName' from element from children
	 * with name 'childName'
	 */
	const string &getElementAttribute(const string &childName,
	                                       const string &attName);

private:
	/// construct attribute from libxml xmlNodePtr node
	Element(xmlNodePtr node);
	Element();                              // block usage
	Element(const Element &foo);            // block usage
	Element &operator=(const Element &foo); // block usage
};
/// An XML Document
/**
 * The XML Document contains the pointer to the root-element
 * of the XML tree.
 */
class Document
{
public:
	/// create from filename
	Document(const string &filename);
	/// create from stringstream
	Document(const ostringstream &stream);
	~Document();

	/// root XML element of the document
	Element *root;

	void dump();

	/**
	 * convert back to libxml xmlDoc
	 * for saving back to disk
	 * !this interface should not be public!
	 */
	xmlDocPtr convertToXmlDoc();
	
private:
	Document();                               // block usage
	Document(const Document &foo);            // block usage
	Document &operator=(const Document &foo); // block usage
	
	/// filename used when constructed from filename
	string filename;

	void handleDoc(xmlDocPtr doc);
};

/**
 * XML escape a string
 * ! changes the string in place!
 * returns const reference to changed self
 * for easy chaining in streams
 * sample:
 * //string stest("hello & world");
 * //cout << XML::Escape(stest) << endl;
 */
const string &Escape(string &str);

}; // end namespace XML

#endif // __XMLX_HH__
