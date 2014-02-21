#include <proton/engine.h>
#include <assert.h>
#include "engine-internal.h"

struct pn_collector_t {
  pn_event_t *head;
  pn_event_t *tail;
  pn_event_t *free_head;
};

struct pn_event_t {
  pn_event_type_t type;
  pn_connection_t *connection;
  pn_session_t *session;
  pn_link_t *link;
  pn_delivery_t *delivery;
  pn_transport_t *transport;
  pn_event_t *next;
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
  static pn_class_t clazz = PN_CLASS(pn_collector);
  pn_collector_t *collector = (pn_collector_t *) pn_new(sizeof(pn_collector_t), &clazz);
  return collector;
}

void pn_collector_free(pn_collector_t *collector)
{
  pn_free(collector);
}

pn_event_t *pn_event(void);
static void pn_event_initialize(void *obj);

pn_event_t *pn_collector_put(pn_collector_t *collector, pn_event_type_t type)
{
  if (!collector) {
    return NULL;
  }

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

  return event;
}

pn_event_t *pn_collector_peek(pn_collector_t *collector)
{
  // discard any events for objects that no longer exist
  pn_event_t *event = collector->head;
  while (event && ((event->delivery && event->delivery->local.settled)
                   ||
                   (event->link && event->link->endpoint.freed)
                   ||
                   (event->session && event->session->endpoint.freed)
                   ||
                   (event->connection && event->connection->endpoint.freed)
                   ||
                   (event->transport && event->transport->freed))) {
    pn_collector_pop(collector);
    event = collector->head;
  }
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

  event->next = collector->free_head;
  collector->free_head = event;

  if (event->connection) pn_decref(event->connection);
  if (event->session) pn_decref(event->session);
  if (event->link) pn_decref(event->link);
  if (event->delivery) pn_decref(event->delivery);
  if (event->transport) pn_decref(event->transport);

  return true;
}

static void pn_event_initialize(void *obj)
{
  pn_event_t *event = (pn_event_t *) obj;
  event->type = PN_EVENT_NONE;
  event->connection = NULL;
  event->session = NULL;
  event->link = NULL;
  event->delivery = NULL;
  event->transport = NULL;
  event->next = NULL;
}

static void pn_event_finalize(void *obj) {}

static int pn_event_inspect(void *obj, pn_string_t *dst)
{
  assert(obj);
  pn_event_t *event = (pn_event_t *) obj;
  int err = pn_string_addf(dst, "(%d", event->type);
  void *objects[] = {event->connection, event->session, event->link,
                     event->delivery, event->transport};
  for (int i = 0; i < 5; i++) {
    if (objects[i]) {
      err = pn_string_addf(dst, ", ");
      if (err) return err;
      err = pn_inspect(objects[i], dst);
      if (err) return err;
    }
  }

  return pn_string_addf(dst, ")");
}

#define pn_event_hashcode NULL
#define pn_event_compare NULL

pn_event_t *pn_event(void)
{
  static pn_class_t clazz = PN_CLASS(pn_event);
  pn_event_t *event = (pn_event_t *) pn_new(sizeof(pn_event_t), &clazz);
  return event;
}

void pn_event_init_transport(pn_event_t *event, pn_transport_t *transport)
{
  event->transport = transport;
  pn_incref(event->transport);
}

void pn_event_init_connection(pn_event_t *event, pn_connection_t *connection)
{
  event->connection = connection;
  pn_event_init_transport(event, event->connection->transport);
  pn_incref(event->connection);
}

void pn_event_init_session(pn_event_t *event, pn_session_t *session)
{
  event->session = session;
  pn_event_init_connection(event, pn_session_connection(event->session));
  pn_incref(event->session);
}

void pn_event_init_link(pn_event_t *event, pn_link_t *link)
{
  event->link = link;
  pn_event_init_session(event, pn_link_session(event->link));
  pn_incref(event->link);
}

void pn_event_init_delivery(pn_event_t *event, pn_delivery_t *delivery)
{
  event->delivery = delivery;
  pn_event_init_link(event, pn_delivery_link(delivery));
  pn_incref(event->delivery);
}

pn_event_type_t pn_event_type(pn_event_t *event)
{
  return event->type;
}

pn_connection_t *pn_event_connection(pn_event_t *event)
{
  return event->connection;
}

pn_session_t *pn_event_session(pn_event_t *event)
{
  return event->session;
}

pn_link_t *pn_event_link(pn_event_t *event)
{
  return event->link;
}

pn_delivery_t *pn_event_delivery(pn_event_t *event)
{
  return event->delivery;
}

pn_transport_t *pn_event_transport(pn_event_t *event)
{
  return event->transport;
}

const char *pn_event_type_name(pn_event_type_t type)
{
  switch (type) {
  case PN_EVENT_NONE:
    return "PN_EVENT_NONE";
  case PN_CONNECTION_STATE:
    return "PN_CONNECTION_STATE";
  case PN_SESSION_STATE:
    return "PN_SESSION_STATE";
  case PN_LINK_STATE:
    return "PN_LINK_STATE";
  case PN_LINK_FLOW:
    return "PN_LINK_FLOW";
  case PN_DELIVERY:
    return "PN_DELIVERY";
  case PN_TRANSPORT:
    return "PN_TRANSPORT";
  }

  return "<unrecognized>";
}
