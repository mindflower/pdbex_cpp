#pragma once
#include "PDBSymbolSorterBase.h"
#include "PDBHeaderReconstructor.h"
#include "PDBSymbolVisitor.h"
#include "UdtFieldDefinition.h"

class PDBExtractor
{
public:
    struct Settings
    {
        PDBHeaderReconstructor::Settings pdbHeaderReconstructorSettings;

        std::filesystem::path pdbPath;
        std::filesystem::path outputFilename;
    };

    int Run(int argc, char** argv);

private:
    void PrintUsage();
    void ParseParameters(int argc, char** argv);
    void OpenPDBFile();
    void PrintPDBDefinitions();
    void PrintPDBFunctions();
    void DumpAllSymbols();

private:
    PDB m_pdb;
    Settings m_settings;

    std::unique_ptr<PDBSymbolSorterBase> m_symbolSorter;
    std::unique_ptr<PDBHeaderReconstructor> m_headerReconstructor;
    std::unique_ptr<PDBSymbolVisitor<UdtFieldDefinition>> m_symbolVisitor;
};
