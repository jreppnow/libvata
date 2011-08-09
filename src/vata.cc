/*****************************************************************************
 *  Vojnar's Army Tree Automata Library
 *
 *  Copyright (c) 2011  Ondra Lengal <ilengal@fit.vutbr.cz>
 *
 *  Description:
 *    The command-line interface to the VATA library.
 *
 *****************************************************************************/

// VATA headers
#include <vata/vata.hh>
#include <vata/bdd_tree_aut.hh>
#include <vata/parsing/timbuk_parser.hh>
#include <vata/serialization/timbuk_serializer.hh>
#include <vata/util/convert.hh>

// Log4cpp headers
#include <log4cpp/Category.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/BasicLayout.hh>

// standard library headers
#include <cstdlib>
#include <fstream>

// local headers
#include "parse_args.hh"


using VATA::BDDTreeAut;
using VATA::Util::Convert;
using VATA::Parsing::AbstrParser;
using VATA::Parsing::TimbukParser;
using VATA::Serialization::AbstrSerializer;
using VATA::Serialization::TimbukSerializer;

const char VATA_USAGE_STRING[] =
	"VATA: Vojnar's Army Tree Automata library interface\n"
	"usage: vata [-r <representation>] [(-I|-O|-F) <format>] [-h|--help] [-t]\n"
	"            <command> [<args>]\n"
	;

const char VATA_USAGE_COMMANDS[] =
	"\nThe following commands are supported:\n"
	"    help                    Display this message\n"
	"    load <file>             Load automaton from <file>\n"
	"    union <file1> <file2>   Compute union of automata from <file1> and <file2>\n"
	"    isect <file1> <file2>   Compute intersection of automata from <file1> and\n"
	"                            <file2>\n"
	;

const char VATA_USAGE_FLAGS[] =
	"\nOptions:\n"
	"    -h, --help              Display this message\n"
	"    -r <representation>     Use <representation> for internal storage of\n"
	"                            automata. The following representations are\n"
	"                            supported:\n"
	"\n"
	"                               'bdd'     : binary decision diagrams\n"
	"    (-I|-O|-F) <format>     Specify format for input (-I), output (-O), or\n"
	"                            both (-F). The following formats are supported:\n"
	"\n"
	"                               'timbuk'  : binary decision diagrams\n"
	"    -t                      Print the time the operation took to error output\n"
	"                            stream\n"
	;


void printHelp(bool full = false)
{
	std::cout << VATA_USAGE_STRING;

	if (full)
	{	// in case full help is wanted
		std::cout << VATA_USAGE_COMMANDS;
		std::cout << VATA_USAGE_FLAGS;
		std::cout << "\n\n";
	}
}


template <class Aut>
std::string performLoad(const Arguments& args, AbstrParser& parser,
	AbstrSerializer& serializer, const std::string str)
{
	Aut aut;
	BDDTreeAut::StringToStateDict stateDict;

	aut.LoadFromString(parser, str, &stateDict);
	return aut.DumpToString(serializer, &stateDict);
}


template <class Aut>
std::string performUnion(const Arguments& args, AbstrParser& parser,
	AbstrSerializer& serializer, const std::string& lhs, const std::string& rhs)
{
	Aut aut1;
	Aut aut2;

	aut1.LoadFromString(parser, lhs);
	aut2.LoadFromString(parser, rhs);

	Aut autRes = Union(aut1, aut2);

	return autRes.DumpToString(serializer);
}


template <class Aut>
std::string performIntersection(const Arguments& args, AbstrParser& parser,
	AbstrSerializer& serializer, const std::string& lhs, const std::string& rhs)
{
	Aut aut1;
	Aut aut2;

	aut1.LoadFromString(parser, lhs);
	aut2.LoadFromString(parser, rhs);

	Aut autRes = Intersection(aut1, aut2);

	return autRes.DumpToString(serializer);
}


std::string readFile(const std::string& fileName)
{
	std::ifstream ifs(fileName.c_str());
	if (!ifs)
	{	// in case the file cannot be open
		throw std::runtime_error("Cannot open file " + fileName);
	}

	std::string result;
	std::string str;
	while (std::getline(ifs, str))
	{
		result += str + "\n";
	}

	return result;
}

template <class Aut>
int performOperation(const Arguments& args, AbstrParser& parser,
	AbstrSerializer& serializer)
{
	Aut autInput1;
	Aut autInput2;
	Aut autResult;

	if (args.operands >= 1)
	{
		autInput1.LoadFromString(parser, readFile(args.fileName1));
	}

	if (args.operands >= 2)
	{
		autInput2.LoadFromString(parser, readFile(args.fileName2));
	}

	// get the start time
	timespec startTime;
	timespec finishTime;
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &startTime))
	{
		throw std::runtime_error("Could not get the start time");
	}

	// process command
	if (args.command == COMMAND_LOAD)
	{
		autResult = autInput1;
	}
	else if (args.command == COMMAND_UNION)
	{
		autResult = Union(autInput1, autInput2);
	}
	else if (args.command == COMMAND_INTERSECTION)
	{
		autResult = Intersection(autInput1, autInput2);
	}
	else
	{
		throw std::runtime_error("Internal error: invalid command");
	}

	// get the finish time
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &finishTime))
	{
		throw std::runtime_error("Could not get the finish time");
	}
	double opTime = (finishTime.tv_sec - startTime.tv_sec)
		+ 1e-9 * (finishTime.tv_nsec - startTime.tv_nsec);

	if (args.showTime)
	{
		std::cerr << opTime << "\n";
	}

	if ((args.command == COMMAND_LOAD) ||
		(args.command == COMMAND_UNION) ||
		(args.command == COMMAND_INTERSECTION) ||
		false)
	{
		std::cout << autResult.DumpToString(serializer);
	}

	return EXIT_SUCCESS;
}


template <class Aut>
int executeCommand(const Arguments& args)
{
	std::auto_ptr<AbstrParser> parser(static_cast<AbstrParser*>(0));
	std::auto_ptr<AbstrSerializer> serializer(static_cast<AbstrSerializer*>(0));

	// create the input parser
	if (args.inputFormat == FORMAT_TIMBUK)
	{
		parser.reset(new TimbukParser());
	}
	else
	{
		throw std::runtime_error("Internal error: invalid input format");
	}

	// create the output serializer
	if (args.outputFormat == FORMAT_TIMBUK)
	{
		serializer.reset(new TimbukSerializer());
	}
	else
	{
		throw std::runtime_error("Internal error: invalid output format");
	}

	return performOperation<Aut>(args, *(parser.get()), *(serializer.get()));
}


void setUpLogging()
{
	// Create the appender
	log4cpp::Appender* app1  = new log4cpp::OstreamAppender("ClogAppender", &std::clog);

	std::string cat_name = "VATA";

	log4cpp::Category::getInstance(cat_name).setAdditivity(false);
	log4cpp::Category::getInstance(cat_name).addAppender(app1);
	log4cpp::Category::getInstance(cat_name).setPriority(log4cpp::Priority::INFO);
}


int main(int argc, char* argv[])
{
	// Assertions
	assert(argc > 0);
	assert(argv != static_cast<char**>(0));

	// start logging
	setUpLogging();

	if (argc == 1)
	{	// in case no arguments were given
		printHelp(true);
		return EXIT_SUCCESS;
	}

	--argc;
	++argv;
	Arguments args;

	try
	{
		args = parseArguments(argc, argv);
	}
	catch (const std::exception& ex)
	{
		std::cerr << "An error occured while parsing arguments: "
			<< ex.what() << "\n";
		printHelp(false);

		return EXIT_FAILURE;
	}

	if (args.command == COMMAND_HELP)
	{
		printHelp(true);
		return EXIT_SUCCESS;
	}

	if (args.representation == REPRESENTATION_BDD)
	{
		try
		{
			return executeCommand<BDDTreeAut>(args);
		}
		catch (std::exception& ex)
		{
			std::cerr << "An error occured: " << ex.what() << "\n";
			return EXIT_FAILURE;
		}
	}
	else
	{
		std::cerr << "Internal error: invalid representation\n";
		return EXIT_FAILURE;
	}
}