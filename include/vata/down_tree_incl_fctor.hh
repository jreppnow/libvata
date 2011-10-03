/*****************************************************************************
 *  Vojnar's Army Tree Automata Library
 *
 *  Copyright (c) 2011  Ondra Lengal <ilengal@fit.vutbr.cz>
 *
 *  Description:
 *    Header file with the downward tree automata language inclusion
 *    checking functor (assuming that the automata do not have useless states).
 *
 *****************************************************************************/

#ifndef _VATA_DOWN_TREE_INCL_FCTOR_HH_
#define _VATA_DOWN_TREE_INCL_FCTOR_HH_

// VATA headers
#include <vata/vata.hh>
#include <vata/util/antichain2c_v2.hh>


namespace VATA
{
	template <class Aut, class Rel>
	class DownwardInclusionFunctor;
}

// NOTE: the automata cannot have useless states
template <class Aut, class Rel>
class VATA::DownwardInclusionFunctor
{
public:   // data types

	typedef typename Aut::StateType StateType;
	typedef typename VATA::Util::OrdVector<StateType> StateSet;
	typedef typename Aut::StateTuple StateTuple;
	typedef typename Aut::DownInclStateTupleSet StateTupleSet;
	typedef typename Aut::DownInclStateTupleVector StateTupleVector;

	typedef std::pair<StateType, StateSet> WorkSetElement;

	typedef std::unordered_multimap<typename WorkSetElement::first_type,
		typename WorkSetElement::second_type> WorkSetType;

	typedef Rel Relation;
	typedef typename Relation::IndexType IndexType;

private:  // data types

	typedef std::pair<StateType, StateSet> ACPair;

	class InclACComparer
	{
	private:  // data members

	public:   // methods
		bool operator()(const ACPair& lhs, const ACPair& rhs)
		{
			return lhs.second.IsSubsetOf(rhs.second);
		}
	};

	class InclACComparerStrict
	{
	public:   // methods
		bool operator()(const ACPair& lhs, const ACPair& rhs)
		{
			return lhs.second.IsSubsetOf(rhs.second) &&
				lhs.second.size() < rhs.second.size();
		}
	};

	class NonInclACComparer
	{
	public:   // methods
		bool operator()(const ACPair& lhs, const ACPair& rhs)
		{
			return rhs.second.IsSubsetOf(lhs.second);
		}
	};

	class NonInclACComparerStrict
	{
	public:
		bool operator()(const ACPair& lhs, const ACPair& rhs)
		{
			return rhs.second.IsSubsetOf(lhs.second) &&
				rhs.second.size() < lhs.second.size();
		}
	};

	typedef VATA::Util::Convert Convert;

public:   // data types

	typedef VATA::Util::Antichain2Cv2
	<
		StateType,
		StateSet
	> InclAntichainType;

	typedef VATA::Util::Antichain2Cv2
	<
		StateType,
		StateSet
	> NonInclAntichainType;

	class SetComparerSmaller
	{
	private:  // data members

		const Relation& preorder_;

	public:   // methods

		SetComparerSmaller(const Relation& preorder) :
			preorder_(preorder)
		{ }

		// TODO: this could be done better
		bool operator()(const StateSet& lhs, const StateSet& rhs) const
		{
			for (const StateType& rhsState : rhs)
			{
				bool found = false;
				for (const StateType& lhsState : lhs)
				{
					if (preorder_.get(rhsState, lhsState))
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					return false;
				}
			}

			return true;
		}
	};

	class SetComparerBigger
	{
	private:  // data members

		const Relation& preorder_;

	public:   // methods

		SetComparerBigger(const Relation& preorder) :
			preorder_(preorder)
		{ }

		// TODO: this could be done better
		bool operator()(const StateSet& lhs, const StateSet& rhs) const
		{
			for (const StateType& lhsState : lhs)
			{
				bool found = false;
				for (const StateType& rhsState : rhs)
				{
					if (preorder_.get(lhsState, rhsState))
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					return false;
				}
			}

			return true;
		}
	};

private:  // data types

	typedef std::vector<unsigned> ChoiceFunctionType;

	class SequentialChoiceFunctionGenerator
	{
	private:  // data types

		typedef ChoiceFunctionType::value_type RangeType;

	private:  // data members

		ChoiceFunctionType currentCf_;

		RangeType range_;
		char lastCnt_;

	public:   // methods

		SequentialChoiceFunctionGenerator(size_t length, RangeType range) :
			currentCf_(length, range-1),
			range_(range),
			lastCnt_(range == 0? 0 : 2)   // 2 because the initial GetNext() decrements it
		{
			// Assertions
			assert(!currentCf_.empty());
		}

		inline const ChoiceFunctionType& GetNext()
		{
			// Assertions
			assert(!IsLast());

			// move to the next choice function
			size_t index = 0;
			while (++currentCf_[index] == range_) {
				currentCf_[index] = 0; // reset this counter
				++index;               // move to the next counter

				if (index == currentCf_.size()) {
					// if we drop out from the n-tuple
					--lastCnt_;
					break;
				}
			}

			return currentCf_;
		}

		inline bool IsLast() const
		{
			return lastCnt_ == 0;
		}
	};

	typedef SequentialChoiceFunctionGenerator ChoiceFunctionGenerator;

private:  // data members

	const Aut& smaller_;
	const Aut& bigger_;

	bool processingStopped_;
	bool inclusionHolds_;

	WorkSetType& workset_;

	NonInclAntichainType& nonIncl_;

	InclAntichainType childrenCache_;

	const Relation& preorder_;

	const IndexType& preorderSmaller_;
	const IndexType& preorderBigger_;

	const SetComparerSmaller& smallerComparer_;
	const SetComparerBigger& biggerComparer_;

private:  // methods

	bool expand(const StateType& smallerState, const StateSet& biggerStateSet)
	{
		auto key = std::make_pair(smallerState, biggerStateSet);

		if (isInWorkset(key))
		{	// in case we returned somewhere we already know
			return true;
		}
		else if (isNoninclusionImplied(key))
		{	// in case we know that the inclusion does not hold
			return false;
		}
		else if (isImpliedByChildren(key))
		{
			return true;
		}
		else if (isImpliedByPreorder(key))
		{
			return true;
		}

		workset_.insert(key);

		DownwardInclusionFunctor innerFctor(*this);
		Aut::ForeachDownSymbolFromStateAndStateSetDo(smaller_, bigger_,
			smallerState, biggerStateSet, innerFctor);

		// erase the element
		bool erased = false;
		for (auto keyRange = workset_.equal_range(smallerState);
			keyRange.first != keyRange.second; ++(keyRange.first))
		{	// for all items with proper key
			if (biggerStateSet == (keyRange.first)->second)
			{	// if we found what we were looking for
				workset_.erase(keyRange.first);
				erased = true;
				break;
			}
		}

		// make sure the element was removed
		if (!erased)
		{
			assert(false);       // fail gracefully
		}

		// cache the result
		if (innerFctor.InclusionHolds())
		{
			processFoundInclusion(smallerState, biggerStateSet);
		}
		else
		{
			processFoundNoninclusion(smallerState, biggerStateSet);
		}

		return innerFctor.InclusionHolds();
	}

	inline void failProcessing()
	{
		inclusionHolds_ = false;
		processingStopped_ = true;
	}

	// workset antichain
	inline bool isInWorkset(const WorkSetElement& elem) const
	{
		for (auto stateSetPair : workset_)
		{
			if (preorder_.get(elem.first, stateSetPair.first))
			{	// if the pair is worth processing
				if (smallerComparer_(stateSetPair.second, elem.second))
				{
					return true;
				}
			}
		}

		return false;
	}

	inline bool isImpliedByChildren(const WorkSetElement& elem) const
	{
		return childrenCache_.contains(preorderBigger_[elem.first],
			elem.second, smallerComparer_);
	}

	inline bool isNoninclusionImplied(const WorkSetElement& elem) const
	{
		return nonIncl_.contains(preorderSmaller_[elem.first],
			elem.second, biggerComparer_);
	}

	inline bool isImpliedByPreorder(const WorkSetElement& elem) const
	{
		for (const StateType& biggerState : elem.second)
		{
			if (preorder_.get(elem.first, biggerState))
			{
				return true;
			}
		}

		return false;
	}

	inline void processFoundInclusion(const StateType& smallerState,
		const StateSet& biggerStateSet)
	{
		childrenCache_.refine(preorderSmaller_[smallerState], biggerStateSet,
			smallerComparer_);
		childrenCache_.insert(smallerState, biggerStateSet);
	}

	inline void processFoundNoninclusion(const StateType& smallerState,
		const StateSet& biggerStateSet)
	{
		nonIncl_.refine(preorderBigger_[smallerState], biggerStateSet,
			biggerComparer_);
		nonIncl_.insert(smallerState, biggerStateSet);
	}

public:   // methods

	DownwardInclusionFunctor(const Aut& smaller, const Aut& bigger,
		WorkSetType& workset, NonInclAntichainType& nonIncl,
		const Relation& preorder, const IndexType& preorderSmaller,
		const IndexType& preorderBigger,
		const SetComparerSmaller& smallerComparer,
		const SetComparerBigger& biggerComparer) :
		smaller_(smaller),
		bigger_(bigger),
		processingStopped_(false),
		inclusionHolds_(true),
		workset_(workset),
		nonIncl_(nonIncl),
		childrenCache_(),
		preorder_(preorder),
		preorderSmaller_(preorderSmaller),
		preorderBigger_(preorderBigger),
		smallerComparer_(smallerComparer),
		biggerComparer_(biggerComparer)
	{ }

	DownwardInclusionFunctor(
		DownwardInclusionFunctor& downFctor) :
		smaller_(downFctor.smaller_),
		bigger_(downFctor.bigger_),
		processingStopped_(false),
		inclusionHolds_(true),
		workset_(downFctor.workset_),
		nonIncl_(downFctor.nonIncl_),
		childrenCache_(),
		preorder_(downFctor.preorder_),
		preorderSmaller_(downFctor.preorderSmaller_),
		preorderBigger_(downFctor.preorderBigger_),
		smallerComparer_(downFctor.smallerComparer_),
		biggerComparer_(downFctor.biggerComparer_)
	{ }

	template <class ElementAccessorLHS, class ElementAccessorRHS>
	void operator()(const StateTupleSet& lhs, ElementAccessorLHS lhsElemAccess,
		const StateTupleSet& rhs, ElementAccessorRHS rhsElemAccess)
	{
		if (lhs.empty())
		{	// in case there is nothing in LHS the inclusion simply holds
			return;
		}

		// arity of the symbol
		const size_t arity = lhsElemAccess(*lhs.begin()).size();

		if (arity == 0)
		{	// in case LHS represents a nullary transition (i.e., a leaf symbol)
			assert(lhs.size() == 1);

			if (!rhs.empty())
			{	// in case there also a nullary transition in the RHS
				assert(rhs.size() == 1);
				assert(rhsElemAccess(*rhs.begin()).size() == 0);
				return;
			}
			else
			{	// in case RHS cannot make this transition
				failProcessing();
				return;
			}
		}
		else
		{	// in case the transition si not nullary
			if (rhs.empty())
			{	// in case RHS is empty
				failProcessing();
				return;
			}

			for (auto lhsTupleCont : lhs)
			{
				const StateTuple& lhsTuple = lhsElemAccess(lhsTupleCont);

				// Assertions
				assert(lhsTuple.size() == arity);

				// first check whether there is a bigger tuple
				bool valid = false;
				for (auto rhsTupleCont : rhs)
				{
					const StateTuple& rhsTuple = rhsElemAccess(rhsTupleCont);

					valid = true;
					for (size_t i = 0; i < arity; ++i)
					{
						if (!expand(lhsTuple[i], StateSet(rhsTuple[i])))
						{
							valid = false;
							break;
						}
					}

					if (valid)
					{
						break;
					}
				}

				if (valid)
				{	// in case there was a bigger tuple
					continue;
				}

				// in case there is not a bigger tuple

				// TODO: could be done more smartly (without conversion to vector)
				StateTupleVector rhsVector = Aut::StateTupleSetToVector(rhs);

				ChoiceFunctionGenerator cfGen(rhsVector.size(), lhsTuple.size());
				while (!cfGen.IsLast())
				{	// for each choice function
					const ChoiceFunctionType& cf = cfGen.GetNext();
					bool found = false;

					for (size_t tuplePos = 0; tuplePos < arity; ++tuplePos)
					{ // for each position of the n-tuple
						StateSet rhsSetForTuplePos;

						for (size_t cfIndex = 0; cfIndex < cf.size(); ++cfIndex)
						{	// for each element in the choice function
							if (cf[cfIndex] == tuplePos)
							{ // in case the choice function for given vector is at
								// current position in the tuple
								assert(cfIndex < rhsVector.size());
								const StateTuple& rhsTuple = rhsElemAccess(rhsVector[cfIndex]);
								assert(rhsTuple.size() == arity);

								// insert tuplePos-th state of the cfIndex-th tuple in the
								// RHS into the set
								rhsSetForTuplePos.insert(rhsTuple[tuplePos]);
							}
						}

						if (rhsSetForTuplePos.empty())
						{	// in case the right-hand side set is empty, we exploit the
							// fact that there are no useless states in any of the automata
							continue;
						}

						if (expand(lhsTuple[tuplePos], rhsSetForTuplePos))
						{	// in case inclusion holds for this case
							found = true;
							break;
						}
					}

					if (!found)
					{	// in case the inclusion does not hold
						failProcessing();
						return;
					}
				}
			}
		}
	}

	inline bool IsProcessingStopped() const
	{
		return processingStopped_;
	}

	inline bool InclusionHolds() const
	{
		return inclusionHolds_;
	}

	inline void Reset()
	{
		inclusionHolds_ = true;
		processingStopped_ = false;
	}
};

#endif
