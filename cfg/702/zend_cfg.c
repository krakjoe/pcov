/*
   +----------------------------------------------------------------------+
   | Zend Engine, CFG - Control Flow Graph                                |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2018 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Dmitry Stogov <dmitry@zend.com>                             |
   +----------------------------------------------------------------------+

   This source file has been adapted for pcov so that the CFG from O+ is standalone
*/

#include "php.h"
#include "zend_compile.h"
#include "zend_cfg.h"
#include "zend_worklist.h"

/* func flags */
#define ZEND_FUNC_INDIRECT_VAR_ACCESS      (1<<0)  /* accesses variables by name  */
#define ZEND_FUNC_HAS_CALLS                (1<<1)
#define ZEND_FUNC_VARARG                   (1<<2)  /* uses func_get_args()        */
#define ZEND_FUNC_NO_LOOPS                 (1<<3)
#define ZEND_FUNC_IRREDUCIBLE              (1<<4)
#define ZEND_FUNC_RECURSIVE                (1<<7)
#define ZEND_FUNC_RECURSIVE_DIRECTLY       (1<<8)
#define ZEND_FUNC_RECURSIVE_INDIRECTLY     (1<<9)
#define ZEND_FUNC_HAS_EXTENDED_INFO        (1<<10)

/* The following flags are valid only for return values of internal functions
 * returned by zend_get_func_info()
 */
#define FUNC_MAY_WARN                      (1<<30)

typedef struct _zend_func_info zend_func_info;
typedef struct _zend_call_info zend_call_info;

#define ZEND_FUNC_INFO(op_array) \
	((zend_func_info*)((op_array)->reserved[zend_func_info_rid]))

#define ZEND_SET_FUNC_INFO(op_array, info) do { \
		zend_func_info** pinfo = (zend_func_info**)&(op_array)->reserved[zend_func_info_rid]; \
		*pinfo = info; \
	} while (0)

static uint32_t zend_cfg_classify_function(zend_string *name, uint32_t num_args) {
	if (zend_string_equals_literal(name, "extract")) {
		return ZEND_FUNC_INDIRECT_VAR_ACCESS;
	} else if (zend_string_equals_literal(name, "compact")) {
		return ZEND_FUNC_INDIRECT_VAR_ACCESS;
	} else if (zend_string_equals_literal(name, "parse_str") && num_args <= 1) {
		return ZEND_FUNC_INDIRECT_VAR_ACCESS;
	} else if (zend_string_equals_literal(name, "mb_parse_str") && num_args <= 1) {
		return ZEND_FUNC_INDIRECT_VAR_ACCESS;
	} else if (zend_string_equals_literal(name, "get_defined_vars")) {
		return ZEND_FUNC_INDIRECT_VAR_ACCESS;
	} else if (zend_string_equals_literal(name, "assert")) {
		return ZEND_FUNC_INDIRECT_VAR_ACCESS;
	} else if (zend_string_equals_literal(name, "func_num_args")) {
		return ZEND_FUNC_VARARG;
	} else if (zend_string_equals_literal(name, "func_get_arg")) {
		return ZEND_FUNC_VARARG;
	} else if (zend_string_equals_literal(name, "func_get_args")) {
		return ZEND_FUNC_VARARG;
	} else {
		return 0;
	}
}

static void zend_mark_reachable(zend_op *opcodes, zend_cfg *cfg, zend_basic_block *b) /* {{{ */
{
	zend_basic_block *blocks = cfg->blocks;

	while (1) {
		int i;

		b->flags |= ZEND_BB_REACHABLE;
		if (b->successors_count == 0) {
			b->flags |= ZEND_BB_EXIT;
			return;
		}

		for (i = 0; i < b->successors_count; i++) {
			zend_basic_block *succ = blocks + b->successors[i];

			if (b->len != 0) {
				zend_uchar opcode = opcodes[b->start + b->len - 1].opcode;
				if (b->successors_count == 1) {
					if (opcode == ZEND_JMP) {
						succ->flags |= ZEND_BB_TARGET;
					} else {
						succ->flags |= ZEND_BB_FOLLOW;

						if (cfg->split_at_calls) {
							if (opcode == ZEND_INCLUDE_OR_EVAL ||
								opcode == ZEND_GENERATOR_CREATE ||
								opcode == ZEND_YIELD ||
								opcode == ZEND_YIELD_FROM ||
								opcode == ZEND_DO_FCALL ||
								opcode == ZEND_DO_UCALL ||
								opcode == ZEND_DO_FCALL_BY_NAME) {
								succ->flags |= ZEND_BB_ENTRY;
							}
						}
						if (cfg->split_at_recv) {
							if (opcode == ZEND_RECV ||
								opcode == ZEND_RECV_INIT) {
								succ->flags |= ZEND_BB_RECV_ENTRY;
							}
						}
					}
				} else if (b->successors_count == 2) {
					if (i == 0 || opcode == ZEND_JMPZNZ) {
						succ->flags |= ZEND_BB_TARGET;
					} else {
						succ->flags |= ZEND_BB_FOLLOW;
					}
				} else {
					ZEND_ASSERT(opcode == ZEND_SWITCH_LONG || opcode == ZEND_SWITCH_STRING);
					if (i == b->successors_count - 1) {
						succ->flags |= ZEND_BB_FOLLOW | ZEND_BB_TARGET;
					} else {
						succ->flags |= ZEND_BB_TARGET;
					}
				}
			} else {
				succ->flags |= ZEND_BB_FOLLOW;
			}

			if (i == b->successors_count - 1) {
				/* Tail call optimization */
				if (succ->flags & ZEND_BB_REACHABLE) {
					return;
				}

				b = succ;
				break;
			} else {
				/* Recusively check reachability */
				if (!(succ->flags & ZEND_BB_REACHABLE)) {
					zend_mark_reachable(opcodes, cfg, succ);
				}
			}
		}
	}
}
/* }}} */

static void zend_mark_reachable_blocks(const zend_op_array *op_array, zend_cfg *cfg, int start) /* {{{ */
{
	zend_basic_block *blocks = cfg->blocks;

	blocks[start].flags = ZEND_BB_START;
	zend_mark_reachable(op_array->opcodes, cfg, blocks + start);

	if (op_array->last_live_range || op_array->last_try_catch) {
		zend_basic_block *b;
		int j, changed;
		uint32_t *block_map = cfg->map;

		do {
			changed = 0;

			/* Add live range paths */
			for (j = 0; j < op_array->last_live_range; j++) {
				zend_live_range *live_range = &op_array->live_range[j];
				if (live_range->var == (uint32_t)-1) {
					/* this live range already removed */
					continue;
				}
				b = blocks + block_map[live_range->start];
				if (b->flags & ZEND_BB_REACHABLE) {
					while (b->len > 0 && op_array->opcodes[b->start].opcode == ZEND_NOP) {
					    /* check if NOP breaks incorrect smart branch */
						if (b->len == 2
						 && (op_array->opcodes[b->start + 1].opcode == ZEND_JMPZ
						  || op_array->opcodes[b->start + 1].opcode == ZEND_JMPNZ)
						 && (op_array->opcodes[b->start + 1].op1_type & (IS_CV|IS_CONST))
						 && b->start > 0
						 && zend_is_smart_branch(op_array->opcodes + b->start - 1)) {
							break;
						}
						b->start++;
						b->len--;
					}
					if (b->len == 0 && (uint32_t)b->successors[0] == block_map[live_range->end]) {
						/* mark as removed (empty live range) */
						live_range->var = (uint32_t)-1;
						continue;
					}
					b->flags |= ZEND_BB_GEN_VAR;
					b = blocks + block_map[live_range->end];
					b->flags |= ZEND_BB_KILL_VAR;
					if (!(b->flags & (ZEND_BB_REACHABLE|ZEND_BB_UNREACHABLE_FREE))) {
						if (cfg->split_at_live_ranges) {
							changed = 1;
							zend_mark_reachable(op_array->opcodes, cfg, b);
						} else {
							b->flags |= ZEND_BB_UNREACHABLE_FREE;
						}
					}
				} else {
					ZEND_ASSERT(!(blocks[block_map[live_range->end]].flags & ZEND_BB_REACHABLE));
				}
			}

			/* Add exception paths */
			for (j = 0; j < op_array->last_try_catch; j++) {

				/* check for jumps into the middle of try block */
				b = blocks + block_map[op_array->try_catch_array[j].try_op];
				if (!(b->flags & ZEND_BB_REACHABLE)) {
					zend_basic_block *end;

					if (op_array->try_catch_array[j].catch_op) {
						end = blocks + block_map[op_array->try_catch_array[j].catch_op];
						while (b != end) {
							if (b->flags & ZEND_BB_REACHABLE) {
								op_array->try_catch_array[j].try_op = b->start;
								break;
							}
							b++;
						}
					}
					b = blocks + block_map[op_array->try_catch_array[j].try_op];
					if (!(b->flags & ZEND_BB_REACHABLE)) {
						if (op_array->try_catch_array[j].finally_op) {
							end = blocks + block_map[op_array->try_catch_array[j].finally_op];
							while (b != end) {
								if (b->flags & ZEND_BB_REACHABLE) {
									op_array->try_catch_array[j].try_op = op_array->try_catch_array[j].catch_op;
									changed = 1;
									zend_mark_reachable(op_array->opcodes, cfg, blocks + block_map[op_array->try_catch_array[j].try_op]);
									break;
								}
								b++;
							}
						}
					}
				}

				b = blocks + block_map[op_array->try_catch_array[j].try_op];
				if (b->flags & ZEND_BB_REACHABLE) {
					b->flags |= ZEND_BB_TRY;
					if (op_array->try_catch_array[j].catch_op) {
						b = blocks + block_map[op_array->try_catch_array[j].catch_op];
						b->flags |= ZEND_BB_CATCH;
						if (!(b->flags & ZEND_BB_REACHABLE)) {
							changed = 1;
							zend_mark_reachable(op_array->opcodes, cfg, b);
						}
					}
					if (op_array->try_catch_array[j].finally_op) {
						b = blocks + block_map[op_array->try_catch_array[j].finally_op];
						b->flags |= ZEND_BB_FINALLY;
						if (!(b->flags & ZEND_BB_REACHABLE)) {
							changed = 1;
							zend_mark_reachable(op_array->opcodes, cfg, b);
						}
					}
					if (op_array->try_catch_array[j].finally_end) {
						b = blocks + block_map[op_array->try_catch_array[j].finally_end];
						b->flags |= ZEND_BB_FINALLY_END;
						if (!(b->flags & ZEND_BB_REACHABLE)) {
							changed = 1;
							zend_mark_reachable(op_array->opcodes, cfg, b);
						}
					}
				} else {
					if (op_array->try_catch_array[j].catch_op) {
						ZEND_ASSERT(!(blocks[block_map[op_array->try_catch_array[j].catch_op]].flags & ZEND_BB_REACHABLE));
					}
					if (op_array->try_catch_array[j].finally_op) {
						ZEND_ASSERT(!(blocks[block_map[op_array->try_catch_array[j].finally_op]].flags & ZEND_BB_REACHABLE));
					}
					if (op_array->try_catch_array[j].finally_end) {
						ZEND_ASSERT(!(blocks[block_map[op_array->try_catch_array[j].finally_end]].flags & ZEND_BB_REACHABLE));
					}
				}
			}
		} while (changed);
	}
}
/* }}} */

static void initialize_block(zend_basic_block *block) {
	block->flags = 0;
	block->successors = block->successors_storage;
	block->successors_count = 0;
	block->predecessors_count = 0;
	block->predecessor_offset = -1;
	block->idom = -1;
	block->loop_header = -1;
	block->level = -1;
	block->children = -1;
	block->next_child = -1;
}

#define BB_START(i) do { \
		if (!block_map[i]) { blocks_count++;} \
		block_map[i]++; \
	} while (0)

int zend_build_cfg(zend_arena **arena, const zend_op_array *op_array, uint32_t build_flags, zend_cfg *cfg) /* {{{ */
{
	uint32_t flags = 0;
	uint32_t i;
	int j;
	uint32_t *block_map;
	zend_function *fn;
	int blocks_count = 0;
	zend_basic_block *blocks;
	zval *zv;
	zend_bool extra_entry_block = 0;

	cfg->split_at_live_ranges = (build_flags & ZEND_CFG_SPLIT_AT_LIVE_RANGES) != 0;
	cfg->split_at_calls = (build_flags & ZEND_CFG_STACKLESS) != 0;
	cfg->split_at_recv = (build_flags & ZEND_CFG_RECV_ENTRY) != 0 && (op_array->fn_flags & ZEND_ACC_HAS_TYPE_HINTS) == 0;

	cfg->map = block_map = zend_arena_calloc(arena, op_array->last, sizeof(uint32_t));

	/* Build CFG, Step 1: Find basic blocks starts, calculate number of blocks */
	BB_START(0);
	for (i = 0; i < op_array->last; i++) {
		zend_op *opline = op_array->opcodes + i;
		switch(opline->opcode) {
			case ZEND_RECV:
			case ZEND_RECV_INIT:
				if (build_flags & ZEND_CFG_RECV_ENTRY) {
					BB_START(i + 1);
				}
				break;
			case ZEND_RETURN:
			case ZEND_RETURN_BY_REF:
			case ZEND_GENERATOR_RETURN:
			case ZEND_EXIT:
			case ZEND_THROW:
				if (i + 1 < op_array->last) {
					BB_START(i + 1);
				}
				break;
			case ZEND_INCLUDE_OR_EVAL:
				flags |= ZEND_FUNC_INDIRECT_VAR_ACCESS;
			case ZEND_GENERATOR_CREATE:
			case ZEND_YIELD:
			case ZEND_YIELD_FROM:
				if (build_flags & ZEND_CFG_STACKLESS) {
					BB_START(i + 1);
				}
				break;
			case ZEND_DO_FCALL:
			case ZEND_DO_UCALL:
			case ZEND_DO_FCALL_BY_NAME:
				flags |= ZEND_FUNC_HAS_CALLS;
				if (build_flags & ZEND_CFG_STACKLESS) {
					BB_START(i + 1);
				}
				break;
			case ZEND_DO_ICALL:
				flags |= ZEND_FUNC_HAS_CALLS;
				break;
			case ZEND_INIT_FCALL:
			case ZEND_INIT_NS_FCALL_BY_NAME:
				zv = CRT_CONSTANT(opline->op2);
				if (opline->opcode == ZEND_INIT_NS_FCALL_BY_NAME) {
					/* The third literal is the lowercased unqualified name */
					zv += 2;
				}
				if ((fn = zend_hash_find_ptr(EG(function_table), Z_STR_P(zv))) != NULL) {
					if (fn->type == ZEND_INTERNAL_FUNCTION) {
						flags |= zend_cfg_classify_function(
							Z_STR_P(zv), opline->extended_value);
					}
				}
				break;
			case ZEND_FAST_CALL:
				BB_START(OP_JMP_ADDR(opline, opline->op1) - op_array->opcodes);
				BB_START(i + 1);
				break;
			case ZEND_FAST_RET:
				if (i + 1 < op_array->last) {
					BB_START(i + 1);
				}
				break;
			case ZEND_JMP:
				BB_START(OP_JMP_ADDR(opline, opline->op1) - op_array->opcodes);
				if (i + 1 < op_array->last) {
					BB_START(i + 1);
				}
				break;
			case ZEND_JMPZNZ:
				BB_START(OP_JMP_ADDR(opline, opline->op2) - op_array->opcodes);
				BB_START(ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, opline->extended_value));
				if (i + 1 < op_array->last) {
					BB_START(i + 1);
				}
				break;
			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
			case ZEND_JMP_SET:
			case ZEND_COALESCE:
			case ZEND_ASSERT_CHECK:
				BB_START(OP_JMP_ADDR(opline, opline->op2) - op_array->opcodes);
				BB_START(i + 1);
				break;
			case ZEND_CATCH:
				if (!opline->result.num) {
					BB_START(ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, opline->extended_value));
				}
				BB_START(i + 1);
				break;
			case ZEND_DECLARE_ANON_CLASS:
			case ZEND_DECLARE_ANON_INHERITED_CLASS:
			case ZEND_FE_FETCH_R:
			case ZEND_FE_FETCH_RW:
				BB_START(ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, opline->extended_value));
				BB_START(i + 1);
				break;
			case ZEND_FE_RESET_R:
			case ZEND_FE_RESET_RW:
				BB_START(OP_JMP_ADDR(opline, opline->op2) - op_array->opcodes);
				BB_START(i + 1);
				break;
			case ZEND_SWITCH_LONG:
			case ZEND_SWITCH_STRING:
			{
				HashTable *jumptable = Z_ARRVAL_P(CRT_CONSTANT(opline->op2));
				zval *zv;
				ZEND_HASH_FOREACH_VAL(jumptable, zv) {
					BB_START(ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, Z_LVAL_P(zv)));
				} ZEND_HASH_FOREACH_END();
				BB_START(ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, opline->extended_value));
				BB_START(i + 1);
				break;
			}
			case ZEND_UNSET_VAR:
			case ZEND_ISSET_ISEMPTY_VAR:
				if ((opline->extended_value & ZEND_FETCH_TYPE_MASK) == ZEND_FETCH_LOCAL) {
					flags |= ZEND_FUNC_INDIRECT_VAR_ACCESS;
				} else if (((opline->extended_value & ZEND_FETCH_TYPE_MASK) == ZEND_FETCH_GLOBAL ||
				            (opline->extended_value & ZEND_FETCH_TYPE_MASK) == ZEND_FETCH_GLOBAL_LOCK) &&
				           !op_array->function_name) {
					flags |= ZEND_FUNC_INDIRECT_VAR_ACCESS;
				}
				break;
			case ZEND_FETCH_R:
			case ZEND_FETCH_W:
			case ZEND_FETCH_RW:
			case ZEND_FETCH_FUNC_ARG:
			case ZEND_FETCH_IS:
			case ZEND_FETCH_UNSET:
				if ((opline->extended_value & ZEND_FETCH_TYPE_MASK) == ZEND_FETCH_LOCAL) {
					flags |= ZEND_FUNC_INDIRECT_VAR_ACCESS;
				} else if (((opline->extended_value & ZEND_FETCH_TYPE_MASK) == ZEND_FETCH_GLOBAL ||
				            (opline->extended_value & ZEND_FETCH_TYPE_MASK) == ZEND_FETCH_GLOBAL_LOCK) &&
				           !op_array->function_name) {
					flags |= ZEND_FUNC_INDIRECT_VAR_ACCESS;
				}
				break;
			case ZEND_FUNC_GET_ARGS:
				flags |= ZEND_FUNC_VARARG;
				break;
		}
	}

	/* If the entry block has predecessors, we may need to split it */
	if ((build_flags & ZEND_CFG_NO_ENTRY_PREDECESSORS)
			&& op_array->last > 0 && block_map[0] > 1) {
		extra_entry_block = 1;
	}

	if (cfg->split_at_live_ranges) {
		for (j = 0; j < op_array->last_live_range; j++) {
			BB_START(op_array->live_range[j].start);
			BB_START(op_array->live_range[j].end);
		}
	}

	if (op_array->last_try_catch) {
		for (j = 0; j < op_array->last_try_catch; j++) {
			BB_START(op_array->try_catch_array[j].try_op);
			if (op_array->try_catch_array[j].catch_op) {
				BB_START(op_array->try_catch_array[j].catch_op);
			}
			if (op_array->try_catch_array[j].finally_op) {
				BB_START(op_array->try_catch_array[j].finally_op);
			}
			if (op_array->try_catch_array[j].finally_end) {
				BB_START(op_array->try_catch_array[j].finally_end);
			}
		}
	}

	blocks_count += extra_entry_block;
	cfg->blocks_count = blocks_count;

	/* Build CFG, Step 2: Build Array of Basic Blocks */
	cfg->blocks = blocks = zend_arena_calloc(arena, sizeof(zend_basic_block), blocks_count);

	blocks_count = -1;

	if (extra_entry_block) {
		initialize_block(&blocks[0]);
		blocks[0].start = 0;
		blocks[0].len = 0;
		blocks_count++;
	}

	for (i = 0; i < op_array->last; i++) {
		if (block_map[i]) {
			if (blocks_count >= 0) {
				blocks[blocks_count].len = i - blocks[blocks_count].start;
			}
			blocks_count++;
			initialize_block(&blocks[blocks_count]);
			blocks[blocks_count].start = i;
		}
		block_map[i] = blocks_count;
	}

	blocks[blocks_count].len = i - blocks[blocks_count].start;
	blocks_count++;

	/* Build CFG, Step 3: Calculate successors */
	for (j = 0; j < blocks_count; j++) {
		zend_basic_block *block = &blocks[j];
		zend_op *opline;
		if (block->len == 0) {
			block->successors_count = 1;
			block->successors[0] = j + 1;
			continue;
		}

		opline = op_array->opcodes + block->start + block->len - 1;
		switch (opline->opcode) {
			case ZEND_FAST_RET:
			case ZEND_RETURN:
			case ZEND_RETURN_BY_REF:
			case ZEND_GENERATOR_RETURN:
			case ZEND_EXIT:
			case ZEND_THROW:
				break;
			case ZEND_JMP:
				block->successors_count = 1;
				block->successors[0] = block_map[OP_JMP_ADDR(opline, opline->op1) - op_array->opcodes];
				break;
			case ZEND_JMPZNZ:
				block->successors_count = 2;
				block->successors[0] = block_map[OP_JMP_ADDR(opline, opline->op2) - op_array->opcodes];
				block->successors[1] = block_map[ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, opline->extended_value)];
				break;
			case ZEND_JMPZ:
			case ZEND_JMPNZ:
			case ZEND_JMPZ_EX:
			case ZEND_JMPNZ_EX:
			case ZEND_JMP_SET:
			case ZEND_COALESCE:
			case ZEND_ASSERT_CHECK:
				block->successors_count = 2;
				block->successors[0] = block_map[OP_JMP_ADDR(opline, opline->op2) - op_array->opcodes];
				block->successors[1] = j + 1;
				break;
			case ZEND_CATCH:
				if (!opline->result.num) {
					block->successors_count = 2;
					block->successors[0] = block_map[ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, opline->extended_value)];
					block->successors[1] = j + 1;
				} else {
					block->successors_count = 1;
					block->successors[0] = j + 1;
				}
				break;
			case ZEND_DECLARE_ANON_CLASS:
			case ZEND_DECLARE_ANON_INHERITED_CLASS:
			case ZEND_FE_FETCH_R:
			case ZEND_FE_FETCH_RW:
				block->successors_count = 2;
				block->successors[0] = block_map[ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, opline->extended_value)];
				block->successors[1] = j + 1;
				break;
			case ZEND_FE_RESET_R:
			case ZEND_FE_RESET_RW:
				block->successors_count = 2;
				block->successors[0] = block_map[OP_JMP_ADDR(opline, opline->op2) - op_array->opcodes];
				block->successors[1] = j + 1;
				break;
			case ZEND_FAST_CALL:
				block->successors_count = 2;
				block->successors[0] = block_map[OP_JMP_ADDR(opline, opline->op1) - op_array->opcodes];
				block->successors[1] = j + 1;
				break;
			case ZEND_SWITCH_LONG:
			case ZEND_SWITCH_STRING:
			{
				HashTable *jumptable = Z_ARRVAL_P(CRT_CONSTANT(opline->op2));
				zval *zv;
				uint32_t s = 0;

				block->successors_count = 2 + zend_hash_num_elements(jumptable);
				block->successors = zend_arena_calloc(arena, block->successors_count, sizeof(int));

				ZEND_HASH_FOREACH_VAL(jumptable, zv) {
					block->successors[s++] = block_map[ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, Z_LVAL_P(zv))];
				} ZEND_HASH_FOREACH_END();

				block->successors[s++] = block_map[ZEND_OFFSET_TO_OPLINE_NUM(op_array, opline, opline->extended_value)];
				block->successors[s++] = j + 1;
				break;
			}
			default:
				block->successors_count = 1;
				block->successors[0] = j + 1;
				break;
		}
	}

	/* Build CFG, Step 4, Mark Reachable Basic Blocks */
	zend_mark_reachable_blocks(op_array, cfg, 0);

	cfg->dynamic = (flags & ZEND_FUNC_INDIRECT_VAR_ACCESS) != 0;
	cfg->vararg = (flags & ZEND_FUNC_VARARG) != 0;

	return SUCCESS;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
