#pragma once
#include "PDBReconstructorBase.h"

#include <iostream>
#include <map>
#include <set>
#include <stack>

class PDBHeaderReconstructor : public PDBReconstructorBase
{
public:
    enum class MemberStructExpansionType
    {
        None,
        InlineUnnamed,
        InlineAll,
    };

    struct Settings
    {
        MemberStructExpansionType memberStructExpansion = MemberStructExpansionType::InlineUnnamed;
        std::unique_ptr<std::ostream> outputFile;
        std::reference_wrapper<std::ostream> output = std::cout;
        std::string paddingMemberPrefix = "Padding_";
        std::string bitFieldPaddingMemberPrefix;
        std::string unnamedTypePrefix;
        std::string symbolPrefix;
        std::string symbolSuffix;
        std::string anonymousStructPrefix = "s";
        std::string anonymousUnionPrefix = "u";
        bool createPaddingMembers = true;
        bool showOffsets = true;
        bool allowBitFieldsInUnion = false;
        bool allowAnonymousDataTypes = true;
    };

    PDBHeaderReconstructor(Settings& visitorSettings);
    void Clear();
    const std::string& GetCorrectedSymbolName(const Symbol& symbol) const;

protected:
    bool OnEnumType(const Symbol& symbol) override;
    void OnEnumTypeBegin(const Symbol& symbol) override;
    void OnEnumTypeEnd(const Symbol& symbol) override;
    void OnEnumField(const SymbolEnumField& enumField) override;

    bool OnUdt(const Symbol& symbol) override;
    void OnUdtBegin(const Symbol& symbol) override;
    void OnUdtEnd(const Symbol& symbol) override;

    void OnUdtFieldBegin(const SymbolUdtField& udtField) override;
    void OnUdtFieldEnd(const SymbolUdtField& udtField) override;
    void OnUdtField(const SymbolUdtField& udtField, UdtFieldDefinitionBase& memberDefinition) override;

    void OnAnonymousUdtBegin(UdtKind kind, const SymbolUdtField& first) override;
    void OnAnonymousUdtEnd(UdtKind kind, const SymbolUdtField& first, const SymbolUdtField& last, DWORD size) override;

    void OnUdtFieldBitFieldBegin(const SymbolUdtField& first, const SymbolUdtField& last) override;
    void OnUdtFieldBitFieldEnd(const SymbolUdtField& first, const SymbolUdtField& last) override;

    void OnPaddingMember(const SymbolUdtField& udtField, BasicType paddingBasicType, DWORD paddingBasicTypeSize, DWORD paddingSize) override;
    void OnPaddingBitFieldField(const SymbolUdtField& udtField, const SymbolUdtField* previousUdtField) override;

private:
    void Write(const char* format, ...);
    void WriteIndent();
    void WriteVariant(const VARIANT& v);
    void WriteUnnamedDataType(UdtKind kind);
    void WriteConstAndVolatile(const Symbol& symbol);
    void WriteOffset(const SymbolUdtField& udtField, int paddingOffset);
    bool HasBeenVisited(const Symbol& symbol) const;
    void MarkAsVisited(const Symbol& symbol);
    DWORD GetParentOffset() const;
    bool ShouldExpand(const Symbol& symbol) const;

private:
    Settings& m_settings;

    std::vector<DWORD> m_offsetStack;
    std::stack<DWORD> m_accessStack;
    DWORD m_depth = 0;
    DWORD m_anonymousDataTypeCounter = 0;
    DWORD m_paddingMemberCounter = 0;

    mutable std::map<DWORD, std::string> m_correctedSymbolNames;

    std::set<std::string> m_visitedSymbols;
};
