// $Id$
//
// test program for libxmlx
//
// (c) 2002 Joost Yervante Damad
//

#include "xmlx.hh"

int main(int argc, char** argv)
{

	if (argc < 2)
	{
		cout << "Usage: " << argv[0] << " <file>" << endl;
		exit(1);
	}
		
	string filename(argv[1]);

	XML::Document doc(filename);

	doc.dump();

	return 0;
}

