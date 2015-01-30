#ifndef SMQ_SAMPLER_H
#define SMQ_SAMPLER_H

#include <random>
#include <vector>

//----------------------------------------------------------------

namespace smq {
	// The pdf functions are passed a value between 0.0 and 1.0.  They
	// should integrate over that range to 1.
	double constant_pdf(double alpha);
	double gaussian_pdf(double mean, double deviation, double alpha);

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

		unsigned sample();
		std::vector<double> const &get_pdf() const;
		std::vector<double> const &get_summation() const;

	private:
		template <typename Generator>
		void calc_pdf(Generator &gen) {
			auto nr_bins = pdf_.size();

			for (unsigned i = 0; i < nr_bins; i++) {
				auto alpha = static_cast<double>(i) / static_cast<double>(nr_bins);
				pdf_[i] = gen(alpha);
			}
		}

		void normalise_pdf();
		void calc_summation();

		std::random_device rd_;
		std::mt19937 rng_;
		std::uniform_real_distribution<double> uniform_;
		std::vector<double> pdf_;
		std::vector<double> summation_;
	};
}

//----------------------------------------------------------------

#endif
