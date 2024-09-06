#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <atlbase.h>
#include <dia2.h>
#include <string>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <variant>
#include <filesystem>

struct Symbol;
using SymbolPtr = std::shared_ptr<Symbol>;

struct SymbolEnumField
{
    std::string name;
    VARIANT value;
    SymbolPtr parent;
};

struct SymbolUdtField
{
    enum SymTagEnum tag = SymTagNull;
    enum DataKind dataKind = DataIsUnknown;
    std::string name;
    SymbolPtr type;
    DWORD offset = 0;
    DWORD bits = 0;
    DWORD bitPosition = 0;
    SymbolPtr parent;
    DWORD access = 0;
    bool isBaseClass = false;
};

struct SymbolEnum
{
    std::vector<SymbolEnumField> fields;
};

struct SymbolTypedef
{
    SymbolPtr type;
};

struct SymbolPointer
{
    SymbolPtr type;
    bool isReference = false;
};

struct SymbolArray
{
    SymbolPtr elementType;
    DWORD elementCount = 0;
};

struct SymbolFunction
{
    SymbolPtr returnType;
    CV_call_e callingConvention = CV_CALL_RESERVED;
    bool isStatic = false;
    bool isVirtual = false;
    DWORD virtualOffset = false;
    bool isOverride = false;
    bool isConst = false;
    bool isPure = false;
    DWORD access = 0;
    std::vector<SymbolPtr> arguments;

};

struct SymbolFunctionArg
{
    SymbolPtr type;
};

struct SymbolUdtBaseClass
{
    SymbolPtr type;
    DWORD access = 0;
    bool isVirtual = false;
};

struct SymbolUdt
{
    UdtKind kind = UdtStruct;
    std::vector<SymbolUdtField> fields;
    std::vector<SymbolUdtBaseClass> baseClassFields;

    //TODO: refactor this shit
    const SymbolUdtField* FieldFirst() const;
    const SymbolUdtField* FieldLast() const;
    const SymbolUdtField* FieldNext(const SymbolUdtField* Field) const;
    const SymbolUdtField* FindFieldNext(const SymbolUdtField* Field) const;
};

using SymbolVariant = std::variant<std::monostate,
                                   SymbolEnum,
                                   SymbolTypedef,
                                   SymbolPointer,
                                   SymbolArray,
                                   SymbolFunction,
                                   SymbolFunctionArg,
                                   SymbolUdt>;

struct Symbol
{
    enum SymTagEnum tag = SymTagNull;
    BasicType baseType = btNoType;
    DWORD typeId = 0;
    DWORD symIndexId = 0;
    DWORD size = 0;
    bool isConst = false;
    bool isVolatile = false;
    std::string name;
    SymbolVariant variant;
};

using SymbolMap = std::unordered_map<DWORD, SymbolPtr>;
using SymbolNameMap = std::unordered_map<std::string, SymbolPtr>;
using SymbolSet = std::unordered_set<SymbolPtr>;
using FunctionSet = std::set<std::string>;

using DiaSymbolPtr = ATL::CComPtr<IDiaSymbol>;
using DiaEnumSymbolsPtr = ATL::CComPtr<IDiaEnumSymbols>;

class SymbolModuleBase
{
public:
    SymbolModuleBase();
    virtual ~SymbolModuleBase() = default;

    virtual bool Open(const std::filesystem::path& path);
    virtual void Close();
    virtual bool IsOpen() const;

private:
    HRESULT LoadDiaViaCoCreateInstance();
    HRESULT LoadDiaViaLoadLibrary();

protected:
    ATL::CComPtr<IDiaDataSource> m_dataSource;
    ATL::CComPtr<IDiaSession> m_session;
    ATL::CComPtr<IDiaSymbol> m_globalSymbol;
};

class SymbolModule : public SymbolModuleBase
{
public:
    ~SymbolModule();

    bool Open(const std::filesystem::path& path) override;
    void Close() override;

    const std::filesystem::path& GetPath() const;
    DWORD GetMachineType() const;
    CV_CFL_LANG GetLanguage() const;

    SymbolPtr GetSymbolByName(const std::string& symbolName);
    SymbolPtr GetSymbolBySymbolIndex(DWORD typeId);
    SymbolPtr GetSymbol(const DiaSymbolPtr& diaSymbol);
    std::string GetSymbolName(const DiaSymbolPtr& DiaSymbol, bool raw = true);

    void UpdateSymbolMapFromEnumerator(const DiaEnumSymbolsPtr& DiaSymbolEnumerator);
    void BuildSymbolMapFromEnumerator(const DiaEnumSymbolsPtr& DiaSymbolEnumerator);
    void BuildFunctionSetFromEnumerator(const DiaEnumSymbolsPtr& DiaSymbolEnumerator);

    void BuildSymbolMap();
    const SymbolMap& GetSymbolMap() const;
    const SymbolNameMap& GetSymbolNameMap() const;
    const FunctionSet& GetFunctionSet() const;

private:
    void InitSymbol(const DiaSymbolPtr& DiaSymbol, const SymbolPtr& Symbol);
    void ProcessSymbolBase(const DiaSymbolPtr& DiaSymbol, const SymbolPtr& Symbol);
    void ProcessSymbolEnum(const DiaSymbolPtr& DiaSymbol, const SymbolPtr& Symbol);
    void ProcessSymbolTypedef(const DiaSymbolPtr& DiaSymbol, const SymbolPtr& Symbol);
    void ProcessSymbolPointer(const DiaSymbolPtr& DiaSymbol, const SymbolPtr& Symbol);
    void ProcessSymbolArray(const DiaSymbolPtr& DiaSymbol, const SymbolPtr& Symbol);
    void ProcessSymbolFunction(const DiaSymbolPtr& DiaSymbol, const SymbolPtr& Symbol);
    void ProcessSymbolFunctionArg(const DiaSymbolPtr& DiaSymbol, const SymbolPtr& Symbol);
    void ProcessSymbolUdt(const DiaSymbolPtr& DiaSymbol, const SymbolPtr& Symbol);
    void ProcessSymbolFunctionEx(const DiaSymbolPtr& DiaSymbol, const SymbolPtr& Symbol);

private:
    std::filesystem::path m_path;
    SymbolMap m_symbolMap;
    SymbolNameMap m_symbolNameMap;
    SymbolSet m_symbolSet;
    FunctionSet m_functionSet;

    DWORD m_machineType = 0;
    CV_CFL_LANG m_language = CV_CFL_C;
};

class PDB
{
public:
    PDB();
    PDB(const std::filesystem::path& path);

    bool Open(const std::filesystem::path& path);
    bool IsOpened() const;
    void Close();

    const std::filesystem::path GetPath() const;

    DWORD GetMachineType() const;
    CV_CFL_LANG GetLanguage() const;

    const SymbolPtr GetSymbolByName(const std::string& symbolName);
    const SymbolPtr GetSymbolBySymbolIndex(DWORD typeId);
    const SymbolMap& GetSymbolMap() const;
    const SymbolNameMap& GetSymbolNameMap() const;
    const FunctionSet& GetFunctionSet() const;

    static const std::string GetBasicTypeString(BasicType baseType, DWORD size);
    static const std::string GetBasicTypeString(const Symbol& symbol);
    static const std::string GetUdtKindString(UdtKind kind);
    static bool IsUnnamedSymbol(const Symbol& symbol);

private:
    std::unique_ptr<SymbolModule> m_impl;
};
