#include "sampler.h"

#include <boost/math/constants/constants.hpp>

using namespace smq;
using namespace std;

//----------------------------------------------------------------

double smq::constant_pdf(double alpha)
{
	return 1.0;
}

double smq::gaussian_pdf(double mean, double deviation, double alpha) {
	auto const pi = boost::math::constants::pi<double>();

	auto power = - ((alpha - mean) * (alpha - mean)) / (2.0 * deviation * deviation);
	auto k = 1.0 / (deviation * sqrt(2.0 * pi));
	return k * exp(power);
}

//----------------------------------------------------------------

unsigned
sampler::sample()
{
	auto r = uniform_(rng_);
	auto p = lower_bound(summation_.cbegin(), summation_.cend(), r);
	auto index = static_cast<unsigned>(p - summation_.cbegin());

	if (index >= summation_.size())
		--index;

	return index;
}

vector<double> const &
sampler::get_pdf() const
{
	return pdf_;
}

vector<double> const &
sampler::get_summation() const
{
	return summation_;
}

void
sampler::normalise_pdf()
{
	auto total = 0.0;

	for (auto const &v : pdf_)
		total += v;

	if (total > 0.00001)
		for (auto &v : pdf_)
			v /= total;
}

void
sampler::calc_summation()
{
	auto total = 0.0;
	for (unsigned i = 0; i < summation_.size(); i++) {
		total += pdf_[i];
		summation_[i] = total;
	}
}

//----------------------------------------------------------------
