#include <proton/engine.h>
#include <assert.h>
#include "engine-internal.h"

struct pn_collector_t {
  pn_event_t *head;
  pn_event_t *tail;
  pn_event_t *free_head;
};

struct pn_event_t {
  void *context;    // depends on type
  pn_event_t *next;
  pn_event_type_t type;
};

static void pn_collector_initialize(void *obj)
{
  pn_collector_t *collector = (pn_collector_t *) obj;
  collector->head = NULL;
  collector->tail = NULL;
  collector->free_head = NULL;
}

static void pn_collector_finalize(void *obj)
{
  pn_collector_t *collector = (pn_collector_t *) obj;

  while (pn_collector_peek(collector)) {
    pn_collector_pop(collector);
  }

  assert(!collector->head);
  assert(!collector->tail);

  pn_event_t *event = collector->free_head;
  while (event) {
    pn_event_t *next = event->next;
    pn_free(event);
    event = next;
  }
}

static int pn_collector_inspect(void *obj, pn_string_t *dst)
{
  assert(obj);
  pn_collector_t *collector = (pn_collector_t *) obj;
  int err = pn_string_addf(dst, "EVENTS[");
  if (err) return err;
  pn_event_t *event = collector->head;
  bool first = true;
  while (event) {
    if (first) {
      first = false;
    } else {
      err = pn_string_addf(dst, ", ");
      if (err) return err;
    }
    err = pn_inspect(event, dst);
    if (err) return err;
    event = event->next;
  }
  return pn_string_addf(dst, "]");
}

#define pn_collector_hashcode NULL
#define pn_collector_compare NULL

pn_collector_t *pn_collector(void)
{
  static const pn_class_t clazz = PN_CLASS(pn_collector);
  pn_collector_t *collector = (pn_collector_t *) pn_new(sizeof(pn_collector_t), &clazz);
  return collector;
}

void pn_collector_free(pn_collector_t *collector)
{
  pn_free(collector);
}

pn_event_t *pn_event(void);
static void pn_event_initialize(void *obj);

pn_event_t *pn_collector_put(pn_collector_t *collector, pn_event_type_t type, void *context)
{
  if (!collector) {
    return NULL;
  }

  assert(context);

  pn_event_t *event;

  if (collector->free_head) {
    event = collector->free_head;
    collector->free_head = collector->free_head->next;
    pn_event_initialize(event);
  } else {
    event = pn_event();
  }

  pn_event_t *tail = collector->tail;

  if (tail) {
    tail->next = event;
    collector->tail = event;
  } else {
    collector->tail = event;
    collector->head = event;
  }

  event->type = type;
  event->context = context;
  pn_incref(event->context);

  return event;
}

pn_event_t *pn_collector_peek(pn_collector_t *collector)
{
  return collector->head;
}

bool pn_collector_pop(pn_collector_t *collector)
{
  pn_event_t *event = collector->head;
  if (event) {
    collector->head = event->next;
  } else {
    return false;
  }

  if (!collector->head) {
    collector->tail = NULL;
  }

  // decref before adding to the free list
  if (event->context) {
    pn_decref(event->context);
    event->context = NULL;
  }

  event->next = collector->free_head;
  collector->free_head = event;

  return true;
}

static void pn_event_initialize(void *obj)
{
  pn_event_t *event = (pn_event_t *) obj;
  event->type = PN_EVENT_NONE;
  event->context = NULL;
  event->next = NULL;
}

static void pn_event_finalize(void *obj) {}

static int pn_event_inspect(void *obj, pn_string_t *dst)
{
  assert(obj);
  pn_event_t *event = (pn_event_t *) obj;
  int err = pn_string_addf(dst, "(0x%X", (unsigned int)event->type);
  if (event->context) {
    err = pn_string_addf(dst, ", ");
    if (err) return err;
    err = pn_inspect(event->context, dst);
    if (err) return err;
  }

  return pn_string_addf(dst, ")");
}

#define pn_event_hashcode NULL
#define pn_event_compare NULL

pn_event_t *pn_event(void)
{
  static const pn_class_t clazz = PN_CLASS(pn_event);
  pn_event_t *event = (pn_event_t *) pn_new(sizeof(pn_event_t), &clazz);
  return event;
}

pn_event_type_t pn_event_type(pn_event_t *event)
{
  return event->type;
}

pn_event_category_t pn_event_category(pn_event_t *event)
{
  return (pn_event_category_t)(event->type & 0xFFFF0000);
}

void *pn_event_context(pn_event_t *event)
{
  assert(event);
  return event->context;
}

pn_connection_t *pn_event_connection(pn_event_t *event)
{
  pn_session_t *ssn;
  pn_transport_t *transport;

  switch (pn_event_type(event)) {
  case PN_CONNECTION_REMOTE_STATE:
  case PN_CONNECTION_LOCAL_STATE:
  case PN_CONNECTION_FINAL:
  case PN_CONNECTION_INIT:
    return (pn_connection_t *)event->context;
  case PN_TRANSPORT:
    transport = pn_event_transport(event);
    if (transport)
      return transport->connection;
    return NULL;
  default:
    ssn = pn_event_session(event);
    if (ssn)
     return pn_session_connection(ssn);
  }
  return NULL;
}

pn_session_t *pn_event_session(pn_event_t *event)
{
  pn_link_t *link;
  switch (pn_event_type(event)) {
  case PN_SESSION_REMOTE_STATE:
  case PN_SESSION_LOCAL_STATE:
  case PN_SESSION_FINAL:
  case PN_SESSION_INIT:
    return (pn_session_t *)event->context;
  default:
    link = pn_event_link(event);
    if (link)
      return pn_link_session(link);
  }
  return NULL;
}

pn_link_t *pn_event_link(pn_event_t *event)
{
  pn_delivery_t *dlv;
  switch (pn_event_type(event)) {
  case PN_LINK_REMOTE_STATE:
  case PN_LINK_LOCAL_STATE:
  case PN_LINK_FLOW:
  case PN_LINK_FINAL:
  case PN_LINK_INIT:
    return (pn_link_t *)event->context;
  default:
    dlv = pn_event_delivery(event);
    if (dlv)
      return pn_delivery_link(dlv);
  }
  return NULL;
}

pn_delivery_t *pn_event_delivery(pn_event_t *event)
{
  if (pn_event_type(event) == PN_DELIVERY)
    return (pn_delivery_t *)event->context;
  return NULL;
}

pn_transport_t *pn_event_transport(pn_event_t *event)
{
  if (pn_event_type(event) == PN_TRANSPORT)
    return (pn_transport_t *)event->context;
  pn_connection_t *conn = pn_event_connection(event);
  if (conn)
    return pn_connection_transport(conn);
  return NULL;
}

const char *pn_event_type_name(pn_event_type_t type)
{
  switch (type) {
  case PN_EVENT_NONE:
    return "PN_EVENT_NONE";
  case PN_CONNECTION_INIT:
    return "PN_CONNECTION_INIT";
  case PN_CONNECTION_REMOTE_STATE:
    return "PN_CONNECTION_REMOTE_STATE";
  case PN_CONNECTION_LOCAL_STATE:
    return "PN_CONNECTION_LOCAL_STATE";
  case PN_CONNECTION_FINAL:
    return "PN_CONNECTION_FINAL";
  case PN_SESSION_INIT:
    return "PN_SESSION_INIT";
  case PN_SESSION_REMOTE_STATE:
    return "PN_SESSION_REMOTE_STATE";
  case PN_SESSION_LOCAL_STATE:
    return "PN_SESSION_LOCAL_STATE";
  case PN_SESSION_FINAL:
    return "PN_SESSION_FINAL";
  case PN_LINK_INIT:
    return "PN_LINK_INIT";
  case PN_LINK_REMOTE_STATE:
    return "PN_LINK_REMOTE_STATE";
  case PN_LINK_LOCAL_STATE:
    return "PN_LINK_LOCAL_STATE";
  case PN_LINK_FLOW:
    return "PN_LINK_FLOW";
  case PN_LINK_FINAL:
    return "PN_LINK_FINAL";
  case PN_DELIVERY:
    return "PN_DELIVERY";
  case PN_TRANSPORT:
    return "PN_TRANSPORT";
  }

  return "<unrecognized>";
}
