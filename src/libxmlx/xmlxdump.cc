// $Id$
//
// test program for libxmlx

#include "xmlx.hh"

int main(int argc, char** argv)
{

	if (argc < 2)
	{
		std::cout << "Usage: " << argv[0] << " <file>" << std::endl;
		exit(1);
	}
		
	std::string filename(argv[1]);

	XML::Document doc(filename);

	doc.dump();

	return 0;
}
