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
			  levels_(nr_levels) {

			for (auto &b : blocks_)
				levels_[0].push_back(b);
		}

		void hit(unsigned bindex) {
			if (bindex < blocks_.size()) {
				block &b = blocks_[bindex];
				queue_level &l = levels_[b.level_];

				b.hit_count_++;
				l.erase(b);
				l.push_back(b);
			}
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

			// Promote a few blocks
			queue_level promotes[nr_levels], demotes[nr_levels];

			for (unsigned level = 0; level < nr_levels; level++) {
				queue_level &l = levels_[level];

				unsigned target = 0;

				if (l.count_ > target_per_level)
					target = (l.count_ - target_per_level) / 4;

				target += adjustment;

				// Promote
				if (level < nr_levels - 1) {
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
					if (level == nr_levels - 1)
						target *= 2;

					for (unsigned count = 0; count < target && !l.empty(); count++) {
						block &b = l.front();
						l.pop_front();

						b.level_ = level - 1;
						demotes[level].push_back(b);
					}
				}
			}

			for (unsigned level = 0; level < nr_levels; level++) {
				if (level < nr_levels - 1)
					levels_[level + 1].splice_front(promotes[level]);

				if (level > 0)
					levels_[level - 1].splice_back(demotes[level]);
			}
		}

	private:
		// reverse order
		static bool cmp_block_high_to_low(block const *lhs, block const *rhs) {
			return lhs->hit_count_ > rhs->hit_count_;
		}

		vector<block> blocks_;
		vector<queue_level> levels_;
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

	template <typename Generator>
	void show_pdf(Generator const &gen, ostream &out) {
		sampler s(8192, gen);
		auto pdf = s.get_pdf();
		for (auto const &v : pdf)
			out << v << "\n";
	}

	template <typename Generator>
	void show_summation(Generator const &gen, ostream &out) {
		sampler s(8192, gen);
		auto pdf = s.get_summation();
		for (auto const &v : pdf)
			out << v << "\n";
	}

	// Runs several mqs in parallel with the same sampler and produces
	// hit analyses for them vs generation
	template <typename Generator>
	void compare_nr_levels(Generator const &gen, unsigned percent, ostream &out) {
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
				    << static_cast<float>(stats.hits_in_levels_) /
				       static_cast<float>(stats.hits_actual_);
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
				mq->shuffle();
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

	with_file("pdf.dat",
		  [gen](ostream &out) {
			  show_pdf(gen, out);
		  });

	with_file("summation_table.dat",
		  [gen](ostream &out) {
			  show_summation(gen, out);
		  });

	with_file("hits_vs_levels.dat",
		  [gen](ostream &out) {
			  hits_vs_levels(gen, out);
		  });

	with_file("hits_vs_adjustments.dat",
		  [gen](ostream &out) {
			  hits_vs_adjustments(gen, out);
		  });


	with_file("compare_nr_levels.dat",
		  [gen](ostream &out) {
			  compare_nr_levels(gen, 10, out);
		  });
	return 0;
}

//----------------------------------------------------------------

