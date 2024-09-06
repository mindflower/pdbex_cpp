#include "PDBHeaderReconstructor.h"
#include "PDBReconstructorBase.h"

#include <iostream>
#include <numeric> // std::accumulate
#include <string>
#include <map>
#include <set>

#include <cassert>

PDBHeaderReconstructor::PDBHeaderReconstructor(Settings& visitorSettings)
	: m_settings(visitorSettings)
{
}

void PDBHeaderReconstructor::Clear()
{
	assert(m_depth == 0);

	m_anonymousDataTypeCounter = 0;
	m_paddingMemberCounter = 0;

	m_correctedSymbolNames.clear();
	m_visitedSymbols.clear();
}

const std::string& PDBHeaderReconstructor::GetCorrectedSymbolName(const Symbol& symbol) const
{
	auto correctedNameIt = m_correctedSymbolNames.find(symbol.symIndexId);
 	if (correctedNameIt == m_correctedSymbolNames.end())
	{
		std::string correctedName = m_settings.symbolPrefix;

		if (PDB::IsUnnamedSymbol(symbol))
		{
			//
		} 
		else
		{
			correctedName += symbol.name;
		}

		correctedName += m_settings.symbolSuffix;
		m_correctedSymbolNames[symbol.symIndexId] = correctedName;
	}
	return m_correctedSymbolNames[symbol.symIndexId];
}

bool PDBHeaderReconstructor::OnEnumType(const Symbol& symbol)
{
	const auto correctedName = GetCorrectedSymbolName(symbol);
	const bool expand = ShouldExpand(symbol);

	MarkAsVisited(symbol);

	if (!expand)
	{
		Write("enum %s", correctedName.c_str());
	}

	return expand;
}

void PDBHeaderReconstructor::OnEnumTypeBegin(const Symbol& symbol)
{
	const auto correctedName = GetCorrectedSymbolName(symbol);

	Write("enum");

	Write(" %s", correctedName.c_str());

	Write("\n");

	WriteIndent();
	Write("{\n");

	m_depth += 1;
}

void PDBHeaderReconstructor::OnEnumTypeEnd(const Symbol& symbol)
{
	m_depth -= 1;

	WriteIndent();
	Write("}");

	if (m_depth == 0)
	{
		Write(";\n\n");
	}
}

void PDBHeaderReconstructor::OnEnumField(const SymbolEnumField& enumField)
{
	WriteIndent();
	Write("%s = ", enumField.name.c_str());

	WriteVariant(enumField.value);
	Write(",\n");
}

bool PDBHeaderReconstructor::OnUdt(const Symbol& symbol)
{
	const bool expand = ShouldExpand(symbol);

	MarkAsVisited(symbol);

	if (!expand)
	{
		const auto correctedName = GetCorrectedSymbolName(symbol);

		WriteConstAndVolatile(symbol);
		Write("%s %s", PDB::GetUdtKindString(std::get<SymbolUdt>(symbol.variant).kind).c_str(), correctedName.c_str());

		if (m_depth == 0)
		{
			Write(";\n\n");
		}
	}

	return expand;
}

void PDBHeaderReconstructor::OnUdtBegin(const Symbol& symbol)
{
	m_accessStack.push(-1);

	WriteConstAndVolatile(symbol);

	const auto& udt = std::get<SymbolUdt>(symbol.variant);
	Write("%s", PDB::GetUdtKindString(udt.kind).c_str());

	if (!PDB::IsUnnamedSymbol(symbol))
	{
		const auto correctedName = GetCorrectedSymbolName(symbol);
		Write(" %s", correctedName.c_str());

		if (!udt.baseClassFields.empty())
		{
			std::string className;
			for (const auto& baseClass : udt.baseClassFields)
			{
				std::string access;
				switch (baseClass.access)
				{
				case 1:
					access = "private ";
					break;

				case 2:
					access = "protected ";
					break;

				case 3:
					access = "public ";
					break;

				default:
					break;
				}
				std::string virtualClass;
				if (baseClass.isVirtual)
				{
					virtualClass = "virtual ";
				}

				assert(baseClass.type);
				std::string correctFuncName = GetCorrectedSymbolName(*baseClass.type);
				if (!className.empty())
				{
					className += ", ";
				}

				className += access + virtualClass + correctFuncName;
			}
			Write(" : %s", className.c_str());
		}
	}

	Write("\n");

	WriteIndent();
	Write("{\n");

	m_depth += 1;
}

void PDBHeaderReconstructor::OnUdtEnd(const Symbol& symbol)
{
	m_depth -= 1;
	m_accessStack.pop();

	WriteIndent();
	Write("}");

	if (m_depth == 0)
	{
		Write(";");
	}

	Write(" /* size: 0x%04x */", symbol.size);

	if (m_depth == 0)
	{
		Write("\n\n");
	}
}

void PDBHeaderReconstructor::OnUdtFieldBegin(const SymbolUdtField& udtField)
{
	assert(udtField.parent);
	if (std::get<SymbolUdt>(udtField.parent->variant).kind == UdtClass)
	{
		auto& prevAccess = m_accessStack.top();
		if (prevAccess != udtField.access)
		{
			std::string access;
			switch (udtField.access)
			{
			case 1: access = "private:\n"; break;
			case 2: access = "protected:\n"; break;
			case 3: access = "public:\n"; break;
			}

			if (prevAccess != -1)
			{
				Write("\n");
			}
			Write(access.c_str());
			prevAccess = udtField.access;
		}
	}

	WriteIndent();
	assert(udtField.type);
	if (udtField.dataKind != DataIsStaticMember &&
        udtField.type->tag != SymTagFunction &&
	    udtField.type->tag != SymTagTypedef &&
        (udtField.type->tag != SymTagEnum || udtField.tag != SymTagEnum) &&
	    (udtField.type->tag != SymTagUDT ||
        (udtField.tag != SymTagUDT && ShouldExpand(*udtField.type) == false)))
	{
		WriteOffset(udtField, GetParentOffset());
	}

	m_offsetStack.push_back(udtField.offset);
}

void PDBHeaderReconstructor::OnUdtFieldEnd(const SymbolUdtField& udtField)
{
	m_offsetStack.pop_back();
}

void PDBHeaderReconstructor::OnUdtField(const SymbolUdtField& udtField, UdtFieldDefinitionBase& memberDefinition)
{
	if (udtField.dataKind == DataIsStaticMember) //TODO
	{
		Write("static ");
	}

    if (udtField.tag == SymTagUDT && udtField.type->tag == SymTagUDT)
    {
        memberDefinition.SetMemberName("");
        Write(PDB::GetUdtKindString(std::get<SymbolUdt>(udtField.type->variant).kind).c_str());
        Write(" ");
    }

    if (udtField.tag == SymTagEnum && udtField.type->tag == SymTagEnum)
    {
        memberDefinition.SetMemberName("");
        Write("enum ");
    }

	Write("%s", memberDefinition.GetPrintableDefinition().c_str());

	if (udtField.bits != 0)
	{
		Write(" : %i", udtField.bits);
	}

	Write(";");

	if (udtField.bits != 0)
	{
		Write("   /* %i */", udtField.bitPosition);
	}

	Write("\n");
}

void PDBHeaderReconstructor::OnAnonymousUdtBegin(UdtKind kind, const SymbolUdtField& first)
{
	WriteIndent();
	Write("%s\n", PDB::GetUdtKindString(kind).c_str());

	WriteIndent();
	Write("{\n");

	m_depth += 1;
}

void PDBHeaderReconstructor::OnAnonymousUdtEnd(
	UdtKind kind,
	const SymbolUdtField& first,
	const SymbolUdtField& last,
	DWORD size)
{
	m_depth -= 1;
	WriteIndent();
	Write("}");

	WriteUnnamedDataType(kind);

	Write(";");
	Write(" /* size: 0x%04x */", size);
	Write("\n");
}

void PDBHeaderReconstructor::OnUdtFieldBitFieldBegin(const SymbolUdtField& first, const SymbolUdtField& last)
{
	if (m_settings.allowBitFieldsInUnion == false)
	{
		if (&first != &last)
		{
			WriteIndent();
			Write("%s /* bitfield */\n", PDB::GetUdtKindString(UdtStruct).c_str());

			WriteIndent();
			Write("{\n");

			m_depth += 1;
		}
	}
}

void PDBHeaderReconstructor::OnUdtFieldBitFieldEnd(const SymbolUdtField& first, const SymbolUdtField& last)
{
	if (m_settings.allowBitFieldsInUnion == false)
	{
		if (&first != &last)
		{
			m_depth -= 1;

			WriteIndent();
			Write("}; /* bitfield */\n");
		}
	}
}

void PDBHeaderReconstructor::OnPaddingMember(
	const SymbolUdtField& udtField,
	BasicType paddingBasicType,
	DWORD paddingBasicTypeSize,
	DWORD paddingSize)
{
	if (m_settings.createPaddingMembers)
	{
		WriteIndent();

		WriteOffset(udtField, -((int)paddingSize * (int)paddingBasicTypeSize));

		Write(
			"%s %s%u",
			PDB::GetBasicTypeString(paddingBasicType, paddingBasicTypeSize).c_str(),
			m_settings.paddingMemberPrefix.c_str(),
			m_paddingMemberCounter++
		);

		if (paddingSize > 1)
		{
			Write("[%u]", paddingSize);
		}

		Write(";\n");
	}
}

void PDBHeaderReconstructor::OnPaddingBitFieldField(
	const SymbolUdtField& udtField,
	const SymbolUdtField* previousUdtField)
{
	WriteIndent();

	WriteOffset(udtField, GetParentOffset());

	assert(udtField.type);
	if (m_settings.bitFieldPaddingMemberPrefix.empty())
	{
		Write("%s", PDB::GetBasicTypeString(*udtField.type).c_str());
	}
	else
	{
		Write("%s %s%u", PDB::GetBasicTypeString(*udtField.type).c_str(),
			m_settings.paddingMemberPrefix.c_str(), m_paddingMemberCounter++);
	}

	DWORD bits = previousUdtField
		? udtField.bitPosition - (previousUdtField->bitPosition + previousUdtField->bits)
		: udtField.bitPosition;

	DWORD bitPosition = previousUdtField
		? previousUdtField->bitPosition + previousUdtField->bits
		: 0;

	assert(bits != 0);

	Write(" : %i", bits);
	Write(";");
	Write("   /* %i */", bitPosition);
	Write("\n");
}

void PDBHeaderReconstructor::Write(const char* Format, ...)
{
	char TempBuffer[8 * 1024];

	va_list ArgPtr;
	va_start(ArgPtr, Format);
	vsprintf_s(TempBuffer, Format, ArgPtr);
	va_end(ArgPtr);

	m_settings.output.get().write(TempBuffer, strlen(TempBuffer));
}

void PDBHeaderReconstructor::WriteIndent()
{
	for (DWORD i = 0; i < m_depth; ++i)
	{
		Write("  ");
	}
}

void PDBHeaderReconstructor::WriteVariant(const VARIANT& v)
{
	switch (v.vt)
	{
	case VT_I1:
		Write("%d", (INT)v.cVal);
		break;

	case VT_UI1:
		Write("0x%x", (UINT)v.cVal);
		break;

	case VT_I2:
		Write("%d", (UINT)v.iVal);
		break;

	case VT_UI2:
		Write("0x%x", (UINT)v.iVal);
		break;

	case VT_INT:
		Write("%d", (UINT)v.lVal);
		break;

	case VT_UINT:
	case VT_UI4:
	case VT_I4:
		Write("0x%x", (UINT)v.lVal);
		break;
	}
}

void PDBHeaderReconstructor::WriteUnnamedDataType(UdtKind kind)
{
	if (m_settings.allowAnonymousDataTypes == false)
	{
		switch (kind)
		{
		case UdtStruct:
		case UdtClass:
			Write(" %s", m_settings.anonymousStructPrefix.c_str());
			break;
		case UdtUnion:
			Write(" %s", m_settings.anonymousUnionPrefix.c_str());
			break;
		default:
			assert(0);
			break;
		}

		if (m_anonymousDataTypeCounter++ > 0)
		{
			Write("%u", m_anonymousDataTypeCounter);
		}
	}
}

void PDBHeaderReconstructor::WriteConstAndVolatile(const Symbol& symbol)
{
	if (m_depth != 0)
	{
		if (symbol.isConst)
		{
			Write("const ");
		}

		if (symbol.isVolatile)
		{
			Write("volatile ");
		}
	}
}

void PDBHeaderReconstructor::WriteOffset(const SymbolUdtField& udtField, int paddingOffset)
{
	if (m_settings.showOffsets)
	{
		Write("/* 0x%04x */ ", udtField.offset + paddingOffset);
	}
}

bool PDBHeaderReconstructor::HasBeenVisited(const Symbol& symbol) const
{
	std::string correctedName = GetCorrectedSymbolName(symbol);
	return m_visitedSymbols.find(correctedName) != m_visitedSymbols.end();
}

void PDBHeaderReconstructor::MarkAsVisited(const Symbol& symbol)
{
	std::string correctedName = GetCorrectedSymbolName(symbol);
	m_visitedSymbols.insert(std::move(correctedName));
}

DWORD PDBHeaderReconstructor::GetParentOffset() const
{
	return std::accumulate(m_offsetStack.begin(), m_offsetStack.end(), (DWORD)0);
}

bool PDBHeaderReconstructor::ShouldExpand(const Symbol& symbol) const
{
	bool expand = false;

	switch (m_settings.memberStructExpansion)
	{
	default:
	case MemberStructExpansionType::None:
		expand = m_depth == 0;
		break;

	case MemberStructExpansionType::InlineUnnamed:
		expand = m_depth == 0 || (symbol.tag == SymTagUDT && PDB::IsUnnamedSymbol(symbol));
		break;

	case MemberStructExpansionType::InlineAll:
		expand = !HasBeenVisited(symbol);
		break;
	}

	return expand && symbol.size > 0;
}
