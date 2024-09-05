#pragma once
#include "UdtFieldDefinitionBase.h"

#include <stack>
#include <vector>

class UdtFieldDefinition : public UdtFieldDefinitionBase
{
public:
    void VisitBaseType(const Symbol& symbol) override
    {
        if (symbol.baseType == btFloat && symbol.size == 10)
        {
            m_comment += " /* 80-bit float */";
        }

        if (symbol.isConst)
        {
            m_typePrefix += "const ";
        }

        if (symbol.isVolatile)
        {
            m_typePrefix += "volatile ";
        }

        m_typePrefix += PDB::GetBasicTypeString(symbol);
    }

    void VisitTypedefTypeEnd(const Symbol& symbol) override
    {
        m_typeSuffix = " = " + m_typePrefix;
        m_typePrefix = "using";
    }

    void VisitEnumType(const Symbol& symbol) override
    {
        if (symbol.isConst)
        {
            m_typePrefix += "const ";
        }

        if (symbol.isVolatile)
        {
            m_typePrefix += "volatile ";
        }

        m_typePrefix += symbol.name;
    }

    void VisitUdtType(const Symbol& symbol) override
    {
        if (symbol.isConst)
        {
            m_typePrefix += "const ";
        }

        if (symbol.isVolatile)
        {
            m_typePrefix += "volatile ";
        }

        m_typePrefix += symbol.name;
    }

    void VisitPointerTypeEnd(const Symbol& symbol) override
    {
        const auto& symbolPointer = std::get<SymbolPointer>(symbol.variant);

        assert(symbolPointer.type);
        if (symbolPointer.type->tag == SymTagFunctionType)
        {
            if (symbolPointer.isReference)
            {
                m_memberName = "& " + m_memberName;
            }
            else
            {
                m_memberName = "* " + m_memberName;
            }

            if (symbol.isConst)
            {
                m_memberName += " const";
            }
            if (symbol.isVolatile)
            {
                m_memberName += " volatile";
            }

            m_memberName = "(" + m_memberName + ")";
            return;
        }

        if (symbolPointer.isReference)
        {
            m_typePrefix += "&";
        }
        else
        {
            m_typePrefix += "*";
        }

        if (symbol.isConst)
        {
            m_typePrefix += " const";
        }

        if (symbol.isVolatile)
        {
            m_typePrefix += " volatile";
        }
    }

    void VisitArrayTypeEnd(const Symbol& symbol) override
    {
        const auto& symbolArray = std::get<SymbolArray>(symbol.variant);
        if (symbolArray.elementCount == 0)
        {
            const_cast<Symbol&>(symbol).size = 1;	//TODO
            m_typeSuffix += "[]";
        }
        else
        {
            m_typeSuffix += "[" + std::to_string(symbolArray.elementCount) + "]";
        }
    }

    void VisitFunctionTypeBegin(const Symbol& symbol) override
    {
        if (m_funcs.size())
        {
            if (m_funcs.top().name == m_memberName)
            {
                m_memberName = "";
            }
        }

        m_funcs.push(Function{ m_memberName, m_args });
        m_memberName = "";
    }

    void VisitFunctionTypeEnd(const Symbol& symbol) override
    {
        const auto& symbolFunction = std::get<SymbolFunction>(symbol.variant);
        if (symbolFunction.isStatic)
        {
            m_typePrefix = "static " + m_typePrefix;
        }
        else if (symbolFunction.isVirtual)
        {
            m_typePrefix = "virtual " + m_typePrefix;
        }

        if (symbolFunction.isConst)
        {
            m_comment += " const";
        }

        if (symbolFunction.isOverride)
        {
            m_comment += " override";
        }

        if (symbolFunction.isPure)
        {
            m_comment += " = 0";
        }

        if (symbolFunction.isVirtual)
        {
            char hexbuf[16] = {};
            snprintf(hexbuf, sizeof(hexbuf), " 0x%02x ", symbolFunction.virtualOffset);
            m_comment += " /*" + std::string(hexbuf) + "*/";
        }

        if (m_typeSuffix.size())
        {
            m_typePrefix = GetPrintableDefinition();
        }

        m_typeSuffix = "";

        for (const auto& arg : m_args)
        {
            if (m_typeSuffix.size())
            {
                m_typeSuffix += ", ";
            }
            m_typeSuffix += arg;
        }
        m_typeSuffix = "(" + m_typeSuffix + ")";

        auto func = m_funcs.top();

        m_funcs.pop();

        m_args = std::move(func.args);
        m_memberName = std::move(func.name);
    }

    void VisitFunctionArgTypeBegin(const Symbol& symbol) override
    {
        //Function func = m_funcs.top();
        //TODO
    }

    void VisitFunctionArgTypeEnd(const Symbol& symbol) override
    {
        std::string argName;
        if (!symbol.name.empty())
        {
            argName = std::string(" ") + symbol.name;
        }

        if (m_memberName.find('(') == std::string::npos)
        {
            m_args.push_back(m_typePrefix + argName);
            m_typeSuffix = "";
            m_typePrefix = "";
        }
        else
        {
            m_typeSuffix = GetPrintableDefinition();
            m_args.push_back(m_typeSuffix + argName);
            m_typeSuffix = "";
            m_typePrefix = "";
        }

        auto func = m_funcs.top();

        m_memberName = std::move(func.name);
        //TODO
    }

    void SetMemberName(const std::string& memberName) override
    {
        m_memberName = memberName;
    }

    std::string GetPrintableDefinition() const override
    {
        std::string res;
        if (!m_typePrefix.empty())
        {
            res += m_typePrefix + " ";
        }
        return res + m_memberName + m_typeSuffix + m_comment;
    }

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
