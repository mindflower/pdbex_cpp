#pragma once
#include "PDBSymbolVisitorBase.h"

#include <vector>

class PDBSymbolSorterBase : public PDBSymbolVisitorBase
{
public:
    enum class ImageArchitecture
    {
        None,
        x86,
        x64,
    };

    virtual	std::vector<DWORD>& GetSortedSymbolIndexes() = 0;
    virtual	ImageArchitecture GetImageArchitecture() const = 0;
    virtual	void Clear() = 0;
};
