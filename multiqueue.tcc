#include "multiqueue.h"

// FIXME: we can't have these in a header
using namespace smq;
using namespace std;

//----------------------------------------------------------------

template <typename Block>
queue_level<Block>::queue_level()
	: count_(0)
{
}

template <typename Block>
bool
queue_level<Block>::empty() const
{
	return list_.empty();
}

template <typename Block>
Block &
queue_level<Block>::front()
{
	return list_.front();
}

template <typename Block>
Block &
queue_level<Block>::back()
{
	return list_.back();
}

template <typename Block>
void
queue_level<Block>::push_back(Block &b)
{
	list_.push_back(b);
	count_++;
}

template <typename Block>
void
queue_level<Block>::push_front(Block &b)
{
	list_.push_front(b);
	count_++;
}

template <typename Block>
void
queue_level<Block>::pop_back()
{
	list_.pop_back();
	count_--;
}

template <typename Block>
void
queue_level<Block>::pop_front()
{
	list_.pop_front();
	count_--;
}

template <typename Block>
void
queue_level<Block>::erase(Block &b)
{
	list_.erase(list_.iterator_to(b));
	count_--;
}

template <typename Block>
void
queue_level<Block>::splice_front(queue_level &l)
{
	list_.splice(list_.begin(), l.list_);
	count_ += l.count_;
	l.count_ = 0;
}

template <typename Block>
void
queue_level<Block>::splice_back(queue_level &l)
{
	list_.splice(list_.end(), l.list_);
	count_ += l.count_;
	l.count_ = 0;
}

//--------------------------------

template <typename Block>
multiqueue_base<Block>::multiqueue_base(unsigned nr_levels)
	: levels_(nr_levels),
	  nr_blocks_(0),
	  autotune_hits_(0),
	  autotune_misses_(0)
{
}

template <typename Block>
void
multiqueue_base<Block>::insert_block(Block &b)
{
	nr_blocks_++;
	levels_[0].push_back(b);
}

template <typename Block>
void
multiqueue_base<Block>::remove_block(Block &b)
{
	// FIXME: annotate block with a level_
	levels_[b.level_].erase(b);
}

template <typename Block>
bool
multiqueue_base<Block>::in_cache(Block const &b)
{
	return (b.level_ > (levels_.size() / 8) * 7);
}

template <typename Block>
void
multiqueue_base<Block>::hit(Block &b)
{
	queue_level<Block> &l = levels_[b.level_];

	if (in_cache(b))
		autotune_hits_++;
	else
		autotune_misses_++;

	l.erase(b);
	l.push_back(b);
}

template <typename Block>
vector<unsigned>
multiqueue_base<Block>::level_populations() const
{
	vector<unsigned> r(levels_.size());
	for (unsigned i = 0; i < levels_.size(); i++)
		r[i] = levels_[i].count_;
	return r;
}

template <typename Block>
void
multiqueue_base<Block>::shuffle(unsigned adjustment)
{
	unsigned nr_levels = levels_.size();
	unsigned target_per_level = nr_blocks_ / nr_levels;

	// Promote a few blocks
	queue_level<Block> promotes[nr_levels], demotes[nr_levels];

	for (unsigned level = 0; level < nr_levels; level++) {
		queue_level<Block> &l = levels_[level];

		unsigned target = 0;
		if (l.count_ > target_per_level + 4)
			target = (l.count_ - target_per_level) / 4;
		target += adjustment;

		// Promote
		if (level < nr_levels - 1) {
			if (level == 0)
				target *= 2;

			auto jump = max<unsigned>(1, target / target_per_level);
			auto new_level = min(level + jump, nr_levels - 1);

			for (unsigned count = 0; count < target && !l.empty(); count++) {
				Block &b = l.back();
				l.pop_back();

				b.level_ = new_level;
				promotes[new_level].push_front(b);
			}
		}

		// demote
		if (level > 0) {
			if (level == nr_levels - 1)
				target *= 2;

			unsigned jump = max<unsigned>(1, target / target_per_level);
			int new_level = jump > level ? 0 : level - jump;

			for (unsigned count = 0; count < target && !l.empty(); count++) {
				Block &b = l.front();
				l.pop_front();

				b.level_ = new_level;
				demotes[new_level].push_back(b);
			}
		}
	}

	for (unsigned level = 0; level < nr_levels; level++) {
		levels_[level].splice_front(promotes[level]);
		levels_[level].splice_back(demotes[level]);
	}

	autotune_hits_ = 0;
	autotune_misses_ = 0;
}

template <typename Block>
unsigned
multiqueue_base<Block>::get_autotune_adjustment() const
{
	unsigned max_adjustment = (nr_blocks_ / levels_.size()) / 4;

	auto miss_ratio = static_cast<double>(autotune_misses_) /
		static_cast<double>(autotune_hits_);

	// I'm assuming hits per generation ~= nr_blocks
	miss_ratio = ((miss_ratio - 1.0) * 4.0) + 1.0;
	miss_ratio = min<double>(miss_ratio, static_cast<double>(max_adjustment));
	miss_ratio = max<double>(miss_ratio, 1.0);
	return floor(miss_ratio);
}

template <typename Block>
void
multiqueue_base<Block>::shuffle_with_autotune()
{
	shuffle(get_autotune_adjustment());
}

//--------------------------------

template <typename Block>
multiqueue<Block>::multiqueue(unsigned nr_blocks, unsigned nr_levels)
	: multiqueue_base<Block>(nr_levels),
	  blocks_(nr_blocks)
{
	for (auto &b : blocks_)
		multiqueue_base<Block>::insert_block(b);
}

template <typename Block>
multiqueue<Block>::~multiqueue()
{
	for (auto &b : blocks_)
		multiqueue_base<Block>::remove_block(b);
}

template <typename Block>
void
multiqueue<Block>::hit(unsigned bindex)
{
	if (bindex < blocks_.size()) {
		Block &b = blocks_[bindex];
		multiqueue_base<Block>::hit(b);
		b.hit_count_++;
	}
}

template <typename Block>
void
multiqueue<Block>::clear_hits()
{
	for (auto & b : blocks_)
		b.hit_count_ = 0;
}

template <typename Block>
typename multiqueue<Block>::hit_analysis
multiqueue<Block>::get_hit_analysis(unsigned top_percent) const
{
	hit_analysis r;
	r.top_percent_ = top_percent;
	r.hits_in_levels_ = 0;
	r.hits_actual_ = 0;

	unsigned target = (blocks_.size() * top_percent) / 100u;

	for (unsigned level = multiqueue_base<Block>::levels_.size(); level; --level) {
		queue_level<Block> const &l = multiqueue_base<Block>::levels_[level - 1];

		for (auto it = l.list_.rbegin(); target > 0 && it != l.list_.rend(); ++it, --target)
			r.hits_in_levels_ += it->hit_count_;
	}

	std::vector<Block const *> sorted(blocks_.size());
	for (unsigned i = 0; i < blocks_.size(); i++)
		sorted[i] = &blocks_[i];
	sort(sorted.begin(), sorted.end(), cmp_block_high_to_low);

	target = (blocks_.size() * top_percent) / 100u;
	for (unsigned i = 0; i < target; i++)
		r.hits_actual_ += sorted[i]->hit_count_;

	return r;
}

template <typename Block>
std::vector<unsigned>
multiqueue<Block>::get_hits() const
{
	vector<unsigned> r(blocks_.size());

	auto index = 0u;
	for (auto const &l : multiqueue_base<Block>::levels_) {
		for (auto it = l.list_.cbegin(); it != l.list_.cend(); ++it)
			r[index++] = it->hit_count_;
	}

	return r;
}

template <typename Block>
bool
multiqueue<Block>::cmp_block_high_to_low(Block const *lhs, Block const *rhs)
{
	return lhs->hit_count_ > rhs->hit_count_;
}

//----------------------------------------------------------------
