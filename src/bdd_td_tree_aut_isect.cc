/*****************************************************************************
 *  Vojnar's Army Tree Automata Library
 *
 *  Copyright (c) 2011  Ondra Lengal <ilengal@fit.vutbr.cz>
 *
 *  Description:
 *    Implementation of intersection on BDD top-down tree automata.
 *
 *****************************************************************************/

// VATA headers
#include <vata/vata.hh>
#include <vata/bdd_td_tree_aut_op.hh>

using VATA::AutBase;
using VATA::BDDTopDownTreeAut;

// Standard library headers
#include <unordered_map>

BDDTopDownTreeAut VATA::Intersection(
	const BDDTopDownTreeAut& lhs, const BDDTopDownTreeAut& rhs,
	AutBase::ProductTranslMap* pTranslMap)
{
	// Assertions
	assert(lhs.isValid());
	assert(rhs.isValid());

	typedef BDDTopDownTreeAut::StateType StateType;
	typedef BDDTopDownTreeAut::StateSet StateSet;
	typedef BDDTopDownTreeAut::StateTuple StateTuple;
	typedef BDDTopDownTreeAut::StateTupleSet StateTupleSet;
	typedef std::pair<StateType, StateType> StatePair;
	typedef std::map<StateType, StatePair> WorkSetType;
	typedef AutBase::ProductTranslMap IntersectionTranslMap;

	GCC_DIAG_OFF(effc++)  // suppress non-virtual destructor warning
	class IntersectionApplyFunctor :
		public VATA::MTBDDPkg::Apply2Functor<IntersectionApplyFunctor, StateTupleSet,
		StateTupleSet, StateTupleSet>
	{
	GCC_DIAG_ON(effc++)
	private:  // data members

		BDDTopDownTreeAut& resultAut_;
		IntersectionTranslMap& translMap_;
		WorkSetType& workset_;

	public:   // methods

		IntersectionApplyFunctor(BDDTopDownTreeAut& resultAut,
			IntersectionTranslMap& translMap, WorkSetType& workset) :
			resultAut_(resultAut),
			translMap_(translMap),
			workset_(workset)
		{ }

		StateTupleSet ApplyOperation(const StateTupleSet& lhs,
			const StateTupleSet& rhs)
		{
			StateTupleSet result;

			for (StateTupleSet::const_iterator itLhs = lhs.begin();
				itLhs != lhs.end(); ++itLhs)
			{	// for each tuple from LHS
				for (StateTupleSet::const_iterator itRhs = rhs.begin();
					itRhs != rhs.end(); ++itRhs)
				{	// for each tuple from RHS
					assert(itLhs->size() == itRhs->size());

					StateTuple resultTuple;
					for (size_t i = 0; i < itLhs->size(); ++i)
					{	// for each position in the tuples
						StatePair newPair = std::make_pair((*itLhs)[i], (*itRhs)[i]);

						StateType state;
						IntersectionTranslMap::const_iterator itTransl;
						if ((itTransl = translMap_.find(newPair)) != translMap_.end())
						{	// if the pair is already known
							state = itTransl->second;
						}
						else
						{	// if the pair is new
							state = resultAut_.AddState();
							translMap_.insert(std::make_pair(newPair, state));
							workset_.insert(std::make_pair(state, newPair));
						}

						resultTuple.push_back(state);
					}

					result.insert(resultTuple);
				}
			}

			return result;
		}
	};

	IntersectionTranslMap translMap;
	if (pTranslMap == nullptr)
	{	// if no translation map was given
		pTranslMap = &translMap;
	}

	BDDTopDownTreeAut result;
	WorkSetType workset;


	for (StateSet::const_iterator itFstLhs = lhs.GetFinalStates().begin();
		itFstLhs != lhs.GetFinalStates().end(); ++itFstLhs)
	{	// iterate over LHS's final states
		for (StateSet::const_iterator itFstRhs = rhs.GetFinalStates().begin();
			itFstRhs != rhs.GetFinalStates().end(); ++itFstRhs)
		{	// iterate over RHS's final states
			StatePair origStates = std::make_pair(*itFstLhs, *itFstRhs);

			StateType newState = result.AddState();
			result.SetStateFinal(newState);

			workset.insert(std::make_pair(newState, origStates));
			pTranslMap->insert(std::make_pair(origStates, newState));
		}
	}

	IntersectionApplyFunctor isect(result, *pTranslMap, workset);

	while (!workset.empty())
	{	// while there is something in the workset
		WorkSetType::iterator itWs = workset.begin();
		const StatePair& procPair  = itWs->second;
		const StateType& procState = itWs->first;

		BDDTopDownTreeAut::TransMTBDD mtbdd = isect(lhs.getMtbdd(procPair.first),
			rhs.getMtbdd(procPair.second));

		result.setMtbdd(procState, mtbdd);

		// remove the processed state from the workset
		workset.erase(itWs);
	}

	assert(result.isValid());

	return result;
}
