#pragma once
#include "PDB.h"
#include "PDBSymbolVisitorBase.h"
#include "PDBReconstructorBase.h"

#include <memory>
#include <stack>

template <typename MEMBER_DEFINITION_TYPE>
class PDBSymbolVisitor : public PDBSymbolVisitorBase
{
public:
    PDBSymbolVisitor(PDBReconstructorBase* reconstructVisitor);

    void Run(const Symbol& symbol);
protected:

    void Visit(const Symbol& symbol) override;
    void VisitBaseType(const Symbol& symbol) override;
    void VisitEnumType(const Symbol& symbol) override;
    void VisitTypedefType(const Symbol& symbol) override;
    void VisitPointerType(const Symbol& symbol) override;
    void VisitArrayType(const Symbol& symbol) override;
    void VisitFunctionType(const Symbol& symbol) override;
    void VisitFunctionArgType(const Symbol& symbol) override;
    void VisitUdt(const Symbol& symbol) override;
    void VisitOtherType(const Symbol& symbol) override;
    void VisitEnumField(const SymbolEnumField& enumField) override;
    void VisitUdtField(const SymbolUdtField& udtField) override;
    void VisitUdtFieldEnd(const SymbolUdtField& udtField) override;
    void VisitUdtFieldBitFieldEnd(const SymbolUdtField& udtField) override;

private:

    struct AnonymousUdt
    {
        AnonymousUdt(UdtKind kind,
                     const SymbolUdtField* first, const SymbolUdtField* last,
                     DWORD size = 0,
                     DWORD memberCount = 0
        )
        {
            this->kind = kind;
            this->first = first;
            this->last = last;
            this->size = size;
            this->memberCount = memberCount;
        }

        const SymbolUdtField* first;
        const SymbolUdtField* last;
        DWORD size;
        DWORD memberCount;
        UdtKind kind;
    };

    struct BitFieldRange
    {
        const SymbolUdtField* first;
        const SymbolUdtField* last;

        BitFieldRange() : first(nullptr), last(nullptr)
        {
        }

        void Clear()
        {
            first = nullptr;
            last = nullptr;
        }

        bool HasValue() const
        {
            return /*First != nullptr &&*/
                last != nullptr;
        }
    };

    struct UdtFieldContext
    {
        UdtFieldContext(const SymbolUdtField* udtField, BOOL respectBitFields = TRUE)
        {
            this->udtField = udtField;

            previousUdtField = &udtField[-1];
            currentUdtField = &udtField[0];
            nextUdtField = std::get<SymbolUdt>(udtField->parent->variant).FindFieldNext(udtField);

            this->respectBitFields = respectBitFields;

            if (respectBitFields)
            {
                nextUdtField = GetNextUdtFieldWithRespectToBitFields(udtField);
            }
        }

        bool IsFirst() const { return previousUdtField < std::get<SymbolUdt>(udtField->parent->variant).FieldFirst(); }
        bool IsLast() const { return nextUdtField == std::get<SymbolUdt>(udtField->parent->variant).FieldLast(); }

        bool GetNext()
        {
            previousUdtField = currentUdtField;
            currentUdtField = nextUdtField;
            nextUdtField = std::get<SymbolUdt>(udtField->parent->variant).FindFieldNext(currentUdtField);

            if (respectBitFields && IsLast() == false)
            {
                nextUdtField = GetNextUdtFieldWithRespectToBitFields(currentUdtField);
            }
            return IsLast() == false;
        }

        const SymbolUdtField* udtField;

        const SymbolUdtField* previousUdtField;
        const SymbolUdtField* currentUdtField;
        const SymbolUdtField* nextUdtField;

        BOOL respectBitFields;
    };

    using AnonymousUdtStack = std::stack<std::shared_ptr<AnonymousUdt>>;
    using ContextStack = std::stack<std::shared_ptr<UdtFieldDefinitionBase>>;

private:
    void CheckForDataFieldPadding(const SymbolUdtField* udtField);
    void CheckForBitFieldFieldPadding(const SymbolUdtField* udtField);
    void CheckForAnonymousUnion(const SymbolUdtField* udtField);
    void CheckForAnonymousStruct(const SymbolUdtField* udtField);
    void CheckForEndOfAnonymousUdt(const SymbolUdtField* udtField);

    std::shared_ptr<UdtFieldDefinitionBase> MemberDefinitionFactory();

    void PushAnonymousUdt(std::shared_ptr<AnonymousUdt> item);
    void PopAnonymousUdt();

private:
    static const SymbolUdtField* GetNextUdtFieldWithRespectToBitFields(const SymbolUdtField* udtField);
    static bool Is64BitBasicType(const Symbol& symbol);

private:
    DWORD m_sizeOfPreviousUdtField = 0;
    const SymbolUdtField* m_previousUdtField = nullptr;
    const SymbolUdtField* m_previousBitFieldField = nullptr;
    AnonymousUdtStack m_anonymousUdtStack;
    AnonymousUdtStack m_anonymousUnionStack;
    AnonymousUdtStack m_anonymousStructStack;
    BitFieldRange m_currentBitField;
    ContextStack m_memberContextStack;
    PDBReconstructorBase* m_reconstructVisitor;
};

#include "PDBSymbolVisitor.inl"
