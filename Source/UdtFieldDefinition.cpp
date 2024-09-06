#include "UdtFieldDefinition.h"
#include <cassert>

void UdtFieldDefinition::VisitBaseType(const Symbol& symbol)
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

void UdtFieldDefinition::VisitTypedefTypeEnd(const Symbol& symbol)
{
    m_typeSuffix = " = " + m_typePrefix;
    m_typePrefix = "using";
}

void UdtFieldDefinition::VisitEnumType(const Symbol& symbol)
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

void UdtFieldDefinition::VisitUdtType(const Symbol& symbol)
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

void UdtFieldDefinition::VisitPointerTypeEnd(const Symbol& symbol)
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

void UdtFieldDefinition::VisitArrayTypeEnd(const Symbol& symbol)
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

void UdtFieldDefinition::VisitFunctionTypeBegin(const Symbol& symbol)
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

void UdtFieldDefinition::VisitFunctionTypeEnd(const Symbol& symbol)
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

void UdtFieldDefinition::VisitFunctionArgTypeBegin(const Symbol& symbol)
{
    //Function func = m_funcs.top();
    //TODO
}

void UdtFieldDefinition::VisitFunctionArgTypeEnd(const Symbol& symbol)
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

void UdtFieldDefinition::SetMemberName(const std::string& memberName)
{
    m_memberName = memberName;
}

std::string UdtFieldDefinition::GetPrintableDefinition() const
{
    std::string res;
    if (!m_typePrefix.empty())
    {
        res += m_typePrefix + " ";
    }
    return res + m_memberName + m_typeSuffix + m_comment;
}
