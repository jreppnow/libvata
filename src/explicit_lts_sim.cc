/*****************************************************************************
 *  Vojnar's Army Tree Automata Library
 *
 *  Copyright (c) 2011  Jiri Simacek <isimacek@fit.vutbr.cz>
 *
 *  Description:
 *    Source for explicit LTS simulation algorithm.
 *
 *****************************************************************************/

#include <ostream>
#include <vector>
#include <algorithm>
#include <memory>

#include <vata/util/binary_relation.hh>
#include <vata/util/smart_set.hh>
#include <vata/util/caching_allocator.hh>
#include <vata/util/shared_list.hh>
#include <vata/explicit_lts.hh>

using VATA::Util::BinaryRelation;
using VATA::Util::SmartSet;
using VATA::Util::CachingAllocator;
using VATA::Util::SharedList;

typedef CachingAllocator<std::vector<size_t>> VectorAllocator;

struct SharedListInitF {

	VectorAllocator& allocator_;

	SharedListInitF(VectorAllocator& allocator) : allocator_(allocator) {}

	void operator()(SharedList<std::vector<size_t>>* list) {

		auto sublist = this->allocator_();

		sublist->clear();

		list->init(sublist);

	}

};

class SharedCounter {

	const std::vector<size_t>& key_;
	const size_t& states_;
	const std::vector<size_t>& range_;

	std::vector<VectorAllocator::Ptr> data_;

	VectorAllocator& vectorAllocator_;

protected:

	VectorAllocator::Ptr allocateEmptyData(size_t size) {

		auto data = this->vectorAllocator_();

		assert(data);

		data->resize(size);
		
		std::fill(data->begin(), data->end(), 0);

		return data;
		
	}

	SharedCounter& operator=(const SharedCounter& rhs);

public:

	SharedCounter(size_t labels, const std::vector<size_t>& key, const size_t& states,
		const std::vector<size_t>& range, VectorAllocator& vectorAllocator) : key_(key),
		states_(states), range_(range), data_(labels), vectorAllocator_(vectorAllocator) {}

	SharedCounter(SharedCounter& counter) : key_(counter.key_), states_(counter.states_),
		range_(counter.range_), data_(counter.data_.size()),
		vectorAllocator_(counter.vectorAllocator_) {}

	void incr(size_t label, size_t state) {

		assert(label < this->data_.size());

		auto& row = this->data_[label];

		if (row) {

			assert((*row)[0] == 1); // refCount
			assert(2 + this->key_[label*this->states_ + state] < row->size());
	
			++(*row)[1]; // master;
			++(*row)[2 + this->key_[label*this->states_ + state]];

			return;
	
		}

		row = this->vectorAllocator_();
		row->resize(2 + this->range_[label]);
		
		assert(2 + this->key_[label*this->states_ + state] < row->size());

		(*row)[0] = 1; // refCount
		(*row)[1] = 1; // master

		std::fill(row->begin() + 2, row->end(), 0);

		(*row)[2 + this->key_[label*this->states_ + state]] = 1;

	} 
	
	size_t decr(size_t label, size_t state) {

		assert(label < this->data_.size());

		auto*& row = this->data_[label];

		assert(row);
		assert(2 + this->key_[label*this->states_ + state] < row->size());

		if ((*row)[1] == 1) { // master

			assert((*row)[2 + this->key_[label*this->states_ + state]] == 1);

			if ((*row)[0] == 1) { // refCount

				this->vectorAllocator_.reclaim(row);

			} else {
				
				--(*row)[0]; // refCount

			}

			assert(!(row = nullptr));

			return 0;

		}

		if ((*row)[0] > 1) { // refCount

			--(*row)[0]; // refCount

			auto newRow = this->vectorAllocator_();

			newRow->resize(row->size());

			(*newRow)[0] = 1; // refCount

			std::copy(row->begin() + 1, row->end(), newRow->begin() + 1);

			row = newRow;

		}

		--(*row)[1]; // master

		return --(*row)[2 + this->key_[label*this->states_ + state]];

	}

	void copyRow(size_t label, SharedCounter& cnt) {

		assert(label < this->data_.size());
		assert(this->data_.size() == cnt.data_.size());
		assert(cnt.data_[label]);
		assert(this->data_[label] == nullptr);

		++(*cnt.data_[label])[0]; // refCount

		this->data_[label] = cnt.data_[label];

	}

	friend std::ostream& operator<<(std::ostream& os, const SharedCounter& cnt) {

		size_t i = 0;
		
		for (auto& row : cnt.data_) {
			
			if (row) {

				os << i << ": ";
	
				for (auto& col : *row)
					os << col;
	
				os << std::endl;

			}

			++i;

		}

		return os;

	}

};

struct StateListElem {
	
	size_t index_;
	class OLRTBlock* block_;
	StateListElem* next_;
	StateListElem* prev_;

	void move(StateListElem*& src, StateListElem*& dst) {

		assert(src);

		if (this == this->next_)

			src = nullptr;

		else {

			src = this->next_;

			this->next_->prev_ = this->prev_;
			this->prev_->next_ = this->next_;

		}

		if (!dst) {

			dst = this;

			this->next_ = this;
			this->prev_ = this;

		} else {

			this->next_ = dst;
			this->prev_ = dst->prev_;
			this->next_->prev_ = this;
			this->prev_->next_ = this;

		}

	}

};

typedef SharedList<std::vector<size_t>> RemoveList;
typedef CachingAllocator<RemoveList, SharedListInitF> RemoveAllocator;

struct OLRTBlock {

	size_t _index;
	StateListElem* _states;
	std::vector<RemoveList*> _remove;
	SharedCounter _counter;
	SmartSet _inset;
	StateListElem* _tmp;

protected:

	OLRTBlock(const OLRTBlock&);

	OLRTBlock& operator=(const OLRTBlock&);

public:

	OLRTBlock(const VATA::ExplicitLTS& lts, size_t index, StateListElem* states,
		const std::vector<size_t>& key, const std::vector<size_t>& range,
		VectorAllocator& vectorAllocator) : _index(index), _states(states), _remove(lts.labels()),
		_counter(lts.labels(), key, lts.states(), range, vectorAllocator), _inset(lts.labels()),
		_tmp(nullptr) {

		for (size_t q = 0; q < lts.states(); ++q) {

			for (auto& a : lts.bwLabels(q))
				this->_inset.add(a);

		}

	}
	
	OLRTBlock(const VATA::ExplicitLTS& lts, OLRTBlock& parent, size_t index) : _index(index),
		_states(parent._tmp), _remove(lts.labels()), _counter(parent._counter),
		_inset(lts.labels()), _tmp(nullptr) {

		assert(this->_states);

		parent._tmp = nullptr;

		StateListElem* elem = this->_states;

		do {

			for (auto& a : lts.bwLabels(elem->index_)) {

				parent._inset.removeStrict(a);

				this->_inset.add(a);

			}

			elem->block_ = this;
			elem = elem->next_;

		} while (elem != this->_states);

	}
	
	StateListElem* states() {
		return this->_states;
	}
	
	StateListElem* tmp() {
		return this->_tmp;
	}

	void moveToTmp(StateListElem& elem) {

		elem.move(this->_states, this->_tmp);	

	}
	
	bool checkEmpty() {

		if (this->_states)
			return false;

		this->_states = this->_tmp;
		this->_tmp = nullptr;

		return true;

	}
	
	SharedCounter& counter() {
		return this->_counter;
	}

	SmartSet& inset() {
		return this->_inset;
	}

	size_t index() const {
		return this->_index;
	}
	
	friend std::ostream& operator<<(std::ostream& os, const OLRTBlock& block) {

		assert(block._states);

		os << block._index << " (";

		const StateListElem* elem = block._states;

		do {

			os << " " << elem->index_;

			elem = elem->next_;

		} while (elem != block._states);

		return os << " )";

	}
	
};

class OLRTAlgorithm {

	const VATA::ExplicitLTS& _lts;

	VectorAllocator vectorAllocator_;
	RemoveAllocator removeAllocator_;

	std::vector<OLRTBlock*> _partition;
	BinaryRelation _relation;
	std::vector<StateListElem> _index;
	std::vector<std::pair<OLRTBlock*, size_t> > _queue;
	std::vector<SmartSet> _delta;
	std::vector<SmartSet> _delta1;
	std::vector<size_t> _key;
	std::vector<size_t> _range;

	OLRTAlgorithm(const OLRTAlgorithm&);

	OLRTAlgorithm& operator=(const OLRTAlgorithm&);

protected:

	void enqueueToRemove(OLRTBlock* block, size_t label, size_t state) {

		if (RemoveList::append(block->_remove[label], state, this->removeAllocator_))
			this->_queue.push_back(std::make_pair(block, label));

	}

	template <class T>
	void buildPre(T& pre, StateListElem* states, size_t label) const {

		std::vector<bool> blockMask(this->_partition.size(), false);

		StateListElem* elem = states;

		do {

			for (auto& q : this->_lts.pre(label)[elem->index_]) {

				OLRTBlock* block = this->_index[q].block_;

				assert(block);

				if (blockMask[block->index()])
					continue;

				blockMask[block->index()] = true;

				pre.push_back(block);

			}

			elem = elem->next_;

		} while (elem != states);

	}

	template <class T1, class T2>
	void internalSplit(T1& modifiedBlocks, const T2& remove) {

		std::vector<bool> blockMask(this->_partition.size(), false);

		for (auto& q : remove) {

			assert(q < this->_index.size());

			StateListElem& elem = this->_index[q];

			OLRTBlock* block = elem.block_;

			assert(block);

			block->moveToTmp(elem);

			assert(block->index() < this->_partition.size());

			if (blockMask[block->index()])
				continue;

			blockMask[block->index()] = true;;

			modifiedBlocks.push_back(block);

		}

	}

	template <class T>
	void fastSplit(const T& remove) {

		std::vector<OLRTBlock*> modifiedBlocks;

		this->internalSplit(modifiedBlocks, remove);

		for (auto& block : modifiedBlocks) {

			block->checkEmpty();

			if (!block->tmp())
				continue;

			size_t newIndex = this->_relation.split(block->index(), true);

			this->_partition.push_back(new OLRTBlock(this->_lts, *block, newIndex));

		}

	}

	template <class T1, class T2>
	void split(T1& removeList, const T2& remove) {

		std::vector<OLRTBlock*> modifiedBlocks;

		this->internalSplit(modifiedBlocks, remove);

		for (auto& block : modifiedBlocks) {

			if (block->checkEmpty()) {

				removeList.push_back(block);

				continue;

			}

			assert(block->tmp());

			size_t newIndex = this->_relation.split(block->index(), true);

			OLRTBlock* newBlock = new OLRTBlock(this->_lts, *block, newIndex);

			this->_partition.push_back(newBlock);

			removeList.push_back(newBlock);

			for (auto& a : newBlock->inset()) {

				newBlock->counter().copyRow(a, block->counter());

				if (block->_remove[a]) {

					this->_queue.push_back(std::make_pair(newBlock, a));

					newBlock->_remove[a] = block->_remove[a]->copy();

				}

			}

		}

	}

	template <class T>
	void makeBlock(const T& states, size_t blockIndex) {

		assert(states.size() > 0);

		OLRTBlock* block = this->_index[states.front()].block_;

		for (auto& q : states) {

			StateListElem& elem = this->_index[q];

			assert(block == elem.block_);
			assert(block->states());

			block->moveToTmp(elem);

		}

		assert(block->states());
		assert(block->tmp());

		this->_partition.push_back(new OLRTBlock(this->_lts, *block, blockIndex));

	}

	void processRemove(OLRTBlock* block, size_t label) {

		assert(block);

		RemoveList* remove = block->_remove[label];

		block->_remove[label] = nullptr;

		assert(remove);

		std::vector<OLRTBlock*> preList, removeList;

		this->buildPre(preList, block->states(), label);

		this->split(removeList, *remove);

		remove->unsafeRelease(
			[this](RemoveList* list){
					this->vectorAllocator_.reclaim(list->subList());
					this->removeAllocator_.reclaim(list);
			}
		);

		for (auto& b1 : preList) {

			for (auto& b2 : removeList) {

				assert(b1->index() != b2->index());

				if (!this->_relation.get(b1->index(), b2->index()))
					continue;

				this->_relation.set(b1->index(), b2->index(), false);

				for (auto a : b2->inset()) {

					if (!b1->inset().contains(a))
						continue;

					StateListElem* elem2 = b2->states();

					do {

						for (auto& pre2 : this->_lts.pre(a)[elem2->index_]) {

							if (!b1->counter().decr(a, pre2))
								this->enqueueToRemove(b1, a, pre2);

						}

						elem2 = elem2->next_;

					} while (elem2 != b2->states());

				}

			}

		}

	}

	bool static isPartition(const std::vector<std::vector<size_t>>& part, size_t states) {
	
		std::vector<bool> mask(states, false);
	
		for (auto& cls : part) {
	
			for (auto& q : cls) {
	
				if (mask[q])
					return false;
	
				mask[q] = true;
	
			}
	
		}
	
		return true;
	
	}

	bool isConsistent() const {

		if (this->_partition.size() != this->_relation.size())
			return false;

		for (size_t i = 0; i < this->_partition.size(); ++i) {

			if (!this->_relation.get(i, i))
				return false;

		}

		return true;

	}

public:

	OLRTAlgorithm(const VATA::ExplicitLTS& lts) : _lts(lts), vectorAllocator_(),
		removeAllocator_(SharedListInitF(vectorAllocator_)), _partition(), _relation(),
		_index(lts.states()), _queue(), _delta(), _delta1(), _key(), _range() {

		assert(this->_index.size());

		OLRTBlock* block = new OLRTBlock(
			lts, 0, &this->_index[0], this->_key, this->_range, this->vectorAllocator_
		);

		size_t q = this->_index.size();

		for (auto& state : this->_index) {

			state.index_ = q % this->_index.size();
			state.block_ = block;
			state.next_ = &this->_index[(q + 1)%this->_index.size()];
			state.prev_ = &this->_index[(q - 1)%this->_index.size()];

			++q;

		}			

		this->_partition.push_back(block);

	}

	~OLRTAlgorithm() {

		for (auto& block : this->_partition)
			delete block;

	}

	void init(const std::vector<std::vector<size_t>>& partition, const BinaryRelation& relation) {

		assert(OLRTAlgorithm::isPartition(partition, this->_lts.states()));

		for (size_t i = 1; i < partition.size(); ++i)
			this->makeBlock(partition[i], i);

		this->_relation = relation;

		assert(this->isConsistent());

		this->_lts.buildDelta(this->_delta, this->_delta1);
		this->_key.resize(this->_lts.labels()*this->_lts.states(), static_cast<size_t>(-1));
		this->_range.resize(this->_lts.labels());

		for (size_t a = 0; a < this->_lts.labels(); ++a) {

			this->_range[a] = this->_delta1[a].size();

			size_t x = 0;

			for (auto& q : this->_delta1[a])
				this->_key[a*this->_lts.states() + q] = x++;

		}

		for (size_t a = 0; a < this->_lts.labels(); ++a)

			this->fastSplit(this->_delta1[a]);

		std::vector<std::vector<size_t>> pre(this->_partition.size()), noPre(this->_lts.labels());

		for (auto& block : this->_partition) {

			StateListElem* elem = block->states();

			do {

				for (size_t a = 0; a < this->_lts.labels(); ++a) {

					this->_delta1[a].contains(elem->index_)
						? pre[block->index()].push_back(a)
						: noPre[a].push_back(block->index());

				}

				elem = elem->next_;

			} while (elem != block->states());

		}

		for (size_t b1 = 0; b1 < this->_partition.size(); ++b1) {

			for (auto& a : pre[b1]) {

				for (auto& b2 : noPre[a]) {

					assert(b1 != b2);

					this->_relation.set(b1, b2, false);

				}

			}

		}

		SmartSet s;

		for (std::vector<OLRTBlock*>::reverse_iterator i = this->_partition.rbegin(); i != this->_partition.rend(); ++i) {

			for (auto& a : (*i)->inset()) {

				for (auto q : this->_delta1[a]) {

					for (auto r : this->_lts.post(a)[q]) {

						if (this->_relation.get((*i)->index(), this->_index[r].block_->index()))
							(*i)->counter().incr(a, q);

					}

				}

				s.assignFlat(this->_delta1[a]);

				for (auto& b2 : this->_partition) {

					if (!this->_relation.get((*i)->index(), b2->index()))
						continue;

					StateListElem* elem = b2->states();

					do {

						for (auto& q : this->_lts.pre(a)[elem->index_])
							s.remove(q);

						elem = elem->next_;

					} while (elem != b2->states());

				}

				if (s.empty())
					continue;

				(*i)->_remove[a] = new RemoveList(new std::vector<size_t>(s.begin(), s.end()));

				this->_queue.push_back(std::make_pair(*i, a));

				assert(s.size() == (*i)->_remove[a]->subList()->size());

			}

		}

	}

	void run() {

	    while (!this->_queue.empty()) {

			std::pair<OLRTBlock*, size_t> tmp(this->_queue.back());

			this->_queue.pop_back();

			this->processRemove(tmp.first, tmp.second);

		}

	}
	
	void buildResult(BinaryRelation& result, size_t size) const {

		result.resize(size);

		for (size_t i = 0; i < size; ++i) {

			size_t ii = this->_index[i].block_->index();

			for (size_t j = 0; j < size; ++j)

				result.set(i, j, this->_relation.get(ii, this->_index[j].block_->index()));

		}

	}

	friend std::ostream& operator<<(std::ostream& os, const OLRTAlgorithm& alg) {

		for (auto& block : alg._partition)
	  		os << *block;

		return os << "relation:" << std::endl << alg._relation;

	}

};

BinaryRelation VATA::ExplicitLTS::computeSimulation(
	const std::vector<std::vector<size_t>>& partition,
	const BinaryRelation& relation,
	size_t outputSize
) {

	if (this->states_ == 0)
		return BinaryRelation();

	OLRTAlgorithm alg(*this);

	alg.init(partition, relation);
	alg.run();

	BinaryRelation result;

	alg.buildResult(result, outputSize);

	return result;

}
