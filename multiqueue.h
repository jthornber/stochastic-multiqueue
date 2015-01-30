#ifndef SMQ_MULTIQUEUE_H
#define SMQ_MULTIQUEUE_H

#include <boost/intrusive/list.hpp>
#include <vector>

//----------------------------------------------------------------

namespace smq {
	namespace bi = boost::intrusive;

	struct block {
		block();

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
		queue_level();

		bool empty() const;
		block &front();
		block &back();
		void push_back(block &b);
		void push_front(block &b);
		void pop_back();
		void pop_front();
		void erase(block &b);
		void splice_front(queue_level &l);
		void splice_back(queue_level &l);

		// FIXME: make private
		unsigned count_;
		block_list list_;
	};

	class multiqueue {
	public:
		multiqueue(unsigned nr_blocks, unsigned nr_levels);

		bool in_cache(block const &b);
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
		static bool cmp_block_high_to_low(block const *lhs, block const *rhs);

		std::vector<block> blocks_;
		std::vector<queue_level> levels_;
		unsigned hits_;
		unsigned misses_;
	};
}

//----------------------------------------------------------------

#endif
