#pragma once
#include "PDBSymbolSorterBase.h"

#include <vector>
#include <map>

class PDBSymbolSorter : public PDBSymbolSorterBase
{
public:
    std::vector<DWORD>& GetSortedSymbolIndexes() override;
    ImageArchitecture GetImageArchitecture() const override;
    void Clear() override;

protected:
    void VisitEnumType(const Symbol& symbol) override;
    void VisitPointerType(const Symbol& symbol) override;
    void VisitUdt(const Symbol& symbol) override;
    void VisitUdtField(const SymbolUdtField& udtField) override;

private:
    bool HasBeenVisited(const Symbol& symbol);
    void AddSymbol(const Symbol& symbol);

private:
    ImageArchitecture m_architecture = ImageArchitecture::None;
    std::map<std::string, DWORD> m_visitedUdts;
    std::vector<DWORD> m_sortedSymbolIndexes;
};
