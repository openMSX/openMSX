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

//#include <stdio.h>
//#include <sys/types.h>
//#include "system.h"

// libxml
#include <tree.h>
#include <parser.h>

// libstdc++
#include <string>
#include <list>
#include <stdexcept>
#include <sstream>

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
class Exception: public std::runtime_error
{
public:
	Exception(const std::string &msg);
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
	std::string name;
	/// value of the attribute
	std::string value;

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
	std::string name;

	/// content of the element
	std::string pcdata;
	
	/// contained elements
	std::list<Element*> children;

	/// attributes of this element
	std::list<Attribute*> attributes;

	void dump(int recursion=0);
	
	/**
	 * get attribute's value from attribute with name 'name' from
	 * attributelist
	 */
	const std::string &getAttribute(const std::string &name);

	/**
	 * get pcdata from element from children with name 'name'
	 */
	const std::string &getElementPcdata(const std::string &name);

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
	Document(const std::string &filename);
	/// create from stringstream
	Document(const std::ostringstream &stream);
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
	std::string filename;

	void handleDoc(xmlDocPtr doc);
};

/**
 * XML escape a string
 * ! changes the string in place!
 * returns const reference to changed self
 * for easy chaining in streams
 * sample:
 * //std::string stest("hello & world");
 * //std::cout << XML::Escape(stest) << std::endl;
 */
const std::string &Escape(std::string &str);

}; // end namespace XML

