#include <fstream>
#include "RomTypes.hh"


int main(int argc, char** argv)
{
	for (int i=1; i<argc; i++) {
		std::cout << argv[i] << ":\t";
		
		std::ifstream file(argv[i]);
		if (file.fail())
			throw "Error opening file";
		file.seekg(0, std::ios::end);
		int size = file.tellg();
		file.seekg(0, std::ios::beg);

		byte* buffer = new byte[size];
		file.read((char*)buffer, size);
		if (file.fail())
			throw "Error reading file";

		try {
			int dbType    = RomTypes::searchDataBase (buffer, size);
			int guessType = RomTypes::guessMapperType(buffer, size);
			if (dbType == guessType) {
				std::cout << "OK" << std::endl;
			} else {
				std::cout << "NOT OK" << std::endl;
			}
		} catch (NotInDataBaseException &e) {
			std::cout << "Not in database" << std::endl;
		}
		
		delete[] buffer;
	}
}
