#include "PDBSymbolSorter.h"
#include <cassert>

std::vector<DWORD>& PDBSymbolSorter::GetSortedSymbolIndexes()
{
    return m_sortedSymbolIndexes;
}

PDBSymbolSorterBase::ImageArchitecture PDBSymbolSorter::GetImageArchitecture() const
{
    return m_architecture;
}

void PDBSymbolSorter::Clear()
{
    ImageArchitecture m_architecture = ImageArchitecture::None;

    m_visitedUdts.clear();
    m_sortedSymbolIndexes.clear();
}

void PDBSymbolSorter::VisitEnumType(const Symbol& symbol)
{
    if (HasBeenVisited(symbol))
    {
        return;
    }

    AddSymbol(symbol);
}

void PDBSymbolSorter::VisitPointerType(const Symbol& symbol)
{
    if (m_architecture == ImageArchitecture::None)
    {
        switch (symbol.size)
        {
        case 4:
            m_architecture = ImageArchitecture::x86;
            break;
        case 8:
            m_architecture = ImageArchitecture::x64;
            break;
        default:
            assert(0);
            break;
        }
    }
}

void PDBSymbolSorter::VisitUdt(const Symbol& symbol)
{
    if (HasBeenVisited(symbol))
    {
        return;
    }

    PDBSymbolVisitorBase::VisitUdt(symbol);

    AddSymbol(symbol);
}

void PDBSymbolSorter::VisitUdtField(const SymbolUdtField& udtField)
{
    assert(udtField.type);
    Visit(*udtField.type);
}

bool PDBSymbolSorter::HasBeenVisited(const Symbol& symbol)
{
    static DWORD UnnamedCounter = 0;

    auto key = symbol.name;
    if (m_visitedUdts.find(key) != m_visitedUdts.end())
    {
        return true;
    }

    if (PDB::IsUnnamedSymbol(symbol))
    {
        key += std::to_string(++UnnamedCounter);
    }

    m_visitedUdts.emplace(std::move(key), symbol.symIndexId);
    return false;
}

void PDBSymbolSorter::AddSymbol(const Symbol& symbol)
{
    if (std::find(m_sortedSymbolIndexes.begin(), m_sortedSymbolIndexes.end(), symbol.symIndexId) == m_sortedSymbolIndexes.end())
    {
        m_sortedSymbolIndexes.push_back(symbol.symIndexId);
    }
}
