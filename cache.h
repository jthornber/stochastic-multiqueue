#ifndef SMQ_CACHE_H
#define SMQ_CACHE_H

#include "multiqueue.h"

#include <boost/intrusive/set.hpp>
#include <boost/optional.hpp>
#include <vector>

//----------------------------------------------------------------

namespace smq {
	namespace bi = boost::intrusive;

	template <typename Block>
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
		struct annotated_block {
			bi::set_member_hook<> set_hook_;
			Block b_;
		};

		multiqueue<annotated_block> hotspots_;

		using block_set = bi::set<annotated_block,
					  bi::member_hook<annotated_block,
							  bi::set_member_hook<>,
							  &annotated_block::set_hook_>>;

		// FIXME: need to split into clean and dirty, can each
		// level have a pair of lists?  How do we unify shuffling.
		multiqueue<annotated_block> cache_;
		block_set blocks_;
	};

}

//----------------------------------------------------------------

#endif
