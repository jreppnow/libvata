// example2.cc - operations with an automaton

// VATA headers
#include <vata/bdd_bu_tree_aut.hh>
#include <vata/bdd_bu_tree_aut_op.hh>
#include <vata/bdd_td_tree_aut.hh>
#include <vata/bdd_td_tree_aut_op.hh>
#include <vata/explicit_tree_aut.hh>
#include <vata/explicit_tree_aut_op.hh>
#include <vata/parsing/timbuk_parser.hh>
#include <vata/serialization/timbuk_serializer.hh>


/*
 * accepts
 *
 *     b         b
 *    / \       / \
 *   a   b   , a   a
 *  / \
 * a   a   
 *
 */
const char* aut1Str =
	"Ops           a:0 b:2\n"
	"Automaton     aut\n"
	"States        q0 q1 q2\n"
	"Final States  q1 q2\n"
	"Transitions\n"
	"a          -> q0\n"
	"b(q0, q0)  -> q1\n"
	"b(q1, q0)  -> q2\n";


/*
 * accepts
 *
 *    b         b
 *   / \       / \
 *  a   b   , a   a
 *     / \
 *    a   a
 *
 */
const char* aut2Str =
	"Ops           a:0 b:2\n"
	"Automaton     aut\n"
	"States        q0 q1 q2\n"
	"Final States  q1 q2\n"
	"Transitions\n"
	"a          -> q0\n"
	"b(q0, q0)  -> q1\n"
	"b(q0, q1)  -> q2\n";


typedef VATA::ExplicitTreeAut<unsigned> Automaton;
//typedef VATA::BDDBottomUpTreeAut Automaton;  // uncomment for BDD BU automaton
//typedef VATA::BDDTopDownTreeAut Automaton;   // uncomment for BDD TD automaton

int main()
{
	// create the parser for the Timbuk format
	std::unique_ptr<VATA::Parsing::AbstrParser> parser(
		new VATA::Parsing::TimbukParser());

	// create the dictionary translating symbol names to internal symbols ...
	Automaton::StringToSymbolDict symbolDict;
	// ... and link it to the automaton
	Automaton::SetSymbolDictPtr(&symbolDict);

	// create the ``next symbol'' variable for the automaton
	Automaton::SymbolType nextSymbol(0);
	//Automaton::SymbolType nextSymbol(16, 0);  // uncomment for BDD aut
	Automaton::SetNextSymbolPtr(&nextSymbol);

	// create the dictionaries for translating state names to internal state numbers
	VATA::AutBase::StringToStateDict stateDict1;
	VATA::AutBase::StringToStateDict stateDict2;

	// create and load the first automaton
	Automaton aut1;
	aut1.LoadFromString(*parser, aut1Str, stateDict1);

	// create and load the second automaton
	Automaton aut2;
	aut2.LoadFromString(*parser, aut2Str, stateDict2);

	// create the serializer for the Timbuk format
	VATA::Serialization::AbstrSerializer* serializer =
		new VATA::Serialization::TimbukSerializer();

	// compute the union automaton
	Automaton::StateToStateMap stateTranslMap1;
	Automaton::StateToStateMap stateTranslMap2;
	Automaton autUnion = Union(aut1, aut2, &stateTranslMap1, &stateTranslMap2);
	VATA::AutBase::StringToStateDict stateDictUnion =
		VATA::Util::CreateUnionStringToStateMap(stateDict1, stateDict2,
		&stateTranslMap1, &stateTranslMap2);

	// dump the union automaton
	std::cout << "Union\n=====\n";
	std::cout << autUnion.DumpToString(*serializer, stateDictUnion) << "\n";

	// compute the intersection automaton
	Automaton::ProductTranslMap prodTranslMap;
	Automaton autIsect = Intersection(aut1, aut2, &prodTranslMap);
	VATA::AutBase::StringToStateDict stateDictIsect =
		VATA::Util::CreateProductStringToStateMap(stateDict1, stateDict2,
		prodTranslMap);

	// dump the intersection automaton
	std::cout << "Intersection\n============\n";
	std::cout << autIsect.DumpToString(*serializer, stateDictIsect) << "\n";

	// check inclusions
	std::cout << "intersection <= union: " << CheckDownwardInclusion(autIsect, autUnion) << "\n";
	std::cout << "union <= intersection: " << CheckDownwardInclusion(autUnion, autIsect) << "\n";
}