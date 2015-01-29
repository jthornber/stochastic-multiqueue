#include <algorithm>
#include <boost/math/constants/constants.hpp>
#include <boost/intrusive/list.hpp>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <random>
#include <vector>

using namespace std;
namespace bi = boost::intrusive;

//----------------------------------------------------------------

namespace {
	// Generator should be a functor that returns the sample count for a
	// particular bin.
	class sampler {
	public:
		template <typename Generator>
		sampler(unsigned nr_bins, Generator const &gen)
			: rng_(rd_()),
			  uniform_(0.0, 1.0),
			  pdf_(nr_bins),
			  summation_(nr_bins) {

			calc_pdf(gen);
			normalise_pdf();
			calc_summation();
		}

		unsigned sample() {
			auto r = uniform_(rng_);
			auto p = lower_bound(summation_.cbegin(), summation_.cend(), r);
			auto index = static_cast<unsigned>(p - summation_.cbegin());

			if (index >= summation_.size())
				--index;

			return index;
		}

		vector<double> const &get_pdf() const {
			return pdf_;
		}

		vector<double> const &get_summation() const {
			return summation_;
		}

	private:
		template <typename Generator>
		void calc_pdf(Generator &gen) {
			auto nr_bins = pdf_.size();

			for (unsigned i = 0; i < nr_bins; i++) {
				auto alpha = static_cast<double>(i) / static_cast<double>(nr_bins);
				pdf_[i] = gen(alpha);
			}
		}

		void normalise_pdf() {
			auto total = 0.0;

			for (auto const &v : pdf_)
				total += v;

			if (total > 0.00001)
				for (auto &v : pdf_)
					v /= total;
		}

		void calc_summation() {
			auto total = 0.0;
			for (unsigned i = 0; i < summation_.size(); i++) {
				total += pdf_[i];
				summation_[i] = total;
			}
		}

		random_device rd_;
		mt19937 rng_;
		uniform_real_distribution<double> uniform_;
		vector<double> pdf_;
		vector<double> summation_;
	};

	//--------------------------------

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
		multiqueue(unsigned nr_blocks, unsigned nr_levels)
			: blocks_(nr_blocks),
			  levels_(nr_levels),
			  hits_(0),
			  misses_(0) {

			for (auto &b : blocks_)
				levels_[0].push_back(b);
		}

		bool in_cache(block const &b) {
			return (b.level_ > (levels_.size() / 8) * 7);
		}

		void hit(unsigned bindex) {
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

		void clear_hits() {
			for (auto & b : blocks_)
				b.hit_count_ = 0;
		}

		vector<unsigned> level_populations() const {
			vector<unsigned> r(levels_.size());
			for (unsigned i = 0; i < levels_.size(); i++)
				r[i] = levels_[i].count_;
			return r;
		}

		struct hit_analysis {
			unsigned top_percent_;
			unsigned hits_in_levels_;
			unsigned hits_actual_;
		};

		hit_analysis get_hit_analysis(unsigned top_percent) const {
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

		vector<unsigned> get_hits() const {
			vector<unsigned> r(blocks_.size());

			auto index = 0u;
			for (auto const &l : levels_) {
				for (auto it = l.list_.cbegin(); it != l.list_.cend(); ++it)
					r[index++] = it->hit_count_;
			}

			return r;
		}

		void shuffle(unsigned adjustment = 1) {
			unsigned nr_blocks = blocks_.size();
			unsigned nr_levels = levels_.size();
			unsigned target_per_level = nr_blocks / nr_levels;

			autotune_overfull_ = false;

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
					if (jump > 1)
						autotune_overfull_ = true;
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
					if (jump > 1)
						autotune_overfull_ = true;
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

		void shuffle_with_autotune() {
			// auto hit_ratio = get_hit_ratio();

			// We don't know what the best achievable hit_ratio
			// is for the current io profile, so how do we
			// interpret this number?
			if (autotune_overfull_)
				shuffle(blocks_.size() / (levels_.size() * 8));
			else
				shuffle();
		}

	private:
		double get_hit_ratio() const {
			return static_cast<double>(hits_) /
				static_cast<double>(misses_);
		}

		static bool cmp_block_high_to_low(block const *lhs, block const *rhs) {
			return lhs->hit_count_ > rhs->hit_count_;
		}

		vector<block> blocks_;
		vector<queue_level> levels_;
		unsigned hits_;
		unsigned misses_;
		bool autotune_overfull_;
	};

	//--------------------------------

	// The pdf functions are passed a value between 0.0 and 1.0.  They
	// should integrate over that range to 1.

	double constant_pdf(double alpha) {
		return 1.0;
	}

	double gaussian_pdf(double mean, double deviation, double alpha) {
		auto const pi = boost::math::constants::pi<double>();

		auto power = - ((alpha - mean) * (alpha - mean)) / (2.0 * deviation * deviation);
		auto k = 1.0 / (deviation * sqrt(2.0 * pi));
		return k * exp(power);
	}

	template <typename Generator1, typename Generator2>
	void show_pdf(Generator1 const &gen1, Generator2 const &gen2, ostream &out) {
		sampler s1(8192, gen1);
		sampler s2(8192, gen2);
		auto pdf1 = s1.get_pdf();
		auto pdf2 = s2.get_pdf();

		for (unsigned i = 0; i < pdf1.size(); i++)
			out << pdf1[i] << " " << pdf2[i] << "\n";
	}

	template <typename Generator>
	void show_summation(Generator const &gen, ostream &out) {
		sampler s(8192, gen);
		auto pdf = s.get_summation();
		for (auto const &v : pdf)
			out << v << "\n";
	}

	template <typename Generator>
	void level_populations(Generator const &gen, ostream &out) {
		unsigned const NR_BLOCKS = 8192;
		unsigned const NR_GENERATIONS = 100;
		unsigned const HITS_PER_GENERATION = 10000;

		sampler s(NR_BLOCKS, gen);
		multiqueue mq(NR_BLOCKS, 64);

		for (unsigned generation = 0; generation < NR_GENERATIONS; generation++) {
			for (unsigned hit = 0; hit < HITS_PER_GENERATION; hit++)
				mq.hit(s.sample());
			mq.shuffle();
			mq.clear_hits();

			for (auto const &p : mq.level_populations())
				out << p << " ";
			out << "\n";
		}
	}


	// Runs several mqs in parallel with the same sampler and produces
	// hit analyses for them vs generation
	template <typename Generator>
	void ha_vs_levels(Generator const &gen, unsigned percent, ostream &out) {
		unsigned const NR_BLOCKS = 8192;
		unsigned const NR_GENERATIONS = 100;
		unsigned const HITS_PER_GENERATION = 10000;

		sampler s(NR_BLOCKS, gen);
		list<unique_ptr<multiqueue>> mqs;

		for (unsigned i = 0; i < 8; i++)
			mqs.push_back(
				unique_ptr<multiqueue>(
					new multiqueue(NR_BLOCKS, 1 << i)));

		for (unsigned generation = 0; generation < NR_GENERATIONS; generation++) {

			for (unsigned hit = 0; hit < HITS_PER_GENERATION; hit++) {
				auto v = s.sample();
				for (auto &mq : mqs)
					mq->hit(v);
			}

			for (auto &mq : mqs)
				mq->shuffle();

			// Print stats
			out << generation;
			for (auto &mq : mqs) {
				auto stats = mq->get_hit_analysis(percent);
				out << " "
				    << static_cast<double>(stats.hits_in_levels_) /
				       static_cast<double>(stats.hits_actual_);
			}
			out << "\n";

			for (auto &mq : mqs)
				mq->clear_hits();
		}
	}

	template <typename Generator>
	void ha_vs_percent(Generator const &gen, ostream &out) {
		unsigned const NR_BLOCKS = 8192;
		unsigned const NR_GENERATIONS = 100;
		unsigned const HITS_PER_GENERATION = 10000;

		sampler s(NR_BLOCKS, gen);
		multiqueue mq(NR_BLOCKS, 64);

		for (unsigned generation = 0; generation < NR_GENERATIONS; generation++) {
			for (unsigned hit = 0; hit < HITS_PER_GENERATION; hit++)
				mq.hit(s.sample());
			mq.shuffle();
			mq.clear_hits();
		}

		for (unsigned percent = 0; percent < 101; percent++) {
			auto stats = mq.get_hit_analysis(percent);
			out << static_cast<double>(stats.hits_in_levels_) /
				static_cast<double>(stats.hits_actual_) << "\n";
		}
	}

	template <typename Generator1, typename Generator2>
	void ha_with_changing_pdf_vs_adjustments(Generator1 const &gen1, Generator2 const &gen2, ostream &out) {
		unsigned const NR_BLOCKS = 8192;
		unsigned const NR_GENERATIONS = 50;
		unsigned const HITS_PER_GENERATION = 10000;

		sampler s1(NR_BLOCKS, gen1);
		sampler s2(NR_BLOCKS, gen2);

		list<unique_ptr<multiqueue>> mqs;
		for (unsigned i = 0; i < 6; i++)
			mqs.push_back(
				unique_ptr<multiqueue>(
					new multiqueue(NR_BLOCKS, 64)));

		for (unsigned generation = 0; generation < NR_GENERATIONS * 100; generation++) {
			auto &cs = ((generation / NR_GENERATIONS) & 1) ? s1 : s2;
			for (unsigned hit = 0; hit < HITS_PER_GENERATION; hit++) {
				auto v = cs.sample();
				for (auto &mq : mqs)
					mq->hit(v);
			}

			out << generation;

			unsigned i = 0;
			for (auto &mq : mqs) {
				mq->shuffle(1 << i++);

#if 1
				auto stats = mq->get_hit_analysis(10);
				out << " "
				    << static_cast<double>(stats.hits_in_levels_) /
					static_cast<double>(stats.hits_actual_);
#else
				out << " " << mq->get_hit_ratio();
#endif

				mq->clear_hits();
			}

			out << "\n";
		}
	}


	// Runs several mqs in parallel with the same sampler and outputs
	// hit vs position
	template <typename Generator>
	void hits_vs_levels(Generator const &gen, ostream &out) {
		unsigned const NR_BLOCKS = 8192;
		unsigned const NR_GENERATIONS = 100;
		unsigned const HITS_PER_GENERATION = 10000;

		sampler s(NR_BLOCKS, gen);
		list<unique_ptr<multiqueue>> mqs;

		for (unsigned i = 0; i < 8; i++)
			mqs.push_back(
				unique_ptr<multiqueue>(
					new multiqueue(NR_BLOCKS, 1 << i)));

		for (unsigned generation = 0; generation < NR_GENERATIONS; generation++) {
			for (unsigned hit = 0; hit < HITS_PER_GENERATION; hit++) {
				auto v = s.sample();
				for (auto &mq : mqs)
					mq->hit(v);
			}

			for (auto &mq : mqs)
				mq->shuffle(32);
		}

		vector<vector<unsigned>> hits(mqs.size());
		auto count = 0u;
		for (auto const &mq : mqs)
			hits[count++] = mq->get_hits();

		for (unsigned b = 0; b < NR_BLOCKS; b++) {
			out << b;
			for (unsigned q = 0; q < mqs.size(); q++)
				out << " " << hits[q][b];
			out << "\n";
		}
	}

	template <typename Generator>
	void hits_vs_adjustments(Generator const &gen, ostream &out) {
		unsigned const NR_BLOCKS = 8192;
		unsigned const NR_GENERATIONS = 100;
		unsigned const HITS_PER_GENERATION = 10000;

		sampler s(NR_BLOCKS, gen);
		list<unique_ptr<multiqueue>> mqs;

		for (unsigned i = 0; i < 4; i++)
			mqs.push_back(
				unique_ptr<multiqueue>(
					new multiqueue(NR_BLOCKS, 64)));

		for (unsigned generation = 0; generation < NR_GENERATIONS; generation++) {
			for (unsigned hit = 0; hit < HITS_PER_GENERATION; hit++) {
				auto v = s.sample();
				for (auto &mq : mqs)
					mq->hit(v);
			}

			auto adjustment = 1u;
			for (auto &mq : mqs) {
				mq->shuffle(adjustment);
				mq->clear_hits();
				adjustment *= 2;
			}
		}

		vector<vector<unsigned>> hits(mqs.size());
		auto count = 0u;
		for (auto const &mq : mqs)
			hits[count++] = mq->get_hits();

		for (unsigned b = 0; b < NR_BLOCKS; b++) {
			out << b;
			for (unsigned q = 0; q < mqs.size(); q++)
				out << " " << hits[q][b];
			out << "\n";
		}
	}


	template <typename Fn>
	void with_file(string const &path, Fn f) {
		ofstream out(path);
		f(out);
	}
}

//----------------------------------------------------------------

int main(int argc, char **argv)
{
	auto gen = [](double alpha) {
		auto r = gaussian_pdf(0.5, 0.02, alpha) +
		gaussian_pdf(0.1, 0.05, alpha) +
		gaussian_pdf(0.8, 0.1, alpha) +
		0.01 * constant_pdf(alpha);

		return r;
	};

	auto gen2 = [](double alpha) {
		auto r = 0.3 * gaussian_pdf(0.6, 0.02, alpha) +
		gaussian_pdf(0.3, 0.05, alpha) +
		0.1 * gaussian_pdf(0.8, 0.1, alpha) +
		0.01 * constant_pdf(alpha);

		return r;
	};

	with_file("pdf.dat",
		  [gen, gen2](ostream &out) {
			  show_pdf(gen, gen2, out);
		  });

	with_file("summation_table.dat",
		  [gen](ostream &out) {
			  show_summation(gen, out);
		  });

	with_file("level_population.dat",
		  [gen](ostream &out) {
			  level_populations(gen, out);
		  });

	with_file("hits_vs_levels.dat",
		  [gen](ostream &out) {
			  hits_vs_levels(gen, out);
		  });

	with_file("hits_vs_adjustments.dat",
		  [gen](ostream &out) {
			  hits_vs_adjustments(gen, out);
		  });

	with_file("ha_vs_levels.dat",
		  [gen](ostream &out) {
			  ha_vs_levels(gen, 10, out);
		  });

	with_file("ha_vs_percent.dat",
		  [gen](ostream &out) {
			  ha_vs_percent(gen, out);
		  });

	with_file("ha_with_changing_pdf_vs_adjustments.dat",
		  [gen, gen2](ostream &out) {
                          ha_with_changing_pdf_vs_adjustments(gen, gen2, out);
		  });

	return 0;
}

//----------------------------------------------------------------

