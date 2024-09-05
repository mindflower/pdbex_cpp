#pragma once
#include "PDB.h"

class UdtFieldDefinitionBase
{
public:
    virtual ~UdtFieldDefinitionBase() = default;

    virtual	void VisitBaseType(const Symbol& symbol) {}

    virtual void VisitTypedefTypeBegin(const Symbol& symbol) {}
    virtual	void VisitTypedefTypeEnd(const Symbol& symbol) {}

    virtual	void VisitEnumType(const Symbol& symbol) {}
    virtual	void VisitUdtType(const Symbol& symbol) {}

    virtual void VisitPointerTypeBegin(const Symbol& symbol) {}
    virtual	void VisitPointerTypeEnd(const Symbol& symbol) {}

    virtual	void VisitArrayTypeBegin(const Symbol& symbol) {}
    virtual	void VisitArrayTypeEnd(const Symbol& symbol) {}

    virtual	void VisitFunctionTypeBegin(const Symbol& symbol) {}
    virtual	void VisitFunctionTypeEnd(const Symbol& symbol) {}

    virtual	void VisitFunctionArgTypeBegin(const Symbol& symbol) {}
    virtual void VisitFunctionArgTypeEnd(const Symbol& symbol) {}

    virtual	void SetMemberName(const std::string& memberName) {}

    virtual	std::string GetPrintableDefinition() const { return {}; }
};
