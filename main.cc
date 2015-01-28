#include <algorithm>
#include <boost/math/constants/constants.hpp>
#include <boost/intrusive/list.hpp>
#include <iostream>
#include <random>
#include <vector>

using namespace std;
namespace bi = boost::intrusive;

//----------------------------------------------------------------

namespace {
	struct block {
		block()
			: level_(0),
			  hit_count_(0) {
		}

		boost::intrusive::list_member_hook<> member_hook_;
		unsigned level_;
		unsigned hit_count_;
	};

	typedef bi::list<block,
			 bi::member_hook<block,
					 bi::list_member_hook<>,
					 &block::member_hook_>
			 > block_list;

	class queue_level {
	public:
		queue_level()
			: count_(0) {
		}

		bool empty() const {
			return list_.empty();
		}

		block &front() {
			return list_.front();
		}

		block &back() {
			return list_.back();
		}

		void push_back(block &b) {
			list_.push_back(b);
			count_++;
		}

		void push_front(block &b) {
			list_.push_front(b);
			count_++;
		}

		void pop_back() {
			list_.pop_back();
			count_--;
		}

		void pop_front() {
			list_.pop_front();
			count_--;
		}

		void erase(block &b) {
			list_.erase(list_.iterator_to(b));
			count_--;
		}

		void splice_front(queue_level &l) {
			list_.splice(list_.begin(), l.list_);
			count_ += l.count_;
			l.count_ = 0;
		}

		void splice_back(queue_level &l) {
			list_.splice(list_.end(), l.list_);
			count_ += l.count_;
			l.count_ = 0;
		}

		unsigned count_;
		block_list list_;
	};

	class multiqueue {
	public:
		multiqueue(unsigned nr_levels)
			: levels_(nr_levels) {
		}

		vector<queue_level> levels_;
	};

	//--------------------------------

	void uniform_pdf(float *array, unsigned nr) {
		float v = 1.0 / nr;
		for (unsigned i = 0; i < nr; i++)
			array[i] = v;
	}

	void normal_pdf(float *array, unsigned nr, float mean, float deviation) {
		const double pi = boost::math::constants::pi<float>();

		for (unsigned i = 0; i < nr; i++) {
			float x = (float) i;

			float power = - ((x - mean) * (x - mean)) / (2.0 * deviation * deviation);
			float k = 1.0 / (deviation * sqrt(2.0 * pi));
			array[i] = k * exp(power);
		}
	}

	void normalise_pdf(float *array, unsigned nr) {
		float total = 0.0f;

		for (unsigned i = 0; i < nr; i++)
			total += array[i];
		total /= nr;

		if (total > 0.00001)
			for (unsigned i = 0; i < nr; i++)
				array[i] /= total;
	}

	void calc_summation_table(float *pdf, unsigned nr, float *table) {
		float sum = 0.0;

		for (unsigned i = 0; i < nr; i++) {
			sum += pdf[i];
			table[i] = sum;
		}
	}

	template <typename T>
	T blend(T v1, T v2, float alpha) {
		if (v1 < v2) {
			T range = v2 - v1;
			return static_cast<T>(alpha * static_cast<float>(range));
		} else {
			T range = v1 - v2;
			return static_cast<T>((1.0 - alpha) * static_cast<float>(range));
		}
	}

	// reverse order
	bool cmp_block_ptr(block *lhs, block *rhs) {
		return lhs->hit_count_ > rhs->hit_count_;
	}
}

//----------------------------------------------------------------

int main(int argc, char **argv)
{
	unsigned const NR_LEVELS = 128;
	unsigned const NR_BLOCKS = 8192;
	unsigned const NR_GENERATIONS = 1000;
	unsigned const HITS_PER_GENERATION = 10000;

	float pdf[NR_BLOCKS];
	// uniform_pdf(pdf, NR_BLOCKS);
	normal_pdf(pdf, NR_BLOCKS, NR_BLOCKS / 2, 1000.0);

	float sigma[NR_BLOCKS];
	calc_summation_table(pdf, NR_BLOCKS, sigma);

	vector<block> blocks(NR_BLOCKS);
	multiqueue mq(NR_LEVELS);

	for (unsigned i = 0; i < blocks.size(); i++)
		mq.levels_[0].push_back(blocks[i]);

	random_device rd;
	mt19937 rng(rd());
	uniform_real_distribution<float> u(0.0, 1.0);

	cout << "generation 10_percent_hits_over_ideal_10_percent_hits\n";

	for (unsigned generation = 0; generation < NR_GENERATIONS; generation++) {
		for (unsigned hit = 0; hit < HITS_PER_GENERATION; hit++) {
			float v = u(rng);
			float *pos = lower_bound(sigma, sigma + NR_BLOCKS, v);
			unsigned bindex = pos - sigma;

			if (bindex < blocks.size()) {
				block &b = blocks[pos - sigma];
				queue_level &l = mq.levels_[b.level_];

				b.hit_count_++;
				l.erase(b);
				l.push_back(b);
			}
		}

		unsigned target_per_level = NR_BLOCKS / NR_LEVELS;

		// Promote a few blocks
		queue_level promotes[NR_LEVELS], demotes[NR_LEVELS];

		for (unsigned level = 0; level < NR_LEVELS; level++) {
			queue_level &l = mq.levels_[level];

			unsigned target = 0;

			if (l.count_ > target_per_level)
				target = (l.count_ - target_per_level) / 4;

#if 0
			unsigned velocity = 1.0 - (static_cast<float>(min<unsigned>(generation, 200u)) / 200.0);
			target += blend(32u, 4u, velocity);
#else
			target += 8u;
#endif

			// Promote
			if (level < NR_LEVELS - 1) {
				if (level == 0)
					target *= 2;

				for (unsigned count = 0; count < target && !l.empty(); count++) {
					block &b = l.back();
					l.pop_back();

					b.level_ = level + 1;
					promotes[level].push_front(b);
				}
			}

			// demote
			if (level > 0) {
				if (level == NR_LEVELS - 1)
					target *= 2;

				for (unsigned count = 0; count < target && !l.empty(); count++) {
					block &b = l.front();
					l.pop_front();

					b.level_ = level - 1;
					demotes[level].push_back(b);
				}
			}
		}

		for (unsigned level = 0; level < NR_LEVELS; level++) {
			if (level < NR_LEVELS - 1)
				mq.levels_[level + 1].splice_front(promotes[level]);

			if (level > 0)
				mq.levels_[level - 1].splice_back(demotes[level]);
		}

		// Print stats
		vector<block *> sorted(NR_BLOCKS);
		unsigned i = 0;
		for (unsigned level = NR_LEVELS; level; --level) {
			queue_level &l = mq.levels_[level - 1];

			for (auto it = l.list_.rbegin(); it != l.list_.rend(); ++it)
				sorted[i++] = &(*it);
		}

		unsigned actual_total_hits = 0;
		for (unsigned i = 0; i < NR_BLOCKS / 10; i++)
			actual_total_hits += sorted[i]->hit_count_;

		for (unsigned i = 0; i < NR_BLOCKS; i++)
			sorted[i] = &blocks[i];
		sort(sorted.begin(), sorted.end(), cmp_block_ptr);

		unsigned ten_pc_total_hits = 0;
		for (unsigned i = 0; i < NR_BLOCKS / 10; i++)
			ten_pc_total_hits += sorted[i]->hit_count_;
		cout << generation << " "
		     << static_cast<float>(actual_total_hits) / static_cast<float>(ten_pc_total_hits)
		     << "\n";
	}

#if 0
	cout << "block hit_count\n";
	for (unsigned level = 0; level < NR_LEVELS; level++) {
		for (auto it = lru[level].list_.begin(); it != lru[level].list_.end(); ++it) {
			unsigned bindex = &(*it) - &blocks[0];
			cout << bindex << " " << it->hit_count_ << " " << it->level_ << "\n";
		}

		cerr << "level " << level << ": " << lru[level].count_ << "\n";
	}
#endif



	return 0;
}

//----------------------------------------------------------------
