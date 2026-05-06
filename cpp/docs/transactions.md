# Transactions {#transactions_page}

**Unsettled API** — Session transactions and related handler callbacks
are still evolving; see @ref mainpage "Conventions".

This page documents the C++ API contract for AMQP 1.0 local
transactions (OASIS AMQP 1.0 §4.6). Automated coverage lives in
`tests/cpp/tx_tests/` and is run by `tests/cpp/test_tx_tester.py`
against `python/examples/broker.py`.

## Lifecycle

| Application call | Success | Failure |
|------------------|---------|---------|
| `session::transaction_declare()` | `messaging_handler::on_session_transaction_declared` | `messaging_handler::on_session_transaction_error` |
| `session::transaction_commit()` | `messaging_handler::on_session_transaction_committed` | `messaging_handler::on_session_transaction_aborted` with `session::transaction_error()` set |
| `session::transaction_abort()` | `messaging_handler::on_session_transaction_aborted` | (rollback cannot fail at the AMQP level) |

After a successful discharge (commit or abort), `session::transaction_is_declared()`
returns `false`. `session::transaction_id()` returns the coordinator-assigned
identifier while a transaction is declared or discharging, and remains
available for the duration of discharge callbacks; it is cleared when those
callbacks return.

Declare sends `amqp:declare:list` on a coordinator link advertising
`amqp:local-transactions`. Commit and abort send `amqp:discharge:list`
with the transaction id and a `fail` flag (`false` for commit, `true`
for abort).

## Proton session guarantees

These rules are enforced synchronously (they throw `proton::error`):

- **One active transaction per session.** A second `transaction_declare()`
  while a transaction is already active is rejected.
- **No declare with unsettled outgoing deliveries.** All outgoing transfers
  on the session during a transaction are treated as part of that
  transaction; declare is only allowed when every non-coordinator sender
  link on the session has no unsettled deliveries.
- **Discharge requires a declared transaction.** `transaction_commit()` and
  `transaction_abort()` throw if no transaction is in the declared state.

After discharge completes, a new transaction may be declared on the same
session.

## Messaging under a transaction

While `transaction_is_declared()` is true:

- `sender::send()` attaches `amqp:transactional-state:list` to outgoing
  transfers. The peer responds with a provisional outcome; the application
  receives `on_transactional_accept`, `on_transactional_reject`, or
  `on_transactional_release` as appropriate.
- `delivery::accept()`, `reject()`, `release()`, and `modify()` attach
  transactional state instead of settling immediately. Incoming deliveries
  stay unsettled until discharge; `on_delivery_settle` is not raised for
  them before commit or abort.

On **successful commit**:

- Incoming deliveries with provisional dispositions are settled.
- Outgoing provisional outcomes become final: `on_tracker_accept`,
  `on_tracker_reject`, or `on_tracker_release`, followed by
  `on_tracker_settle` when the peer settles.

On **abort or failed commit**:

- Actions taken under the transaction are rolled back (as if they never
  happened).
- Unsettled outgoing trackers on the session are settled locally without
  a disposition update.
- Unsettled incoming deliveries are dispositioned with `modified` (failed)
  by default (`transaction_options::auto_modify_on_abort`, default `true`).

Failed commit is reported through `on_session_transaction_aborted`, not
`on_session_transaction_error`. Use `session::transaction_error()` inside
the aborted callback to distinguish a broker-rejected commit from an
explicit abort.

## Transaction options

`transaction_options::auto_modify_on_abort(bool)` controls whether Proton
automatically modifies unsettled incoming deliveries when a transaction
aborts or a commit discharge is rejected. The default is `true`. When
`false`, the application must update those deliveries itself.

## Test environment

CI runs the script suite against `python/examples/broker.py` with a
2-second transaction timeout (`-t 2`). Scripts are numbered so they
execute in order against a single broker instance; an initial setup
script seeds the target queue.

**Future work (out of scope today):** broker abstraction in the test
harness (e.g. `TX_TESTER_BROKER_CMD` / `TX_TESTER_BROKER_URL`) so the
same scripts can be run against Artemis or other peers; non-fatal
assertions for broker-specific provisional disposition timing.

Provisional disposition callbacks depend on the peer implementing
transactional state promptly. The example broker does; other brokers may
differ.

## Examples

- `cpp/examples/tx_recv.cpp` — transactional receive with commit/abort batches
- `cpp/examples/tx_send.cpp` — transactional send with provisional outcomes
- `tests/cpp/tx_tester.cpp` — scriptable transaction tester used by the suite
