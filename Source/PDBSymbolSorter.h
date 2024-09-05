#pragma once
#include "PDBSymbolSorterBase.h"

#include <cassert>
#include <string>
#include <vector>
#include <map>

class PDBSymbolSorter : public PDBSymbolSorterBase
{
public:
    std::vector<DWORD>& GetSortedSymbolIndexes() override
    {
        return m_sortedSymbolIndexes;
    }

    ImageArchitecture GetImageArchitecture() const override
    {
        return m_architecture;
    }

    void Clear() override
    {
        ImageArchitecture m_architecture = ImageArchitecture::None;

        m_visitedUdts.clear();
        m_sortedSymbolIndexes.clear();
    }

protected:
    void VisitEnumType(const Symbol& symbol) override
    {
        if (HasBeenVisited(symbol))
        {
            return;
        }

        AddSymbol(symbol);
    }

    void VisitPointerType(const Symbol& symbol) override
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

    void VisitUdt(const Symbol& symbol) override
    {
        if (HasBeenVisited(symbol))
        {
            return;
        }

        PDBSymbolVisitorBase::VisitUdt(symbol);

        AddSymbol(symbol);
    }

    void VisitUdtField(const SymbolUdtField& udtField) override
    {
        assert(udtField.type);
        Visit(*udtField.type);
    }

private:
    bool HasBeenVisited(const Symbol& symbol)
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

    void AddSymbol(const Symbol& symbol)
    {
        if (std::find(m_sortedSymbolIndexes.begin(), m_sortedSymbolIndexes.end(), symbol.symIndexId) == m_sortedSymbolIndexes.end())
        {
            m_sortedSymbolIndexes.push_back(symbol.symIndexId);
        }
    }

    ImageArchitecture m_architecture = ImageArchitecture::None;

    std::map<std::string, DWORD> m_visitedUdts;
    std::vector<DWORD> m_sortedSymbolIndexes;
};
