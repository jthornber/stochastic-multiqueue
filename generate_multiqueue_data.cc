#include "multiqueue.h"
#include "sampler.h"
#include "utils.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <vector>

using namespace std;
using namespace smq;

//----------------------------------------------------------------

namespace {
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
			mqs.push_back(make_unique<multiqueue>(NR_BLOCKS, 1 << i));

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
			mqs.push_back(make_unique<multiqueue>(NR_BLOCKS, 64));

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

				auto stats = mq->get_hit_analysis(10);
				out << " "
				    << static_cast<double>(stats.hits_in_levels_) /
					static_cast<double>(stats.hits_actual_);

				mq->clear_hits();
			}

			out << "\n";
		}
	}

	template <typename Generator1, typename Generator2>
	void ha_with_changing_pdf_and_autotune(Generator1 const &gen1, Generator2 const &gen2, ostream &out) {
		unsigned const NR_BLOCKS = 8192;
		unsigned const NR_GENERATIONS = 50;
		unsigned const HITS_PER_GENERATION = 10000;

		sampler s1(NR_BLOCKS, gen1);
		sampler s2(NR_BLOCKS, gen2);
		multiqueue mq(NR_BLOCKS, 64);

		for (unsigned generation = 0; generation < NR_GENERATIONS * 6; generation++) {
			auto &cs = ((generation / NR_GENERATIONS) & 1) ? s1 : s2;
			for (unsigned hit = 0; hit < HITS_PER_GENERATION; hit++)
				mq.hit(cs.sample());

			auto adjustment = mq.get_autotune_adjustment(); // gets zeroed by shuffle, so we have to get early
			mq.shuffle_with_autotune();

			out << generation;

			auto stats = mq.get_hit_analysis(10);
			out << " "
			    << static_cast<double>(stats.hits_in_levels_) /
				static_cast<double>(stats.hits_actual_)
			    << " " << adjustment << "\n";

			mq.clear_hits();
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
			mqs.push_back(make_unique<multiqueue>(NR_BLOCKS, 1 << i));

		for (unsigned generation = 0; generation < NR_GENERATIONS; generation++) {
			for (unsigned hit = 0; hit < HITS_PER_GENERATION; hit++) {
				auto v = s.sample();
				for (auto &mq : mqs)
					mq->hit(v);
			}

			for (auto &mq : mqs)
				mq->shuffle_with_autotune();
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
			mqs.push_back(make_unique<multiqueue>(NR_BLOCKS, 64));

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

	with_file("ha_with_changing_pdf_and_autotune.dat",
		  [gen, gen2](ostream &out) {
                          ha_with_changing_pdf_and_autotune(gen, gen2, out);
		  });

	return 0;
}

//----------------------------------------------------------------

