#include "multiqueue.h"

using namespace smq;
using namespace std;

//----------------------------------------------------------------

block::block()
	: level_(0),
	  hit_count_(0)
{
}

//--------------------------------

queue_level::queue_level()
	: count_(0)
{
}

bool
queue_level::empty() const
{
	return list_.empty();
}

block &
queue_level::front()
{
	return list_.front();
}

block &
queue_level::back()
{
	return list_.back();
}

void
queue_level::push_back(block &b)
{
	list_.push_back(b);
	count_++;
}

void
queue_level::push_front(block &b)
{
	list_.push_front(b);
	count_++;
}

void
queue_level::pop_back()
{
	list_.pop_back();
	count_--;
}

void
queue_level::pop_front()
{
	list_.pop_front();
	count_--;
}

void
queue_level::erase(block &b)
{
	list_.erase(list_.iterator_to(b));
	count_--;
}

void
queue_level::splice_front(queue_level &l)
{
	list_.splice(list_.begin(), l.list_);
	count_ += l.count_;
	l.count_ = 0;
}

void
queue_level::splice_back(queue_level &l)
{
	list_.splice(list_.end(), l.list_);
	count_ += l.count_;
	l.count_ = 0;
}

//--------------------------------

multiqueue::multiqueue(unsigned nr_blocks, unsigned nr_levels)
	: blocks_(nr_blocks),
	  levels_(nr_levels),
	  hits_(0),
	  misses_(0)
{
	for (auto &b : blocks_)
		levels_[0].push_back(b);
}

bool
multiqueue::in_cache(block const &b)
{
	return (b.level_ > (levels_.size() / 8) * 7);
}

void
multiqueue::hit(unsigned bindex)
{
	if (bindex < blocks_.size()) {
		block &b = blocks_[bindex];
		queue_level &l = levels_[b.level_];

		b.hit_count_++;

		if (in_cache(b))
			hits_++;
		else
			misses_++;

		l.erase(b);
		l.push_back(b);
	}
}

void
multiqueue::clear_hits()
{
	for (auto & b : blocks_)
		b.hit_count_ = 0;
}

vector<unsigned>
multiqueue::level_populations() const
{
	vector<unsigned> r(levels_.size());
	for (unsigned i = 0; i < levels_.size(); i++)
		r[i] = levels_[i].count_;
	return r;
}

multiqueue::hit_analysis
multiqueue::get_hit_analysis(unsigned top_percent) const
{
	hit_analysis r;
	r.top_percent_ = top_percent;
	r.hits_in_levels_ = 0;
	r.hits_actual_ = 0;

	unsigned target = (blocks_.size() * top_percent) / 100u;

	for (unsigned level = levels_.size(); level; --level) {
		queue_level const &l = levels_[level - 1];

		for (auto it = l.list_.rbegin(); target > 0 && it != l.list_.rend(); ++it, --target)
			r.hits_in_levels_ += it->hit_count_;
	}

	vector<block const *> sorted(blocks_.size());
	for (unsigned i = 0; i < blocks_.size(); i++)
		sorted[i] = &blocks_[i];
	sort(sorted.begin(), sorted.end(), cmp_block_high_to_low);

	target = (blocks_.size() * top_percent) / 100u;
	for (unsigned i = 0; i < target; i++)
		r.hits_actual_ += sorted[i]->hit_count_;

	return r;
}

vector<unsigned>
multiqueue::get_hits() const
{
	vector<unsigned> r(blocks_.size());

	auto index = 0u;
	for (auto const &l : levels_) {
		for (auto it = l.list_.cbegin(); it != l.list_.cend(); ++it)
			r[index++] = it->hit_count_;
	}

	return r;
}

void
multiqueue::shuffle(unsigned adjustment)
{
	unsigned nr_blocks = blocks_.size();
	unsigned nr_levels = levels_.size();
	unsigned target_per_level = nr_blocks / nr_levels;

	// Promote a few blocks
	queue_level promotes[nr_levels], demotes[nr_levels];

	for (unsigned level = 0; level < nr_levels; level++) {
		queue_level &l = levels_[level];

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
				block &b = l.back();
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
				block &b = l.front();
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

	hits_ = 0;
	misses_ = 0;
}

unsigned
multiqueue::get_autotune_adjustment() const
{
	unsigned max_adjustment = (blocks_.size() / levels_.size()) / 4;

	auto miss_ratio = static_cast<double>(misses_) /
		static_cast<double>(hits_);

	// I'm assuming hits per generation ~= nr_blocks
	miss_ratio = ((miss_ratio - 1.0) * 4.0) + 1.0;
	miss_ratio = min<double>(miss_ratio, static_cast<double>(max_adjustment));
	miss_ratio = max<double>(miss_ratio, 1.0);
	return floor(miss_ratio);
}

void
multiqueue::shuffle_with_autotune()
{
	shuffle(get_autotune_adjustment());
}

bool
multiqueue::cmp_block_high_to_low(block const *lhs, block const *rhs)
{
	return lhs->hit_count_ > rhs->hit_count_;
}

//----------------------------------------------------------------
