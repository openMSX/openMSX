// $Id$
//
// bin2c: a simple program that converts arbitrary binary files
//        in arrays of unsigned chars
//
// in the context of openmsx, this can be used to convert e.g.
// romfiles to code ;-)
//
// (c) 2002 Joost Yervante Damad <andete@worldcity.nl>
//
// License: GPL
//
// License of generated code: up to the user

#include <cstdio>

#include <iostream>
#include <string>
#include <sstream>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }
    FILE* f = fopen(argv[1],"r");
    if (f == 0)
    {
        std::cerr << "Illegal filename: " << argv[1] << std::endl;
        return 1;
    }

	for (unsigned i=0; i < strlen(argv[1]); i++)
	{
		if (argv[1][i]=='.')
		{
			argv[1][i] = '_';
		}
	}
	
    std::cout << "unsigned char " << argv[1] << "[] = {" << std::endl;

    size_t s = 1;
    size_t length = 0;
    while (s == 1)
    {
        unsigned char buf[1];
        s = fread((void*)buf , 1, 1, f);
        if (s > 0)
        {
            length += s;
        }
        std::cout << (int)buf[0] << ",";
        if ((length % 16) == 0)
        {
            std::cout << std::endl;
        }
    }
    std::cout << "};\n\n\n" <<  std::endl;
    std::cerr << length << " bytes read." << std::endl;
    fclose(f);
    return 0;
}
