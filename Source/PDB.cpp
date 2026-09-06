#include "PDB.h"
#include "PDBCallback.h"

#include <cassert>
#include <functional>

SymbolModuleBase::SymbolModuleBase()
{
    HRESULT hr = CoInitialize(nullptr);
    assert(hr == S_OK);
}

HRESULT SymbolModuleBase::LoadDiaViaCoCreateInstance()
{
    return CoCreateInstance(
        __uuidof(DiaSource),
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IDiaDataSource),
        (void**)&m_dataSource
    );
}

HRESULT SymbolModuleBase::LoadDiaViaLoadLibrary()
{
    HRESULT result = S_OK;
    HMODULE module = LoadLibrary(TEXT("msdia140.dll"));
    if (!module)
    {
        result = HRESULT_FROM_WIN32(GetLastError());
        return result;
    }

    using PDLLGETCLASSOBJECT_ROUTINE = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID);
    auto dllGetClassObject = reinterpret_cast<PDLLGETCLASSOBJECT_ROUTINE>(GetProcAddress(module, "DllGetClassObject"));
    if (!dllGetClassObject)
    {
        result = HRESULT_FROM_WIN32(GetLastError());
        return result;
    }

    IClassFactory* classFactory;
    result = dllGetClassObject(__uuidof(DiaSource), __uuidof(IClassFactory), &classFactory);
    if (FAILED(result))
    {
        return result;
    }

    return classFactory->CreateInstance(nullptr, __uuidof(IDiaDataSource), (void**)&m_dataSource);
}

bool SymbolModuleBase::Open(const std::filesystem::path& path)
{
    HRESULT result = S_OK;

    if (FAILED(result = LoadDiaViaCoCreateInstance()) &&
        FAILED(result = LoadDiaViaLoadLibrary()))
    {
        return false;
    }

    if (path.extension() == ".pdb")
    {
        result = m_dataSource->loadDataFromPdb(path.c_str());
    }
    else
    {
        PDBCallback callback;
        callback.AddRef();

        result = m_dataSource->loadDataForExe(path.c_str(), L"srv*.\\Symbols*https://msdl.microsoft.com/download/symbols", &callback);
    }

    if (FAILED(result))
    {
        Close();
        return false;
    }

    result = m_dataSource->openSession(&m_session);
    if (FAILED(result))
    {
        Close();
        return false;
    }

    result = m_session->get_globalScope(&m_globalSymbol);
    if (FAILED(result))
    {
        Close();
        return false;
    }

    return true;
}

void SymbolModuleBase::Close()
{
    m_globalSymbol.Release();
    m_session.Release();
    m_dataSource.Release();

    CoUninitialize();
}

bool SymbolModuleBase::IsOpen() const
{
    return m_dataSource && m_session && m_globalSymbol;
}

SymbolModule::~SymbolModule()
{
    Close();
}

bool SymbolModule::Open(const std::filesystem::path& path)
{
    if (!SymbolModuleBase::Open(path))
    {
        return false;
    }

    m_globalSymbol->get_machineType(&m_machineType);

    DWORD language = 0;
    m_globalSymbol->get_language(&language);
    m_language = static_cast<CV_CFL_LANG>(language);

    BuildSymbolMap();
    return true;
}

void SymbolModule::Close()
{
    SymbolModuleBase::Close();

    m_path.clear();
    m_symbolMap.clear();
    m_symbolNameMap.clear();
    m_symbolSet.clear();
}

const std::filesystem::path& SymbolModule::GetPath() const
{
    return m_path;
}

DWORD SymbolModule::GetMachineType() const
{
    return m_machineType;
}

CV_CFL_LANG SymbolModule::GetLanguage() const
{
    return m_language;
}

std::string SymbolModule::GetSymbolName(const DiaSymbolPtr& diaSymbol, bool raw)
{
    if (!diaSymbol)
    {
        return {};
    }

    BSTR symbolNameBstr;
    if (raw || (diaSymbol->get_undecoratedName(&symbolNameBstr) != S_OK))
    {
        if (diaSymbol->get_name(&symbolNameBstr) != S_OK)
        {
            return {};
        }
    }

    CHAR* symbolNameMb = nullptr;
    size_t symbolNameLength = 0;

    symbolNameLength = static_cast<size_t>(SysStringLen(symbolNameBstr) + 1);
    symbolNameMb = new CHAR[symbolNameLength];
    wcstombs(symbolNameMb, symbolNameBstr, symbolNameLength);

    std::string res = symbolNameMb;

    SysFreeString(symbolNameBstr);
    delete[] symbolNameMb;

    return res;
}

SymbolPtr SymbolModule::GetSymbolByName(const std::string& symbolName)
{
    auto it = m_symbolNameMap.find(symbolName);
    return it == m_symbolNameMap.end() ? nullptr : it->second;
}

SymbolPtr SymbolModule::GetSymbolBySymbolIndex(DWORD symIndex)
{
    auto it = m_symbolMap.find(symIndex);
    return it == m_symbolMap.end() ? nullptr : it->second;
}

SymbolPtr SymbolModule::GetSymbol(const DiaSymbolPtr& diaSymbol)
{
    if (!diaSymbol)
    {
        return {};
    }

    DWORD typeId = 0;
    diaSymbol->get_symIndexId(&typeId);

    auto it = m_symbolMap.find(typeId);
    if (it != m_symbolMap.end())
    {
        return it->second;
    }

    auto symbol = std::make_shared<Symbol>();
    m_symbolMap[typeId] = symbol;
    m_symbolSet.insert(symbol);

    InitSymbol(diaSymbol, symbol);

    if (!symbol->name.empty())
    {
        m_symbolNameMap[symbol->name] = symbol;
    }

    return symbol;
}

namespace
{
    void ForEachDiaSymbol(
        const DiaEnumSymbolsPtr& diaSymbolEnumerator,
        const std::function<void(const DiaSymbolPtr&)>& func)
    {
        assert(diaSymbolEnumerator);

        ULONG fetchedSymbolCount = 0;
        DiaSymbolPtr diaChildSymbol;

        auto result = diaSymbolEnumerator->Next(1, &diaChildSymbol, &fetchedSymbolCount);

        for (; SUCCEEDED(result) && (fetchedSymbolCount == 1);
             diaChildSymbol.Release(), result = diaSymbolEnumerator->Next(1, &diaChildSymbol, &fetchedSymbolCount))
        {
            func(diaChildSymbol);
        }
    }
}

void SymbolModule::UpdateSymbolMapFromEnumerator(const DiaEnumSymbolsPtr& diaSymbolEnumerator)
{
    ForEachDiaSymbol(diaSymbolEnumerator, [this](const DiaSymbolPtr& symbol)
    {
        DiaEnumSymbolsPtr diaSymbolEnumerator1;
        if (SUCCEEDED(symbol->findChildren(SymTagNull, nullptr, nsNone, &diaSymbolEnumerator1)))
        {
            ForEachDiaSymbol(diaSymbolEnumerator1, [this](const DiaSymbolPtr& symbol)
            {
                DWORD dwordResult = 0;
                symbol->get_symTag(&dwordResult);

                auto tag = static_cast<enum SymTagEnum>(dwordResult);
                if (tag != SymTagFunction)
                {
                    return;
                }

                DWORD symIndex = 0;
                symbol->get_symIndexId(&symIndex);

                auto it = m_symbolMap.find(symIndex);
                if (it == m_symbolMap.end())
                {
                    return;
                }

                DiaEnumSymbolsPtr diaSymbolEnumeratorF;
                if (FAILED(symbol->findChildren(SymTagNull, nullptr, nsNone, &diaSymbolEnumeratorF)))
                {
                    return;
                }

                DWORD argc = 0;
                ForEachDiaSymbol(diaSymbolEnumeratorF, [this, &argc, &it](const DiaSymbolPtr& symbol)
                {
                    DWORD symTag = 0;
                    symbol->get_symTag(&symTag);
                    if (symTag != SymTagData)
                    {
                        return;
                    }

                    DWORD dwordResult = 0;
                    symbol->get_dataKind(&dwordResult);
                    if (dwordResult == DataIsParam)
                    {
                        auto newSymbol = std::make_shared<Symbol>();
                        newSymbol->name = GetSymbolName(symbol);
                        std::get<SymbolFunction>(it->second->variant).arguments.push_back({std::move(newSymbol)});
                    }
                });
            });
        }
    });
}

void SymbolModule::BuildSymbolMapFromEnumerator(const DiaEnumSymbolsPtr& diaSymbolEnumerator)
{
    ForEachDiaSymbol(diaSymbolEnumerator, [this](const DiaSymbolPtr& symbol)
    {
        GetSymbol(symbol);
    });
}

void SymbolModule::BuildFunctionSetFromEnumerator(const DiaEnumSymbolsPtr& diaSymbolEnumerator)
{
    ForEachDiaSymbol(diaSymbolEnumerator, [this](const DiaSymbolPtr& symbol)
    {
        DWORD dwordResult = 0;
        symbol->get_symTag(&dwordResult);

        auto tag = static_cast<enum SymTagEnum>(dwordResult);
        if (tag != SymTagThunk)
        {
            auto functionName = GetSymbolName(symbol, false);
            m_functionSet.insert(functionName);
        }
    });
}

void SymbolModule::BuildSymbolMap()
{
    if (DiaEnumSymbolsPtr diaSymbolEnumerator; SUCCEEDED(m_globalSymbol->findChildren(SymTagPublicSymbol, nullptr, nsNone, &diaSymbolEnumerator)))
    {
        BuildFunctionSetFromEnumerator(diaSymbolEnumerator);
    }
    if (DiaEnumSymbolsPtr diaSymbolEnumerator; SUCCEEDED(m_globalSymbol->findChildren(SymTagEnum, nullptr, nsNone, &diaSymbolEnumerator)))
    {
        BuildSymbolMapFromEnumerator(diaSymbolEnumerator);
    }
    if (DiaEnumSymbolsPtr diaSymbolEnumerator; SUCCEEDED(m_globalSymbol->findChildren(SymTagUDT, nullptr, nsNone, &diaSymbolEnumerator)))
    {
        BuildSymbolMapFromEnumerator(diaSymbolEnumerator);
    }
    if (DiaEnumSymbolsPtr diaSymbolEnumerator; SUCCEEDED(m_globalSymbol->findChildren(SymTagCompiland, nullptr, nsNone, &diaSymbolEnumerator)))
    {
        UpdateSymbolMapFromEnumerator(diaSymbolEnumerator);
    }

    ResolveNestedTemplateNames();
}

namespace
{
    //
    // MSVC's codes for the builtin types, as they turn up in the argument list
    // of a decorated template name.  The longer codes come first so that "_J"
    // is not read as "_" followed by "J".
    //
    struct MangledBuiltinType
    {
        const char* code;
        const char* type;
    };

    const MangledBuiltinType MangledBuiltinTypes[] = {
        { "_N", "bool"             },
        { "_W", "wchar_t"          },
        { "_Q", "char8_t"          },
        { "_S", "char16_t"         },
        { "_U", "char32_t"         },
        { "_J", "__int64"          },
        { "_K", "unsigned __int64" },
        { "D",  "char"             },
        { "E",  "unsigned char"    },
        { "F",  "short"            },
        { "G",  "unsigned short"   },
        { "H",  "int"              },
        { "I",  "unsigned int"     },
        { "J",  "long"             },
        { "K",  "unsigned long"    },
        { "M",  "float"            },
        { "N",  "double"           },
        { "O",  "long double"      },
        { "X",  "void"             },
    };

    //
    // Spells out the arguments of a decorated template name as long as every
    // one of them is a builtin type: "?$Same@H" gives "int".  Class arguments,
    // non-type arguments and pointers need a full MSVC demangler, so they give
    // nothing back and the caller falls back on another way of telling the
    // instantiations apart.
    //
    std::string DecodeTemplateArguments(const std::string& name)
    {
        const auto argumentsStart = name.find('@', 2);
        if (!PDB::IsDecoratedTemplateName(name) || argumentsStart == std::string::npos)
        {
            return {};
        }

        std::string arguments;

        for (size_t i = argumentsStart + 1; i < name.size() && name[i] != '@'; )
        {
            const MangledBuiltinType* decoded = nullptr;

            for (const auto& builtin : MangledBuiltinTypes)
            {
                const auto codeLength = strlen(builtin.code);
                if (name.compare(i, codeLength, builtin.code) == 0)
                {
                    decoded = &builtin;
                    i += codeLength;
                    break;
                }
            }

            if (decoded == nullptr)
            {
                return {};
            }

            if (!arguments.empty())
            {
                arguments += ",";
            }

            arguments += decoded->type;
        }

        return arguments;
    }
}

void SymbolModule::ResolveNestedTemplateNames()
{
    //
    // Reached through its parent, a nested template instantiation carries the
    // decorated name "?$InnerT@H", and DIA refuses to undecorate it.  The same
    // type is listed at global scope under the readable qualified name
    // "OuterT<float>::InnerT<int>", so the readable one is looked up there and
    // put back on the nested symbol.
    //
    // Candidates are grouped by the scope they live in together with the
    // template they instantiate, which is all the decorated name gives away.
    //
    std::unordered_map<std::string, std::vector<SymbolPtr>> candidatesByScope;

    for (const auto&[name, symbol] : m_symbolNameMap)
    {
        if (!symbol || (symbol->tag != SymTagUDT && symbol->tag != SymTagEnum))
        {
            continue;
        }

        const std::string unqualifiedName = PDB::GetUnqualifiedName(name);
        if (unqualifiedName.size() == name.size())
        {
            continue;
        }

        const std::string scope = name.substr(0, name.size() - unqualifiedName.size() - 2);
        candidatesByScope[scope + "::" + PDB::GetTemplateBaseName(unqualifiedName)].push_back(symbol);
    }

    //
    // A nested template can hold nested templates of its own, and looking those
    // up needs the enclosing name to be readable already, so the pass repeats
    // until it stops finding anything.
    //
    for (bool resolvedAny = true; resolvedAny; )
    {
        resolvedAny = false;

        for (const auto&[_, symbol] : m_symbolMap)
        {
            if (!symbol || symbol->name.empty() || !std::holds_alternative<SymbolUdt>(symbol->variant))
            {
                continue;
            }

            for (auto& field : std::get<SymbolUdt>(symbol->variant).fields)
            {
                if (!field.IsNestedTypeDeclaration() ||
                    !PDB::IsDecoratedTemplateName(field.type->name))
                {
                    continue;
                }

                const std::string templateName =
                    symbol->name + "::" + PDB::GetTemplateBaseName(field.type->name);

                const auto candidates = candidatesByScope.find(templateName);
                if (candidates == candidatesByScope.end())
                {
                    continue;
                }

                //
                // Where the decorated arguments can be read, they name the
                // instantiation outright; the name is still only accepted if a
                // symbol really goes by it, so a misread cannot invent one.
                // Otherwise the size has to tell the instantiations apart, and
                // if even that does not single one out the decorated name stays:
                // a wrong name would be worse than an unreadable one.
                //
                const std::string decodedArguments = DecodeTemplateArguments(field.type->name);
                const std::string decodedName = decodedArguments.empty()
                    ? std::string{}
                    : templateName + "<" + decodedArguments + ">";

                const Symbol* match = nullptr;
                bool ambiguous = false;

                for (const auto& candidate : candidates->second)
                {
                    if (candidate->tag != field.type->tag || candidate->size != field.type->size)
                    {
                        continue;
                    }

                    if (!decodedName.empty() && candidate->name == decodedName)
                    {
                        match = candidate.get();
                        ambiguous = false;
                        break;
                    }

                    if (match != nullptr)
                    {
                        ambiguous = true;
                        continue;
                    }

                    match = candidate.get();
                }

                if (match == nullptr || ambiguous)
                {
                    continue;
                }

                field.type->name = match->name;
                field.name = match->name;
                resolvedAny = true;
            }
        }
    }
}

const SymbolMap& SymbolModule::GetSymbolMap() const
{
    return m_symbolMap;
}

const SymbolNameMap& SymbolModule::GetSymbolNameMap() const
{
    return m_symbolNameMap;
}

const FunctionSet& SymbolModule::GetFunctionSet() const
{
    return m_functionSet;
}

void SymbolModule::InitSymbol(const DiaSymbolPtr& diaSymbol, const SymbolPtr& symbol)
{
    assert(symbol);
    assert(diaSymbol);

    DWORD dwordResult = 0;
    ULONGLONG uLongLongResult = 0;
    BOOL boolResult = FALSE;

    diaSymbol->get_symIndexId(&dwordResult);
    symbol->symIndexId = dwordResult;

    diaSymbol->get_symTag(&dwordResult);
    symbol->tag = static_cast<enum SymTagEnum>(dwordResult);

    diaSymbol->get_baseType(&dwordResult);
    symbol->baseType = static_cast<BasicType>(dwordResult);

    diaSymbol->get_typeId(&dwordResult);
    symbol->typeId = dwordResult;

    diaSymbol->get_length(&uLongLongResult);
    symbol->size = static_cast<DWORD>(uLongLongResult);

    diaSymbol->get_constType(&boolResult);
    symbol->isConst = static_cast<bool>(boolResult);

    diaSymbol->get_volatileType(&boolResult);
    symbol->isVolatile = static_cast<bool>(boolResult);

    symbol->name = GetSymbolName(diaSymbol);

    switch (symbol->tag)
    {
    case SymTagUDT:
        ProcessSymbolUdt(diaSymbol, symbol);
        break;

    case SymTagEnum:
        ProcessSymbolEnum(diaSymbol, symbol);
        break;

    case SymTagFunctionType:
        ProcessSymbolFunction(diaSymbol, symbol);
        break;

    case SymTagPointerType:
        ProcessSymbolPointer(diaSymbol, symbol);
        break;

    case SymTagArrayType:
        ProcessSymbolArray(diaSymbol, symbol);
        break;

    case SymTagBaseType:
        ProcessSymbolBase(diaSymbol, symbol);
        break;

    case SymTagTypedef:
        ProcessSymbolTypedef(diaSymbol, symbol);
        break;

    case SymTagFunctionArgType:
        ProcessSymbolFunctionArgType(diaSymbol, symbol);
        break;

    case SymTagFunction:
        ProcessSymbolFunctionEx(diaSymbol, symbol);
        break;

    case SymTagVTable:
        ProcessSymbolVTable(diaSymbol, symbol);
        break;

    default:
        break;
    }
}

void SymbolModule::ProcessSymbolBase(const DiaSymbolPtr& diaSymbol, const SymbolPtr& symbol)
{
}

void SymbolModule::ProcessSymbolVTable(const DiaSymbolPtr& diaSymbol, const SymbolPtr& symbol)
{
    assert(symbol);

    //
    // DIA reports no length for the virtual function table pointer, yet it does
    // take up one pointer at the front of the layout.  Leaving it at zero makes
    // every following member look misplaced and produces a bogus padding member.
    //
    if (symbol->size == 0)
    {
        symbol->size = m_machineType == IMAGE_FILE_MACHINE_I386 ? 4 : 8;
    }
}

void SymbolModule::ProcessSymbolEnum(const DiaSymbolPtr& diaSymbol, const SymbolPtr& symbol)
{
    assert(diaSymbol);
    assert(symbol);

    symbol->variant = SymbolEnum{};
    auto& symbolEnum = std::get<SymbolEnum>(symbol->variant);

    DiaEnumSymbolsPtr diaSymbolEnumerator;
    if (FAILED(diaSymbol->findChildren(SymTagNull, nullptr, nsNone, &diaSymbolEnumerator)))
    {
        return;
    }

    ForEachDiaSymbol(diaSymbolEnumerator, [this, &symbolEnum, &symbol](const DiaSymbolPtr& diaSymbol)
    {
        symbolEnum.fields.push_back({});
        auto& enumValue = symbolEnum.fields.back();

        enumValue.parent = symbol;
        enumValue.name = GetSymbolName(diaSymbol);

        VariantInit(&enumValue.value);
        diaSymbol->get_value(&enumValue.value);
    });
}

void SymbolModule::ProcessSymbolTypedef(const DiaSymbolPtr& diaSymbol, const SymbolPtr& symbol)
{
    assert(diaSymbol);
    assert(symbol);

    DiaSymbolPtr diaTypedefSymbol;
    diaSymbol->get_type(&diaTypedefSymbol);

    SymbolTypedef typedefSymbol;
    typedefSymbol.type = GetSymbol(diaTypedefSymbol);
    symbol->variant = std::move(typedefSymbol);
}

void SymbolModule::ProcessSymbolPointer(const DiaSymbolPtr& diaSymbol, const SymbolPtr& symbol)
{
    assert(diaSymbol);
    assert(symbol);

    DiaSymbolPtr diaPointerSymbol;
    diaSymbol->get_type(&diaPointerSymbol);

    SymbolPointer pointer;
    BOOL isReference = FALSE;
    diaSymbol->get_reference(&isReference);
    pointer.isReference = static_cast<bool>(isReference);

    pointer.type = GetSymbol(diaPointerSymbol);

    if (m_machineType == 0)
    {
        switch (symbol->size)
        {
        case 4:  m_machineType = IMAGE_FILE_MACHINE_I386;  break;
        case 8:  m_machineType = IMAGE_FILE_MACHINE_AMD64; break;
        default: m_machineType = 0; break;
        }
    }

    symbol->variant = std::move(pointer);
}

void SymbolModule::ProcessSymbolArray(const DiaSymbolPtr& diaSymbol, const SymbolPtr& symbol)
{
    assert(diaSymbol);
    assert(symbol);

    DiaSymbolPtr diaDataTypeSymbol;
    diaSymbol->get_type(&diaDataTypeSymbol);

    SymbolArray arraySymbol;
    arraySymbol.elementType = GetSymbol(diaDataTypeSymbol);

    diaSymbol->get_count(&arraySymbol.elementCount);

    symbol->variant = std::move(arraySymbol);
}

void SymbolModule::ProcessSymbolFunction(const DiaSymbolPtr& diaSymbol, const SymbolPtr& symbol)
{
    assert(diaSymbol);
    assert(symbol);

    if (std::holds_alternative<std::monostate>(symbol->variant))
    {
        symbol->variant = SymbolFunction{};
    }
    auto& function = std::get<SymbolFunction>(symbol->variant);

    DWORD callingConvention = 0;
    diaSymbol->get_callingConvention(&callingConvention);

    function.callingConvention = static_cast<CV_call_e>(callingConvention);

    DiaSymbolPtr diaReturnTypeSymbol;
    diaSymbol->get_type(&diaReturnTypeSymbol);
    function.returnType = GetSymbol(diaReturnTypeSymbol);

    // Getting this pointer type
    DiaSymbolPtr diaThisPointerTypeSymbol;
    diaSymbol->get_objectPointerType(&diaThisPointerTypeSymbol);

    if (diaThisPointerTypeSymbol)
    {
        // Getting this type
        DiaSymbolPtr diaThisTypeSymbol;
        diaThisPointerTypeSymbol->get_type(&diaThisTypeSymbol);

        BOOL isConst = FALSE;
        diaThisTypeSymbol->get_constType(&isConst);
        function.isConst = static_cast<bool>(isConst);
    }

    DiaEnumSymbolsPtr diaSymbolEnumerator;

    if (FAILED(diaSymbol->findChildren(SymTagNull, nullptr, nsNone, &diaSymbolEnumerator)))
    {
        return;
    }

    ForEachDiaSymbol(diaSymbolEnumerator, [this, &function](const DiaSymbolPtr& diaSymbol)
    {
        function.arguments.push_back({GetSymbol(diaSymbol)});
    });
}

void SymbolModule::ProcessSymbolFunctionArgType(const DiaSymbolPtr& diaSymbol, const SymbolPtr& symbol)
{
    assert(diaSymbol);
    assert(symbol);

    DiaSymbolPtr diaArgumentTypeSymbol;

    diaSymbol->get_type(&diaArgumentTypeSymbol);

    SymbolFunctionArgType funcArg;
    funcArg.type = GetSymbol(diaArgumentTypeSymbol);

    symbol->variant = std::move(funcArg);
}

void SymbolModule::ProcessSymbolUdt(const DiaSymbolPtr& diaSymbol, const SymbolPtr& symbol)
{
    assert(diaSymbol);
    assert(symbol);

    symbol->variant = SymbolUdt{};
    auto& udt = std::get<SymbolUdt>(symbol->variant);

    DWORD kind = 0;
    diaSymbol->get_udtKind(&kind);
    udt.kind = static_cast<UdtKind>(kind);

    DiaEnumSymbolsPtr diaSymbolEnumerator;

    if (FAILED(diaSymbol->findChildren(SymTagNull, nullptr, nsNone, &diaSymbolEnumerator)))
    {
        return;
    }

    ForEachDiaSymbol(diaSymbolEnumerator, [this, &udt, &symbol](const DiaSymbolPtr& diaChildSymbol)
    {
        BOOL isCompilerGenerated = FALSE;
        diaChildSymbol->get_compilerGenerated(&isCompilerGenerated);
        if (isCompilerGenerated == TRUE)
        {
            // TODO: resolve this by pdbex input parameter
            return;
        }

        DWORD symTag = 0;
        diaChildSymbol->get_symTag(&symTag);

        SymbolUdtField member;

        member.name = GetSymbolName(diaChildSymbol);
        member.parent = symbol;
        member.isBaseClass = false;

        member.tag = static_cast<enum SymTagEnum>(symTag);
        member.dataKind = static_cast<enum DataKind>(0); //???

        LONG offset = 0;
        diaChildSymbol->get_offset(&offset);
        member.offset = static_cast<DWORD>(offset);

        ULONGLONG bits = 0;
        if (symTag == SymTagData)
        {
            diaChildSymbol->get_length(&bits);
        }

        DWORD dwAccess = 0;
        diaChildSymbol->get_access(&dwAccess);
        member.access = dwAccess;

        member.bits = static_cast<DWORD>(bits);

        diaChildSymbol->get_bitPosition(&member.bitPosition);

        if (symTag == SymTagData || symTag == SymTagBaseClass)
        {
            DWORD dwordResult = 0;
            diaChildSymbol->get_dataKind(&dwordResult);
            member.dataKind = static_cast<enum DataKind>(dwordResult);

            DiaSymbolPtr memberTypeDiaSymbol;
            diaChildSymbol->get_type(&memberTypeDiaSymbol);
            member.type = GetSymbol(memberTypeDiaSymbol);
            if (symTag == SymTagBaseClass)
            {
                udt.baseClassFields.push_back({});
                auto& baseClass = udt.baseClassFields.back();

                baseClass.type = member.type;

                DWORD access = 0;
                diaChildSymbol->get_access(&access);
                baseClass.access = access;

                BOOL isVirtual = FALSE;
                diaChildSymbol->get_virtualBaseClass(&isVirtual);
                baseClass.isVirtual = static_cast<bool>(isVirtual);
                member.isBaseClass = true;
            }
        }
        else
        {
            member.type = GetSymbol(diaChildSymbol);

            if (symTag == SymTagFunction && std::holds_alternative<SymbolFunction>(member.type->variant))
            {
                auto& memberTypeFunction = std::get<SymbolFunction>(member.type->variant);

                // Check if ctor or dtor
                const auto nsPos = symbol->name.rfind("::");
                const auto nameWithoutNS = nsPos == std::string::npos ? symbol->name : symbol->name.substr(nsPos + 2);
                if (member.name == nameWithoutNS || member.name == '~' + nameWithoutNS)
                {
                    memberTypeFunction.returnType = nullptr;
                }

                DiaSymbolPtr memberTypeDiaSymbol;
                diaChildSymbol->get_type(&memberTypeDiaSymbol);

                DiaSymbolPtr diaThisPointerTypeSymbol;
                memberTypeDiaSymbol->get_objectPointerType(&diaThisPointerTypeSymbol);
                if (!diaThisPointerTypeSymbol)
                {
                    memberTypeFunction.isStatic = true;
                }

                if (memberTypeFunction.isOverride && !udt.baseClassFields.empty())
                {
                    for (const auto& baseClass : udt.baseClassFields)
                    {
                        auto& tmpSymbolUdt = std::get<SymbolUdt>(baseClass.type->variant);
                        for (auto& field : tmpSymbolUdt.fields)
                        {
                            if (field.type->tag == SymTagFunction &&
                                strcmp(field.name.c_str(), member.name.c_str()) == 0 &&
                                std::get<SymbolFunction>(field.type->variant).arguments.size() == memberTypeFunction.arguments.size())
                            {
                                memberTypeFunction.virtualOffset = std::get<SymbolFunction>(field.type->variant).virtualOffset;
                            }
                        }
                    }
                }
            }
        }
        udt.fields.push_back(std::move(member));
    });
}

void SymbolModule::ProcessSymbolFunctionEx(const DiaSymbolPtr& diaSymbol, const SymbolPtr& symbol)
{
    assert(diaSymbol);
    assert(symbol);

    symbol->variant = SymbolFunction{};
    auto& function = std::get<SymbolFunction>(symbol->variant);

    DWORD dwAccess = 0;
    diaSymbol->get_access(&dwAccess);
    function.access = dwAccess;

    BOOL isVirtual = FALSE;
    diaSymbol->get_virtual(&isVirtual);
    function.isVirtual = static_cast<bool>(isVirtual);
    function.isOverride = false;

    BOOL isIntro = FALSE;
    if (SUCCEEDED(diaSymbol->get_intro(&isIntro)) && !isIntro && isVirtual)
    {
        function.isOverride = true;
    }

    function.virtualOffset = -1;
    if (function.isVirtual)
    {
        DWORD virtualOffset = 0;
        diaSymbol->get_virtualBaseOffset(&virtualOffset);
        function.virtualOffset = virtualOffset;
    }

    BOOL isPure = FALSE;
    if (isVirtual == TRUE)
    {
        diaSymbol->get_pure(&isPure);
    }
    function.isPure = static_cast<bool>(isPure);

    DiaSymbolPtr diaArgumentTypeSymbol;
    diaSymbol->get_type(&diaArgumentTypeSymbol);

    ProcessSymbolFunction(diaArgumentTypeSymbol, symbol);

    DiaEnumSymbolsPtr diaSymbolEnumerator;
    if (FAILED(diaSymbol->findChildren(SymTagData, nullptr, nsNone, &diaSymbolEnumerator)))
    {
        return;
    }

    size_t argIdx = 0;
    ForEachDiaSymbol(diaSymbolEnumerator, [this, &function, &argIdx](const DiaSymbolPtr& diaSymbol)
    {
        DWORD dataKind = 0;
        diaSymbol->get_dataKind(&dataKind);
        if (dataKind == DataIsParam)
        {
            function.arguments[argIdx++].name = GetSymbolName(diaSymbol);
        }
    });
}

//////////////////////////////////////////////////////////////////////////
// PDB - implementation
//
namespace
{
    struct BasicTypeMapElement
    {
        BasicType baseType = btNoType;
        DWORD length = 0;
        const char* basicTypeString = nullptr;
        const char* typeString = nullptr;
    };

    //
    // Names of the C++ standard library, of the MSVC C runtime and of the types
    // the compiler itself emits.  These tokens are looked for anywhere in a
    // symbol name so that templates instantiated over library types
    // (Foo<std::string>) and nested types (std::vector<int>::iterator) are
    // recognised as well.  The list is deliberately a heuristic: a PDB does not
    // record which library a type came from.
    //
    const char* const StandardLibraryTokens[] = {
        "std::",
        "stdext::",
        "Concurrency::",
        "__std_",
        "__vc_attributes",
        "__crt",
        "__acrt",
        "__vcrt",
        "__scrt",
        "_CRT",
        "_Crt",
        "_crt_",
        "__FrameHandler",
        "FH4::",
        "_LocaleUpdate",
        "_Locinfo",
        "_Lockit",
        "_Iosb",
        "_Yarn",
        "type_info",
    };

    //
    // Runtime types that live in the global namespace and therefore have to be
    // matched from the beginning of the name.
    //
    const char* const StandardLibraryPrefixes[] = {
        "_s_",                  // exception handling tables: _s_FuncInfo, ...
        "_s__",                 // RTTI tables: _s__RTTIBaseClassDescriptor, ...
        "_TypeDescriptor",
        "__non_rtti_object",
        "__ArrayUnwind",
        "_PMD",
        "_ThrowInfo",
        "_CatchableType",
        "_UnwindMapEntry",
        "_TryBlockMapEntry",
        "_HandlerType",
        "_ESTypeList",
        "_FuncInfo",
        "_IPtoStateMap",
        "_onexit_table_t",
        "_iobuf",
        "_ptiddata",
        "_locale",
        "_setloc_struct",
        "localeinfo_struct",
        "threadlocaleinfostruct",
        "threadmbcinfostruct",
        "__lc_time_data",
        "lconv",
        "tagLC_ID",
        "_Mbstatet",
        "_Timevec",
        "_Ctypevec",
        "_Collvec",
        "_Cvtvec",
        "_Dconst",
    };

    BasicTypeMapElement BasicTypeMapMSVC[] = {
        { btNoType,       0,  "btNoType",         "..."              }, //nullptr
        { btVoid,         0,  "btVoid",           "void"             },
        { btChar,         1,  "btChar",           "char"             },
        { btChar8,        1,  "btChar8",          "char8_t"          },
        { btChar16,       2,  "btChar16",         "char16_t"         },
        { btChar32,       4,  "btChar32",         "char32_t"         },
        { btWChar,        2,  "btWChar",          "wchar_t"          },
        { btInt,          1,  "btInt",            "char"             },
        { btInt,          2,  "btInt",            "short"            },
        { btInt,          4,  "btInt",            "int"              },
        { btInt,          8,  "btInt",            "int64_t"          },
        { btUInt,        16,  "btInt",            "__m128"           },
        { btUInt,         1,  "btUInt",           "unsigned char"    },
        { btUInt,         2,  "btUInt",           "unsigned short"   },
        { btUInt,         4,  "btUInt",           "unsigned int"     },
        { btUInt,         8,  "btUInt",           "uint64_t"         },
        { btUInt,        16,  "btUInt",           "__m128"           },
        { btFloat,        4,  "btFloat",          "float"            },
        { btFloat,        8,  "btFloat",          "double"           },
        { btFloat,       10,  "btFloat",          "long double"      }, // 80-bit float
        { btBCD,          0,  "btBCD",            "BCD"              },
        { btBool,         0,  "btBool",           "bool"             },
        { btLong,         4,  "btLong",           "long"             },
        { btULong,        4,  "btULong",          "unsigned long"    },
        { btCurrency,     0,  "btCurrency",       "<NoType>"         }, //nullptr
        { btDate,         0,  "btDate",           "DATE"             },
        { btVariant,      0,  "btVariant",        "VARIANT"          },
        { btComplex,      0,  "btComplex",        "<NoType>"         }, //nullptr
        { btBit,          0,  "btBit",            "<NoType>"         }, //nullptr
        { btBSTR,         0,  "btBSTR",           "BSTR"             },
        { btHresult,      4,  "btHresult",        "HRESULT"          },
        { (BasicType)0,   0,  nullptr,            "<NoType>"         }, //nullptr
    };
}

PDB::PDB()
{
    m_impl = std::make_unique<SymbolModule>();
}

PDB::PDB(const std::filesystem::path& path)
{
    m_impl = std::make_unique<SymbolModule>();
    m_impl->Open(path);
}


bool PDB::Open(const std::filesystem::path& path)
{
    assert(m_impl != nullptr);
    return m_impl->Open(path);
}

bool PDB::IsOpened() const
{
    assert(m_impl != nullptr);
    return m_impl->IsOpen();
}

const std::filesystem::path PDB::GetPath() const
{
    return m_impl->GetPath();
}

void PDB::Close()
{
    m_impl->Close();
}

DWORD PDB::GetMachineType() const
{
    return m_impl->GetMachineType();
}

CV_CFL_LANG PDB::GetLanguage() const
{
    return m_impl->GetLanguage();
}

const SymbolPtr PDB::GetSymbolByName(const std::string& symbolName)
{
    return m_impl->GetSymbolByName(symbolName);
}

const SymbolPtr PDB::GetSymbolBySymbolIndex(DWORD typeId)
{
    return m_impl->GetSymbolBySymbolIndex(typeId);
}

const SymbolMap& PDB::GetSymbolMap() const
{
    return m_impl->GetSymbolMap();
}

const SymbolNameMap& PDB::GetSymbolNameMap() const
{
    return m_impl->GetSymbolNameMap();
}

const FunctionSet& PDB::GetFunctionSet() const
{
    return m_impl->GetFunctionSet();
}

const std::string PDB::GetBasicTypeString(BasicType BaseType, DWORD size)
{
    for (int n = 0; BasicTypeMapMSVC[n].basicTypeString != nullptr; ++n)
    {
        if (BasicTypeMapMSVC[n].baseType == BaseType)
        {
            if (BasicTypeMapMSVC[n].length == size ||
                BasicTypeMapMSVC[n].length == 0)
            {
                return BasicTypeMapMSVC[n].typeString; //TODO NULL ???
            }
        }
    }

    return "<NoType>"; //nullptr;
}

const std::string PDB::GetBasicTypeString(const Symbol& symbol)
{
    return GetBasicTypeString(symbol.baseType, symbol.size);
}

const std::string PDB::GetUdtKindString(UdtKind kind)
{
    static const CHAR* UdtKindStrings[] = {
        "struct",
        "class",
        "union",
    };

    if (kind >= UdtStruct && kind <= UdtUnion)
    {
        return UdtKindStrings[kind];
    }

    return {};
}

bool PDB::IsUnnamedSymbol(const Symbol& symbol)
{
    return strstr(symbol.name.c_str(), "<anonymous-") != nullptr ||
           strstr(symbol.name.c_str(), "<unnamed-") != nullptr ||
           strstr(symbol.name.c_str(), "__unnamed") != nullptr;
}

bool PDB::IsDecoratedTemplateName(const std::string& name)
{
    return name.compare(0, 2, "?$") == 0;
}

std::string PDB::GetTemplateBaseName(const std::string& name)
{
    if (IsDecoratedTemplateName(name))
    {
        const auto argumentsStart = name.find('@', 2);
        return name.substr(2, argumentsStart == std::string::npos
            ? std::string::npos
            : argumentsStart - 2);
    }

    const auto argumentsStart = name.find('<');
    return argumentsStart == std::string::npos ? name : name.substr(0, argumentsStart);
}

bool PDB::IsStandardLibraryName(const std::string& name)
{
    for (const auto* token : StandardLibraryTokens)
    {
        if (name.find(token) != std::string::npos)
        {
            return true;
        }
    }

    //
    // The compiler decorates some runtime records it grows by hand, for example
    // "$_s__RTTIBaseClassArray$_extraBytes_16"; the type they are built from
    // follows the leading '$'.
    //
    const char* bareName = name.c_str();
    if (*bareName == '$')
    {
        ++bareName;
    }

    for (const auto* prefix : StandardLibraryPrefixes)
    {
        if (strncmp(bareName, prefix, strlen(prefix)) == 0)
        {
            return true;
        }
    }

    return false;
}

bool PDB::IsStandardLibrarySymbol(const Symbol& symbol)
{
    return IsStandardLibraryName(symbol.name);
}

std::string PDB::GetUnqualifiedName(const std::string& name)
{
    //
    // Only the '::' that separate the enclosing scopes count; the ones that
    // appear inside template arguments belong to the name itself.
    //
    int templateDepth = 0;
    size_t lastScope = std::string::npos;

    for (size_t i = 0; i < name.size(); ++i)
    {
        switch (name[i])
        {
        case '<':
            ++templateDepth;
            break;

        case '>':
            --templateDepth;
            break;

        case ':':
            if (templateDepth == 0 && i + 1 < name.size() && name[i + 1] == ':')
            {
                lastScope = i;
                ++i;
            }
            break;

        default:
            break;
        }
    }

    return lastScope == std::string::npos ? name : name.substr(lastScope + 2);
}

bool SymbolUdtField::OccupiesStorage() const
{
    switch (tag)
    {
    case SymTagData:
        return dataKind != DataIsStaticMember;

    case SymTagBaseClass:
    case SymTagVTable:
        return true;

    default:
        return false;
    }
}

bool SymbolUdtField::IsNestedTypeDeclaration() const
{
    return (tag == SymTagUDT || tag == SymTagEnum) && type != nullptr;
}

const SymbolUdtField* SymbolUdt::FieldFirst() const
{
    return fields.data();
}

const SymbolUdtField* SymbolUdt::FieldLast() const
{
    //
    // One past the last field, so that the last field itself still takes part
    // in the look-ahead the anonymous struct/union detection relies on.
    //
    return fields.data() + fields.size();
}

const SymbolUdtField* SymbolUdt::FieldNext(const SymbolUdtField* Field) const
{
    return Field == FieldLast() ? FieldLast() : &Field[1];
}

const SymbolUdtField* SymbolUdt::FindFieldNext(const SymbolUdtField* Field) const
{
    //
    // Look-ahead is about the layout, so everything that does not take up space
    // (nested types, member functions, typedefs, static data members) is passed
    // over.
    //
    while ((Field = FieldNext(Field)) != FieldLast() && !Field->OccupiesStorage());
    return Field;
}
