#include "PDBExtractor.h"

#pragma comment(lib, "dbghelp.lib")

int main(int argc, char** argv)
{
    int result = 0;

    try
    {
        result = PDBExtractor{}.Run(argc, argv);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        result = 1;
    }

    return result;
}
