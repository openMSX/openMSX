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

namespace XML
{

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

class Attribute
{
public:
	Attribute(xmlAttrPtr node);
	~Attribute();

	std::string name;
	std::string value;

	void dump(int recursion=0);

private:
	Attribute();                                // block usage
	Attribute(const Attribute &foo);            // block usage
	Attribute &operator=(const Attribute &foo); // block usage
};

class Element
{
public:
	Element(xmlNodePtr node);
	~Element();

	// name of element
	std::string name;

	// content of element
	std::string pcdata;
	
	// contained elements
	std::list<Element*> children;

	// attributes [AKA properties]
	std::list<Attribute*> attributes;

	void dump(int recursion=0);

private:
	Element();                              // block usage
	Element(const Element &foo);            // block usage
	Element &operator=(const Element &foo); // block usage
};

class Document
{
public:
	Document(const std::string &filename);
	~Document();

	// root element of document
	Element *root;

	void dump();

private:
	Document();                               // block usage
	Document(const Document &foo);            // block usage
	Document &operator=(const Document &foo); // block usage

	std::string filename;
};

}; // end namespace XML

