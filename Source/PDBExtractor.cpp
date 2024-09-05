#include "PDBExtractor.h"
#include "PDBHeaderReconstructor.h"
#include "PDBSymbolVisitor.h"
#include "PDBSymbolSorter.h"
#include "UdtFieldDefinition.h"

#include <iostream>
#include <fstream>
#include <stdexcept>

namespace
{
	static const char* MESSAGE_INVALID_PARAMETERS = "Invalid parameters";
	static const char* MESSAGE_FILE_NOT_FOUND = "File not found";
	static const char* MESSAGE_SYMBOL_NOT_FOUND = "Symbol not found";

	class PDBDumperException : public std::runtime_error
	{
	public:
		PDBDumperException(const char* Message)
			: std::runtime_error(Message)
		{

		}
	};
}

int PDBExtractor::Run(int argc, char** argv)
{
	int result = 0;

	try
	{
		ParseParameters(argc, argv);
		OpenPDBFile();
		DumpAllSymbols();
	}
	catch (const PDBDumperException& e)
	{
		std::cerr << e.what() << std::endl;
		result = 1;
	}

	return result;
}

void PDBExtractor::PrintUsage()
{
	std::cout << ("Extracts types and structures from PDB (Program database).\n");
	std::cout << ("\n");
	std::cout << ("pdbex <path> [-o <filename>] [-e <type>]\n");
	std::cout << ("                     [-u <prefix>] [-s prefix] [-r prefix] [-g suffix]\n");
	std::cout << ("                     [-p] [-x] [-b] [-d]\n");
	std::cout << ("\n");
	std::cout << ("<path>               Path to the PDB file.\n");
	std::cout << (" -o filename         Specifies the output file.                       (stdout)\n");
	std::cout << (" -e [n,i,a]          Specifies expansion of nested structures/unions. (i)\n");
	std::cout << ("                       n = none            Only top-most type is printed.\n");
	std::cout << ("                       i = inline unnamed  Unnamed types are nested.\n");
	std::cout << ("                       a = inline all      All types are nested.\n");
	std::cout << (" -u prefix           Unnamed union prefix  (in combination with -d).\n");
	std::cout << (" -s prefix           Unnamed struct prefix (in combination with -d).\n");
	std::cout << (" -r prefix           Prefix for all symbols.\n");
	std::cout << (" -g suffix           Suffix for all symbols.\n");
	std::cout << ("\n");
	std::cout << ("Following options can be explicitly turned off by adding trailing '-'.\n");
	std::cout << ("Example: -p-\n");
	std::cout << (" -p                  Create padding members.                          (T)\n");
	std::cout << (" -x                  Show offsets.                                    (T)\n");
	std::cout << (" -b                  Allow bitfields in union.                        (F)\n");
	std::cout << (" -d                  Allow unnamed data types.                        (T)\n");
	std::cout << ("\n");
}

void PDBExtractor::ParseParameters(int argc, char** argv)
{
	if ( argc == 1 ||
	    (argc == 2 && strcmp(argv[1], "-h") == 0) ||
	    (argc == 2 && strcmp(argv[1], "--help") == 0))
	{
		PrintUsage();
		std::exit(0);
	}

	int argumentPointer = 0;

	m_settings.pdbPath = argv[++argumentPointer];

	while (++argumentPointer < argc)
	{
		const std::string currentArgument = argv[argumentPointer];
		const std::string nextArgument = argumentPointer < argc ? argv[argumentPointer + 1] : std::string{};

		if ((currentArgument.size() != 2 && currentArgument.size() != 3) ||
		    (currentArgument.size() == 2 && currentArgument[0] != '-') ||
		    (currentArgument.size() == 3 && currentArgument[0] != '-' && currentArgument[2] != '-'))
		{
			throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);
		}

		bool offSwitch = currentArgument.size() == 3 && currentArgument[2] == '-';

		switch (currentArgument[1])
		{
		case 'o':
			if (nextArgument.empty())
			{
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);
			}

			++argumentPointer;
			m_settings.outputFilename = nextArgument;
			m_settings.pdbHeaderReconstructorSettings.outputFile = std::make_unique<std::ofstream>(nextArgument, std::ios::out);
			m_settings.pdbHeaderReconstructorSettings.output = *m_settings.pdbHeaderReconstructorSettings.outputFile;
			break;

		case 'e':
			if (nextArgument.empty())
			{
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);
			}

			++argumentPointer;
			switch (nextArgument[0])
			{
			case 'n':
				m_settings.pdbHeaderReconstructorSettings.memberStructExpansion =
					PDBHeaderReconstructor::MemberStructExpansionType::None;
				break;

			case 'i':
				m_settings.pdbHeaderReconstructorSettings.memberStructExpansion =
					PDBHeaderReconstructor::MemberStructExpansionType::InlineUnnamed;
				break;

			case 'a':
				m_settings.pdbHeaderReconstructorSettings.memberStructExpansion =
					PDBHeaderReconstructor::MemberStructExpansionType::InlineAll;
				break;

			default:
				m_settings.pdbHeaderReconstructorSettings.memberStructExpansion =
					PDBHeaderReconstructor::MemberStructExpansionType::InlineUnnamed;
				break;
			}
			break;

		case 'u':
			if (nextArgument.empty())
			{
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);
			}

			++argumentPointer;
			m_settings.pdbHeaderReconstructorSettings.anonymousUnionPrefix = nextArgument;
			break;

		case 's':
			if (nextArgument.empty())
			{
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);
			}

			++argumentPointer;
			m_settings.pdbHeaderReconstructorSettings.anonymousStructPrefix = nextArgument;
			break;

		case 'r':
			if (nextArgument.empty())
			{
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);
			}

			++argumentPointer;
			m_settings.pdbHeaderReconstructorSettings.symbolPrefix = nextArgument;
			break;

		case 'g':
			if (nextArgument.empty())
			{
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);
			}

			++argumentPointer;
			m_settings.pdbHeaderReconstructorSettings.symbolSuffix = nextArgument;
			break;

		case 'p':
			m_settings.pdbHeaderReconstructorSettings.createPaddingMembers = !offSwitch;
			break;

		case 'x':
			m_settings.pdbHeaderReconstructorSettings.showOffsets = !offSwitch;
			break;

		case 'b':
			m_settings.pdbHeaderReconstructorSettings.allowBitFieldsInUnion = !offSwitch;
			break;

		case 'd':
			m_settings.pdbHeaderReconstructorSettings.allowAnonymousDataTypes = !offSwitch;
			break;

		default:
			throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);
		}
	}

	m_headerReconstructor = std::make_unique<PDBHeaderReconstructor>(m_settings.pdbHeaderReconstructorSettings);
	m_symbolVisitor = std::make_unique<PDBSymbolVisitor<UdtFieldDefinition>>(m_headerReconstructor.get());
	m_symbolSorter = std::make_unique<PDBSymbolSorter>();
}

void PDBExtractor::OpenPDBFile()
{
	if (!m_pdb.Open(m_settings.pdbPath))
	{
		throw PDBDumperException(MESSAGE_FILE_NOT_FOUND);
	}
}

void PDBExtractor::PrintPDBDefinitions()
{
	for (const auto& symIndex : m_symbolSorter->GetSortedSymbolIndexes())
	{
		bool expand = true;

		auto symbol = m_pdb.GetSymbolBySymbolIndex(symIndex);
		assert(symbol);

		if (m_settings.pdbHeaderReconstructorSettings.memberStructExpansion ==
		    PDBHeaderReconstructor::MemberStructExpansionType::InlineUnnamed &&
		    symbol->tag == SymTagUDT &&
		    PDB::IsUnnamedSymbol(*symbol))
		{
			expand = false;
		}

		if (expand)
		{
			m_symbolVisitor->Run(*symbol);
		}
	}
}

void PDBExtractor::PrintPDBFunctions()
{
	m_settings.pdbHeaderReconstructorSettings.output.get() << "/*" << std::endl;

	for (const auto& func : m_pdb.GetFunctionSet())
	{
		m_settings.pdbHeaderReconstructorSettings.output.get() << func << std::endl;
	}

	m_settings.pdbHeaderReconstructorSettings.output.get() << "*/" << std::endl;
}

void PDBExtractor::DumpAllSymbols()
{
	for (const auto&[_, symbol] : m_pdb.GetSymbolMap())
	{
		assert(symbol);
		m_symbolSorter->Visit(*symbol);
	}

	PrintPDBDefinitions();
	PrintPDBFunctions();
}
