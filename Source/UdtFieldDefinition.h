#pragma once
#include "UdtFieldDefinitionBase.h"

#include <stack>
#include <vector>

class UdtFieldDefinition : public UdtFieldDefinitionBase
{
public:
    void VisitBaseType(const Symbol& symbol) override;
    void VisitTypedefTypeEnd(const Symbol& symbol) override;
    void VisitEnumType(const Symbol& symbol) override;
    void VisitUdtType(const Symbol& symbol) override;
    void VisitPointerTypeEnd(const Symbol& symbol) override;
    void VisitArrayTypeEnd(const Symbol& symbol) override;

    void VisitFunctionTypeBegin(const Symbol& symbol) override;
    void VisitFunctionTypeEnd(const Symbol& symbol) override;

    void VisitFunctionArgTypeBegin(const Symbol& symbol) override;
    void VisitFunctionArgTypeEnd(const Symbol& symbol) override;

    void SetMemberName(const std::string& memberName) override;

    std::string GetPrintableDefinition() const override;

private:
    struct Function
    {
        std::string name;
        std::vector<std::string> args;
    };

    std::string m_typePrefix;
    std::string m_memberName;
    std::string m_typeSuffix;
    std::string m_comment;
    std::stack<Function> m_funcs;
    std::vector<std::string> m_args;
};
