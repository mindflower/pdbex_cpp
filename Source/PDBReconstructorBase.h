#pragma once
#include "PDB.h"
#include "UdtFieldDefinitionBase.h"

class PDBReconstructorBase
{
public:
    virtual ~PDBReconstructorBase() = default;
    virtual	bool OnEnumType(const Symbol& symbol) { return false; }
    virtual void OnEnumTypeBegin(const Symbol& symbol) {}
    virtual void OnEnumTypeEnd(const Symbol& symbol) {}
    virtual void OnEnumField(const SymbolEnumField& enumField) {}

    virtual	bool OnUdt(const Symbol& symbol) { return false; }
    virtual	void OnUdtBegin(const Symbol& symbol) {}
    virtual	void OnUdtEnd(const Symbol& symbol) {}

    virtual	void OnUdtFieldBegin(const SymbolUdtField& udtField) {}
    virtual	void OnUdtFieldEnd(const SymbolUdtField& udtField) {}
    virtual void OnUdtField(const SymbolUdtField& udtField, UdtFieldDefinitionBase& memberDefinition) {}

    virtual void OnAnonymousUdtBegin(UdtKind kind, const SymbolUdtField& first) {}
    virtual	void OnAnonymousUdtEnd(UdtKind kind, const SymbolUdtField& first, const SymbolUdtField& last, DWORD size) {}

    virtual	void OnUdtFieldBitFieldBegin(const SymbolUdtField& first, const SymbolUdtField& last) {}
    virtual	void OnUdtFieldBitFieldEnd(const SymbolUdtField& first, const SymbolUdtField& last) {}

    virtual	void OnPaddingMember(const SymbolUdtField& udtField, BasicType PaddingBasicType, DWORD PaddingBasicTypeSize, DWORD PaddingSize) {}

    virtual void OnPaddingBitFieldField(const SymbolUdtField& udtField, const SymbolUdtField* previousUdtField) {}
};
