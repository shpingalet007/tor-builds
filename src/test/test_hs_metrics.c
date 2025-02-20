/* Copyright (c) 2020-2021, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file test_hs_metrics.c
 * \brief Test hidden service metrics.
 */

#define HS_SERVICE_PRIVATE

#include "test/test.h"
#include "test/test_helpers.h"
#include "test/log_test_helpers.h"

#include "app/config/config.h"

#include "feature/hs/hs_metrics.h"
#include "feature/hs/hs_service.h"

#include "lib/crypt_ops/crypto_ed25519.h"

static void
test_metrics(void *arg)
{
  hs_service_t *service = NULL;

  (void) arg;

  hs_init();

  service = hs_service_new(get_options());
  tt_assert(service);
  service->config.version = HS_VERSION_THREE;
  ed25519_secret_key_generate(&service->keys.identity_sk, 0);
  ed25519_public_key_generate(&service->keys.identity_pk,
                              &service->keys.identity_sk);
  register_service(get_hs_service_map(), service);

  tt_assert(service->metrics.store);

  /* Update entry by identifier. */
  hs_metrics_update_by_ident(HS_METRICS_NUM_INTRODUCTIONS,
                             &service->keys.identity_pk, 0, NULL, 42);

  /* Confirm the entry value. */
  const smartlist_t *entries = metrics_store_get_all(service->metrics.store,
                                                     "tor_hs_intro_num_total");
  tt_assert(entries);
  tt_int_op(smartlist_len(entries), OP_EQ, 1);
  const metrics_store_entry_t *entry = smartlist_get(entries, 0);
  tt_assert(entry);
  tt_int_op(metrics_store_entry_get_value(entry), OP_EQ, 42);

  /* Update entry by service now. */
  hs_metrics_update_by_service(HS_METRICS_NUM_INTRODUCTIONS,
                               service, 0, NULL, 42);
  tt_int_op(metrics_store_entry_get_value(entry), OP_EQ, 84);

  const char *reason = HS_METRICS_ERR_INTRO_REQ_BAD_AUTH_KEY;

  /* Update tor_hs_intro_rejected_intro_req_count */
  hs_metrics_update_by_ident(HS_METRICS_NUM_REJECTED_INTRO_REQ,
                             &service->keys.identity_pk, 0, reason, 112);

  entries = metrics_store_get_all(service->metrics.store,
                                  "tor_hs_intro_rejected_intro_req_count");
  tt_assert(entries);
  tt_int_op(smartlist_len(entries), OP_EQ,
            hs_metrics_intro_req_error_reasons_size);

  entry = metrics_store_find_entry_with_label(
      entries, "reason=\"bad_auth_key\"");
  tt_assert(entry);
  tt_int_op(metrics_store_entry_get_value(entry), OP_EQ, 112);

  /* Update tor_hs_intro_rejected_intro_req_count entry by service now. */
  hs_metrics_update_by_service(HS_METRICS_NUM_REJECTED_INTRO_REQ, service, 0,
                               reason, 10);
  tt_int_op(metrics_store_entry_get_value(entry), OP_EQ, 122);

 done:
  hs_free_all();
}

struct testcase_t hs_metrics_tests[] = {

  { "metrics", test_metrics, TT_FORK, NULL, NULL },

  END_OF_TESTCASES
};
