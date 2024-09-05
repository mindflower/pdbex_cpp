#pragma once
#include "PDB.h"
#include <cassert>

class PDBSymbolVisitorBase
{
public:
    virtual ~PDBSymbolVisitorBase() = default;

    virtual	void Visit(const Symbol& symbol)
    {
        switch (symbol.tag)
        {
        case SymTagBaseType:
            VisitBaseType(symbol);
            break;

        case SymTagEnum:
            VisitEnumType(symbol);
            break;

        case SymTagTypedef:
            VisitTypedefType(symbol);
            break;

        case SymTagPointerType:
            VisitPointerType(symbol);
            break;

        case SymTagArrayType:
            VisitArrayType(symbol);
            break;

        case SymTagFunction:
        case SymTagFunctionType:
            VisitFunctionType(symbol);
            break;

        case SymTagFunctionArgType:
            VisitFunctionArgType(symbol);
            break;

        case SymTagUDT:
            VisitUdt(symbol);
            break;

        default:
            VisitOtherType(symbol);
            break;
        }
    }

protected:
    virtual	void VisitBaseType(const Symbol& symbol)
    {
    }

    virtual	void VisitEnumType(const Symbol& symbol)
    {
        const auto& symbolEnums = std::get<SymbolEnum>(symbol.variant);
        for (const auto& symbolEnum : symbolEnums.fields)
        {
            VisitEnumField(symbolEnum);
        }
    }

    virtual	void VisitTypedefType(const Symbol& symbol)
    {
        const auto& symbolTypedef = std::get<SymbolTypedef>(symbol.variant);

        assert(symbolTypedef.type);
        Visit(*symbolTypedef.type);
    }

    virtual void VisitPointerType(const Symbol& symbol)
    {
        const auto& symbolPointer = std::get<SymbolPointer>(symbol.variant);

        assert(symbolPointer.type);
        Visit(*symbolPointer.type);
    }

    virtual void VisitArrayType(const Symbol& symbol)
    {
        const auto& symbolArray = std::get<SymbolArray>(symbol.variant);

        assert(symbolArray.elementType);
        Visit(*symbolArray.elementType);
    }

    virtual void VisitFunctionType(const Symbol& symbol)
    {
        const auto& symbolFunction = std::get<SymbolFunction>(symbol.variant);
        for (const auto& argument : symbolFunction.arguments)
        {
            assert(argument);
            Visit(*argument);
        }

        if (symbolFunction.returnType)
        {
            Visit(*symbolFunction.returnType);
        }
    }

    virtual void VisitFunctionArgType(const Symbol& symbol)
    {
        const auto& symbolFunctionArg = std::get<SymbolFunctionArg>(symbol.variant);

        assert(symbolFunctionArg.type);
        Visit(*symbolFunctionArg.type);
    }

    virtual void VisitUdt(const Symbol& symbol)
    {
        const auto& symbolUdt = std::get<SymbolUdt>(symbol.variant);

        const auto endOfUdtFieldIt = symbolUdt.fields.end();
        for (auto udtFieldIt = symbolUdt.fields.begin(); udtFieldIt != endOfUdtFieldIt; ++udtFieldIt)
        {
            if (udtFieldIt->bits == 0)
            {
                VisitUdtFieldBegin(*udtFieldIt);
                VisitUdtField(*udtFieldIt);
                VisitUdtFieldEnd(*udtFieldIt);
            }
            else
            {
                VisitUdtFieldBitFieldBegin(*udtFieldIt);

                do
                {
                    VisitUdtFieldBitField(*udtFieldIt);
                } while (++udtFieldIt != endOfUdtFieldIt && udtFieldIt->bitPosition != 0);

                VisitUdtFieldBitFieldEnd(*(--udtFieldIt));
            }
        };
    }

    virtual void VisitOtherType(const Symbol& symbol) {}

    virtual	void VisitEnumField(const SymbolEnumField& enumField) {}

    virtual void VisitUdtFieldBegin(const SymbolUdtField& udtField) {}
    virtual void VisitUdtFieldEnd(const SymbolUdtField& udtField) {}

    virtual void VisitUdtField(const SymbolUdtField& udtField) {}

    virtual	void VisitUdtFieldBitFieldBegin(const SymbolUdtField& udtField) {}
    virtual	void VisitUdtFieldBitFieldEnd(const SymbolUdtField& udtField) {}

    virtual	void VisitUdtFieldBitField(const SymbolUdtField& udtField)
    {
        VisitUdtField(udtField);
    }
};
