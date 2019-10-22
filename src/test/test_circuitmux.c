/* Copyright (c) 2013-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define TOR_CHANNEL_INTERNAL_
#define CIRCUITMUX_PRIVATE
#define CIRCUITMUX_EWMA_PRIVATE
#define RELAY_PRIVATE

#include "core/or/or.h"
#include "core/or/channel.h"
#include "core/or/circuitmux.h"
#include "core/or/circuitmux_ewma.h"
#include "core/or/destroy_cell_queue_st.h"
#include "core/or/relay.h"
#include "core/or/scheduler.h"

#include "test/fakechans.h"
#include "test/test.h"

#include <math.h>

static int
mock_has_queued_writes_true(channel_t *c)
{
  (void) c;
  return 1;
}

/** Test destroy cell queue with no interference from other queues. */
static void
test_cmux_destroy_cell_queue(void *arg)
{
  circuitmux_t *cmux = NULL;
  channel_t *ch = NULL;
  circuit_t *circ = NULL;
  destroy_cell_queue_t *cq = NULL;
  packed_cell_t *pc = NULL;
  destroy_cell_t *dc = NULL;

  MOCK(scheduler_release_channel, scheduler_release_channel_mock);

  (void) arg;

  ch = new_fake_channel();
  ch->has_queued_writes = mock_has_queued_writes_true;
  ch->wide_circ_ids = 1;
  cmux = ch->cmux;

  circ = circuitmux_get_first_active_circuit(cmux, &cq);
  tt_ptr_op(circ, OP_EQ, NULL);
  tt_ptr_op(cq, OP_EQ, NULL);

  circuitmux_append_destroy_cell(ch, cmux, 100, 10);
  circuitmux_append_destroy_cell(ch, cmux, 190, 6);
  circuitmux_append_destroy_cell(ch, cmux, 30, 1);

  tt_int_op(circuitmux_num_cells(cmux), OP_EQ, 3);

  circ = circuitmux_get_first_active_circuit(cmux, &cq);
  tt_ptr_op(circ, OP_EQ, NULL);
  tt_assert(cq);

  tt_int_op(cq->n, OP_EQ, 3);

  dc = destroy_cell_queue_pop(cq);
  tt_assert(dc);
  tt_uint_op(dc->circid, OP_EQ, 100);

  tt_int_op(circuitmux_num_cells(cmux), OP_EQ, 2);

 done:
  free_fake_channel(ch);
  packed_cell_free(pc);
  tor_free(dc);

  UNMOCK(scheduler_release_channel);
}

static void
test_cmux_compute_ticks(void *arg)
{
  const int64_t NS_PER_S = 1000 * 1000 * 1000;
  const int64_t START_NS = UINT64_C(1217709000)*NS_PER_S;
  int64_t now;
  double rem;
  unsigned tick;
  (void)arg;
  circuitmux_ewma_free_all();
  monotime_enable_test_mocking();

  monotime_coarse_set_mock_time_nsec(START_NS);
  cell_ewma_initialize_ticks();
  const unsigned tick_zero = cell_ewma_get_current_tick_and_fraction(&rem);
  tt_double_op(rem, OP_GT, -1e-9);
  tt_double_op(rem, OP_LT, 1e-9);

  /* 1.5 second later and we should still be in the same tick. */
  now = START_NS + NS_PER_S + NS_PER_S/2;
  monotime_coarse_set_mock_time_nsec(now);
  tick = cell_ewma_get_current_tick_and_fraction(&rem);
  tt_uint_op(tick, OP_EQ, tick_zero);
#ifdef USING_32BIT_MSEC_HACK
  const double tolerance = .0005;
#else
  const double tolerance = .00000001;
#endif
  tt_double_op(fabs(rem - .15), OP_LT, tolerance);

  /* 25 second later and we should be in another tick. */
  now = START_NS + NS_PER_S * 25;
  monotime_coarse_set_mock_time_nsec(now);
  tick = cell_ewma_get_current_tick_and_fraction(&rem);
  tt_uint_op(tick, OP_EQ, tick_zero + 2);
  tt_double_op(fabs(rem - .5), OP_LT, tolerance);

 done:
  ;
}

static void *
cmux_setup_test(const struct testcase_t *tc)
{
  static int whatever;

  (void) tc;

  cell_ewma_initialize_ticks();
  return &whatever;
}

static int
cmux_cleanup_test(const struct testcase_t *tc, void *ptr)
{
  (void) tc;
  (void) ptr;

  circuitmux_ewma_free_all();

  return 1;
}

static struct testcase_setup_t cmux_test_setup = {
  .setup_fn = cmux_setup_test,
  .cleanup_fn = cmux_cleanup_test,
};

#define TEST_CMUX(name) \
  { #name, test_cmux_##name, TT_FORK, &cmux_test_setup, NULL }

struct testcase_t circuitmux_tests[] = {
  TEST_CMUX(compute_ticks),
  TEST_CMUX(destroy_cell_queue),

  END_OF_TESTCASES
};

