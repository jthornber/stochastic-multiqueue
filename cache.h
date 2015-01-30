#ifndef SMQ_CACHE_H
#define SMQ_CACHE_H

#include <boost/optional.hpp>
#include <vector>

//----------------------------------------------------------------

namespace smq {
	class cache {
	public:
		// The origin block size must be a multiple of the cache
		// block size
		cache(unsigned origin_block_size,
		      unsigned nr_origin_blocks,
		      unsigned cache_block_size,
		      unsigned nr_cache_blocks);

		using maybe_block = boost::optional<unsigned>;

		struct map_result {
			maybe_block cache;
			maybe_block promote;
			maybe_block demote;
		};

		map_result map(unsigned cache_block);

	private:
		multiqueue hotspots_;

		// FIXME: need to split into clean and dirty, can each
		// level have a pair of lists?  How do we unify shuffling.
		multiqueue cache_;
	}

}

//----------------------------------------------------------------

#endif
