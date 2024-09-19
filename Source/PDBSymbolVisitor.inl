#include "PDBSymbolVisitor.h"
#include "PDB.h"
#include "PDBSymbolVisitorBase.h"
#include "PDBReconstructorBase.h"

#include <memory>
#include <stack>

template <typename MEMBER_DEFINITION_TYPE>
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::PDBSymbolVisitor(PDBReconstructorBase* ReconstructVisitor) :
    m_reconstructVisitor(ReconstructVisitor)
{
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::Run(const Symbol& symbol)
{
    Visit(symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::Visit(const Symbol& symbol)
{
    PDBSymbolVisitorBase::Visit(symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitBaseType(const Symbol& symbol)
{
    m_memberContextStack.top()->VisitBaseType(symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitEnumType(const Symbol& symbol)
{
    if (m_memberContextStack.size())
    {
        m_memberContextStack.top()->VisitEnumType(symbol);
    }
    else
        if (m_reconstructVisitor->OnEnumType(symbol))
        {
            m_reconstructVisitor->OnEnumTypeBegin(symbol);
            PDBSymbolVisitorBase::VisitEnumType(symbol);
            m_reconstructVisitor->OnEnumTypeEnd(symbol);
        }
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitTypedefType(const Symbol& symbol)
{
    m_memberContextStack.top()->VisitTypedefTypeBegin(symbol);
    PDBSymbolVisitorBase::VisitTypedefType(symbol);
    m_memberContextStack.top()->VisitTypedefTypeEnd(symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitPointerType(const Symbol& symbol)
{
    m_memberContextStack.top()->VisitPointerTypeBegin(symbol);
    PDBSymbolVisitorBase::VisitPointerType(symbol);
    m_memberContextStack.top()->VisitPointerTypeEnd(symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitArrayType(const Symbol& symbol)
{
    m_memberContextStack.top()->VisitArrayTypeBegin(symbol);
    PDBSymbolVisitorBase::VisitArrayType(symbol);
    m_memberContextStack.top()->VisitArrayTypeEnd(symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitFunctionType(const Symbol& symbol)
{
    m_memberContextStack.top()->VisitFunctionTypeBegin(symbol);
    PDBSymbolVisitorBase::VisitFunctionType(symbol);
    m_memberContextStack.top()->VisitFunctionTypeEnd(symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitFunctionArgType(const Symbol& symbol)
{
    m_memberContextStack.top()->VisitFunctionArgTypeBegin(symbol);
    PDBSymbolVisitorBase::VisitFunctionArgType(symbol);
    m_memberContextStack.top()->VisitFunctionArgTypeEnd(symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUdt(const Symbol& symbol)
{
    if (!PDB::IsUnnamedSymbol(symbol) && m_memberContextStack.size())
    {
        m_memberContextStack.top()->VisitUdtType(symbol);
    }
    else if (m_reconstructVisitor->OnUdt(symbol))
    {
        if (symbol.size > 0)
        {
            AnonymousUdtStack AnonymousUDTStackBackup;
            AnonymousUdtStack AnonymousUnionStackBackup;
            AnonymousUdtStack AnonymousStructStackBackup;
            m_anonymousUdtStack.swap(AnonymousUDTStackBackup);
            m_anonymousUnionStack.swap(AnonymousUnionStackBackup);
            m_anonymousStructStack.swap(AnonymousStructStackBackup);
            {
                m_memberContextStack.push(MemberDefinitionFactory());

                m_reconstructVisitor->OnUdtBegin(symbol);
                PDBSymbolVisitorBase::VisitUdt(symbol);
                m_reconstructVisitor->OnUdtEnd(symbol);

                m_memberContextStack.pop();
            }
            m_anonymousStructStack.swap(AnonymousStructStackBackup);
            m_anonymousUnionStack.swap(AnonymousUnionStackBackup);
            m_anonymousUdtStack.swap(AnonymousUDTStackBackup);
        }
    }
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitOtherType(const Symbol& symbol)
{

}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitEnumField(const SymbolEnumField& EnumField)
{
    m_reconstructVisitor->OnEnumField(EnumField);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUdtField(const SymbolUdtField& udtField)
{
    static const std::set<std::string> IgnoredNames = {"__local_vftable_ctor_closure", "__vecDelDtor"};

    if (udtField.type->tag == SymTagVTable ||
        udtField.isBaseClass == true ||
        IgnoredNames.contains(udtField.name))
    {
        return;
    }

    BOOL IsBitFieldMember = udtField.bits != 0;
    BOOL IsFirstBitFieldMember = IsBitFieldMember && !m_previousBitFieldField;

    m_memberContextStack.push(MemberDefinitionFactory());
    m_memberContextStack.top()->SetMemberName(udtField.name);

    if (!IsBitFieldMember || IsFirstBitFieldMember)
    {
        CheckForDataFieldPadding(&udtField);
        CheckForAnonymousUnion(&udtField);
        CheckForAnonymousStruct(&udtField);
    }

    if (IsFirstBitFieldMember)
    {
        BOOL IsFirstBitFieldMemberPadding = udtField.bitPosition != 0;

        assert(m_currentBitField.HasValue() == false);

        m_currentBitField.first = IsFirstBitFieldMemberPadding ? nullptr : &udtField;
        m_currentBitField.last = GetNextUdtFieldWithRespectToBitFields(&udtField) - 1;

        m_reconstructVisitor->OnUdtFieldBitFieldBegin(*m_currentBitField.first, *m_currentBitField.last);
    }

    if (IsBitFieldMember)
    {
        CheckForBitFieldFieldPadding(&udtField);
    }

    m_reconstructVisitor->OnUdtFieldBegin(udtField);
    Visit(*udtField.type);
    m_reconstructVisitor->OnUdtField(udtField, *m_memberContextStack.top().get());
    m_reconstructVisitor->OnUdtFieldEnd(udtField);

    m_memberContextStack.pop();

    if (IsBitFieldMember)
    {
        m_previousBitFieldField = &udtField;
    }
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUdtFieldEnd(const SymbolUdtField& udtField)
{
    CheckForEndOfAnonymousUdt(&udtField);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUdtFieldBitFieldEnd(const SymbolUdtField& udtField)
{
    assert(m_currentBitField.HasValue() == true);
    //assert(m_currentBitField.last == udtField);

    m_reconstructVisitor->OnUdtFieldBitFieldEnd(*m_currentBitField.first, *m_currentBitField.last);

    m_currentBitField.Clear();

    VisitUdtFieldEnd(udtField);

    m_previousBitFieldField = nullptr;
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitFunctionArg(const SymbolFunctionArg& functionArg)
{
    m_memberContextStack.top()->SetMemberName(functionArg.name);
    assert(functionArg.type);
    Visit(*functionArg.type);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForDataFieldPadding(const SymbolUdtField* udtField)
{
    UdtFieldContext UdtFieldCtx(udtField);
    DWORD PreviousUdtFieldOffset = 0;
    DWORD SizeOfPreviousUdtField = 0;
    DWORD BaseClassOffset = 0;
    bool IsPreviousTypedef = false;

    const auto& BaseClassFields = std::get<SymbolUdt>(udtField->parent->variant).baseClassFields;
    for (const auto& Field : BaseClassFields)
    {
        BaseClassOffset += Field.type->size;
    }

    if (UdtFieldCtx.IsFirst() == false)
    {
        PreviousUdtFieldOffset = m_previousUdtField->offset;
        SizeOfPreviousUdtField = m_sizeOfPreviousUdtField;
        IsPreviousTypedef = m_previousUdtField->tag == SymTagTypedef;
    }

    if (!IsPreviousTypedef &&
        BaseClassOffset < udtField->offset &&
        PreviousUdtFieldOffset + SizeOfPreviousUdtField < udtField->offset)
    {
        DWORD Difference = udtField->offset - (PreviousUdtFieldOffset + SizeOfPreviousUdtField);

        m_reconstructVisitor->OnPaddingMember(
            *udtField,
            btChar,
            1,
            Difference
        );
    }
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForBitFieldFieldPadding(const SymbolUdtField* udtField)
{
    BOOL WasPreviousBitFieldMember = m_previousBitFieldField ? m_previousBitFieldField->bits != 0 : FALSE;

    if (
        (udtField->bitPosition != 0 && !WasPreviousBitFieldMember) ||
        (WasPreviousBitFieldMember &&
         udtField->bitPosition != m_previousBitFieldField->bitPosition + m_previousBitFieldField->bits)
        )
    {
        m_reconstructVisitor->OnPaddingBitFieldField(*udtField, m_previousBitFieldField);
    }
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForAnonymousUnion(const SymbolUdtField* udtField)
{
    UdtFieldContext UdtFieldCtx(udtField);
    if (UdtFieldCtx.IsLast())
        return;

    if (!m_anonymousUdtStack.empty() &&
        m_anonymousUdtStack.top()->kind == UdtUnion)
    {
        return;
    }

    do {
        if (UdtFieldCtx.nextUdtField->offset == udtField->offset
            && UdtFieldCtx.nextUdtField->tag == SymTagData
            && udtField->tag == SymTagData
            && UdtFieldCtx.nextUdtField->dataKind != DataIsStaticMember
            && udtField->dataKind != DataIsStaticMember
            )
        {
            if (m_anonymousStructStack.empty() ||
                (!m_anonymousStructStack.empty() && UdtFieldCtx.nextUdtField <= m_anonymousStructStack.top()->last))
            {
                PushAnonymousUdt(std::make_shared<AnonymousUdt>(UdtUnion, udtField, nullptr, udtField->type->size));
                m_reconstructVisitor->OnAnonymousUdtBegin(UdtUnion, *udtField);
                break;
            }
        }
    } while (UdtFieldCtx.GetNext());
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForAnonymousStruct(const SymbolUdtField* udtField)
{
    UdtFieldContext UdtFieldCtx(udtField);
    if (UdtFieldCtx.IsLast())
        return;

    if (!m_anonymousUdtStack.empty() &&
        m_anonymousUdtStack.top()->kind != UdtUnion)
    {
        return;
    }

    if (UdtFieldCtx.nextUdtField->offset <= udtField->offset)
        return;

    do {
        if ((UdtFieldCtx.nextUdtField->offset == udtField->offset ||
             (!m_anonymousUdtStack.empty() &&
              UdtFieldCtx.nextUdtField->offset < m_anonymousUdtStack.top()->first->offset + m_anonymousUdtStack.top()->size
              ))
            && UdtFieldCtx.nextUdtField->tag == SymTagData
            && udtField->tag == SymTagData
            && UdtFieldCtx.nextUdtField->dataKind != DataIsStaticMember)
        {
            do {
                bool IsEndOfAnonymousStruct =
                    UdtFieldCtx.IsLast() ||
                    UdtFieldCtx.nextUdtField->offset <= udtField->offset;

                if (IsEndOfAnonymousStruct)
                    break;
            } while (UdtFieldCtx.GetNext());

            PushAnonymousUdt(std::make_shared<AnonymousUdt>(UdtStruct, udtField, UdtFieldCtx.currentUdtField));
            m_reconstructVisitor->OnAnonymousUdtBegin(UdtStruct, *udtField);
            break;
        }
    } while (UdtFieldCtx.GetNext());
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForEndOfAnonymousUdt(const SymbolUdtField* udtField)
{
    m_previousUdtField = ((udtField->tag == SymTagData
                           || udtField->tag == SymTagBaseClass
                           || udtField->tag == SymTagTypedef
                           ) && udtField->dataKind != DataIsStaticMember)
        ? udtField : m_previousUdtField;
    m_sizeOfPreviousUdtField = ((udtField->tag == SymTagData
                                 || udtField->tag == SymTagBaseClass
                                 || udtField->tag == SymTagTypedef
                                 ) && udtField->dataKind != DataIsStaticMember)
        ? udtField->type->size : m_sizeOfPreviousUdtField;

    if (m_anonymousUdtStack.empty())
        return;

    UdtFieldContext UdtFieldCtx(udtField, FALSE);

    AnonymousUdt* LastAnonymousUdt;

    do {
        LastAnonymousUdt = m_anonymousUdtStack.top().get();
        LastAnonymousUdt->memberCount += 1;

        bool IsEndOfAnonymousUdt = false;

        if (LastAnonymousUdt->kind == UdtUnion)
        {
            LastAnonymousUdt->size = max(LastAnonymousUdt->size, m_sizeOfPreviousUdtField);

            IsEndOfAnonymousUdt =
                UdtFieldCtx.IsLast() ||
                UdtFieldCtx.nextUdtField->tag != SymTagData ||
                UdtFieldCtx.nextUdtField->offset < udtField->offset ||
                (UdtFieldCtx.nextUdtField->offset == udtField->offset + LastAnonymousUdt->size) ||
                (UdtFieldCtx.nextUdtField->offset == udtField->offset + 8 && Is64BitBasicType(*UdtFieldCtx.nextUdtField->type)) ||
                (UdtFieldCtx.nextUdtField->offset > udtField->offset && udtField->bits != 0) ||
                (UdtFieldCtx.nextUdtField->offset > udtField->offset && udtField->offset + udtField->type->size != UdtFieldCtx.nextUdtField->offset);
        }
        else
        {
            LastAnonymousUdt->size += m_sizeOfPreviousUdtField;

            IsEndOfAnonymousUdt =
                UdtFieldCtx.IsLast() ||
                UdtFieldCtx.nextUdtField->offset <= udtField->offset;

            AnonymousUdt* LastAnonymousUnion =
                m_anonymousUnionStack.empty() ? nullptr : m_anonymousUnionStack.top().get();

            IsEndOfAnonymousUdt = IsEndOfAnonymousUdt || (
                LastAnonymousUnion != nullptr &&
                (LastAnonymousUnion->first->offset + LastAnonymousUnion->size == udtField->offset + udtField->type->size ||
                 LastAnonymousUnion->first->offset + LastAnonymousUnion->size == UdtFieldCtx.nextUdtField->offset) &&
                LastAnonymousUdt->memberCount >= 2
                );
        }

        if (IsEndOfAnonymousUdt)
        {
            m_sizeOfPreviousUdtField = LastAnonymousUdt->size;
            LastAnonymousUdt->last = udtField;

            m_reconstructVisitor->OnAnonymousUdtEnd(
                LastAnonymousUdt->kind,
                *LastAnonymousUdt->first, *LastAnonymousUdt->last, LastAnonymousUdt->size);

            PopAnonymousUdt();

            LastAnonymousUdt = nullptr;
        }

        if (!m_anonymousUdtStack.empty())
        {
            if (m_anonymousUdtStack.top()->kind == UdtUnion)
            {
                udtField = m_anonymousUdtStack.top()->first;
                m_previousUdtField = udtField;
            }
            else
            {
                udtField = UdtFieldCtx.currentUdtField;
                m_previousUdtField = udtField;
            }
        }
    } while (LastAnonymousUdt == nullptr && !m_anonymousUdtStack.empty());
}

template <typename MEMBER_DEFINITION_TYPE>
std::shared_ptr<UdtFieldDefinitionBase> PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::MemberDefinitionFactory()
{
    auto MemberDefinition = std::make_shared<MEMBER_DEFINITION_TYPE>();
    return MemberDefinition;
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::PushAnonymousUdt(std::shared_ptr<AnonymousUdt> item)
{
    m_anonymousUdtStack.push(item);
    if (item->kind == UdtUnion)
        m_anonymousUnionStack.push(item);
    else	m_anonymousStructStack.push(item);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::PopAnonymousUdt()
{
    if (m_anonymousUdtStack.top()->kind == UdtUnion)
        m_anonymousUnionStack.pop();
    else	m_anonymousStructStack.pop();
    m_anonymousUdtStack.pop();
}

template <typename MEMBER_DEFINITION_TYPE>
const SymbolUdtField*
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::GetNextUdtFieldWithRespectToBitFields(const SymbolUdtField* udtField)
{
    const SymbolUdtField* nextUdtField = udtField;

    while ((nextUdtField = std::get<SymbolUdt>(udtField->parent->variant).FindFieldNext(nextUdtField))
           < std::get<SymbolUdt>(udtField->parent->variant).FieldLast())
    {
        if (nextUdtField->bitPosition == 0)
        {
            break;
        }
    }
    return nextUdtField;
}

template <typename MEMBER_DEFINITION_TYPE>
bool PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::Is64BitBasicType(const Symbol& symbol)
{
    return (symbol.tag == SymTagBaseType && symbol.size == 8);
}

template<typename MEMBER_DEFINITION_TYPE>
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::AnonymousUdt::AnonymousUdt(UdtKind kind, const SymbolUdtField* first, const SymbolUdtField* last, DWORD size, DWORD memberCount)
{
    this->kind = kind;
    this->first = first;
    this->last = last;
    this->size = size;
    this->memberCount = memberCount;
}

template<typename MEMBER_DEFINITION_TYPE>
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::BitFieldRange::BitFieldRange() : first(nullptr), last(nullptr)
{
}

template<typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::BitFieldRange::Clear()
{
    first = nullptr;
    last = nullptr;
}

template<typename MEMBER_DEFINITION_TYPE>
bool PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::BitFieldRange::HasValue() const
{
    return /*First != nullptr &&*/
        last != nullptr;
}

template<typename MEMBER_DEFINITION_TYPE>
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::UdtFieldContext::UdtFieldContext(const SymbolUdtField* udtField, BOOL respectBitFields)
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

template<typename MEMBER_DEFINITION_TYPE>
bool PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::UdtFieldContext::IsFirst() const
{
    return previousUdtField < std::get<SymbolUdt>(udtField->parent->variant).FieldFirst();
}

template<typename MEMBER_DEFINITION_TYPE>
bool PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::UdtFieldContext::IsLast() const
{
    return nextUdtField == std::get<SymbolUdt>(udtField->parent->variant).FieldLast();
}

template<typename MEMBER_DEFINITION_TYPE>
bool PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::UdtFieldContext::GetNext()
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
