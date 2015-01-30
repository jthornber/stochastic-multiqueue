#ifndef SMQ_MULTIQUEUE_H
#define SMQ_MULTIQUEUE_H

#include <boost/intrusive/list.hpp>
#include <vector>

//----------------------------------------------------------------

namespace smq {
	namespace bi = boost::intrusive;

	template <typename Block>
	class queue_level {
	public:
		queue_level();

		bool empty() const;
		Block &front();
		Block &back();
		void push_back(Block &b);
		void push_front(Block &b);
		void pop_back();
		void pop_front();
		void erase(Block &b);
		void splice_front(queue_level &l);
		void splice_back(queue_level &l);

		// FIXME: make private
		unsigned count_;

		using block_list = bi::list<Block,
					    bi::member_hook<Block,
							    bi::list_member_hook<>,
							    &Block::member_hook_>
					    >;

		block_list list_;
	};

	template <typename Block>
	class multiqueue {
	public:
		multiqueue(unsigned nr_blocks, unsigned nr_levels);

		bool in_cache(Block const &b); // FIXME: rename or remove
		void hit(unsigned bindex);
		void clear_hits();
		std::vector<unsigned> level_populations() const;

		struct hit_analysis {
			unsigned top_percent_;
			unsigned hits_in_levels_;
			unsigned hits_actual_;
		};

		hit_analysis get_hit_analysis(unsigned top_percent) const;
		std::vector<unsigned> get_hits() const;
		void shuffle(unsigned adjustment = 1);
		unsigned get_autotune_adjustment() const;
		void shuffle_with_autotune();

	private:
		static bool cmp_block_high_to_low(Block const *lhs, Block const *rhs);

		std::vector<Block> blocks_;
		std::vector<queue_level<Block>> levels_;
		unsigned hits_;
		unsigned misses_;
	};
}

#include "multiqueue.tcc"

//----------------------------------------------------------------

#endif
