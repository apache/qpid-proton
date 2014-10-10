#include <proton/object.h>
#include <proton/event.h>
#include <assert.h>

struct pn_collector_t {
  pn_event_t *head;
  pn_event_t *tail;
  pn_event_t *free_head;
  bool freed;
};

struct pn_event_t {
  const pn_class_t *clazz;
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
  collector->freed = false;
}

static void pn_collector_drain(pn_collector_t *collector)
{
  while (pn_collector_peek(collector)) {
    pn_collector_pop(collector);
  }

  assert(!collector->head);
  assert(!collector->tail);
}

static void pn_collector_shrink(pn_collector_t *collector)
{
  pn_event_t *event = collector->free_head;
  while (event) {
    pn_event_t *next = event->next;
    pn_free(event);
    event = next;
  }

  collector->free_head = NULL;
}

static void pn_collector_finalize(void *obj)
{
  pn_collector_t *collector = (pn_collector_t *) obj;
  pn_collector_drain(collector);
  pn_collector_shrink(collector);
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
  pn_collector_t *collector = (pn_collector_t *) pn_class_new(&clazz, sizeof(pn_collector_t));
  return collector;
}

void pn_collector_free(pn_collector_t *collector)
{
  collector->freed = true;
  pn_collector_drain(collector);
  pn_collector_shrink(collector);
  pn_class_decref(PN_OBJECT, collector);
}

pn_event_t *pn_event(void);
static void pn_event_initialize(void *obj);

pn_event_t *pn_collector_put(pn_collector_t *collector,
                             const pn_class_t *clazz, void *context,
                             pn_event_type_t type)
{
  if (!collector) {
    return NULL;
  }

  assert(context);

  if (collector->freed) {
    return NULL;
  }

  pn_event_t *tail = collector->tail;
  if (tail && tail->type == type && tail->context == context) {
    return NULL;
  }

  clazz = clazz->reify(context);

  pn_event_t *event;

  if (collector->free_head) {
    event = collector->free_head;
    collector->free_head = collector->free_head->next;
    pn_event_initialize(event);
  } else {
    event = pn_event();
  }

  if (tail) {
    tail->next = event;
    collector->tail = event;
  } else {
    collector->tail = event;
    collector->head = event;
  }

  event->clazz = clazz;
  event->context = context;
  event->type = type;
  pn_class_incref(clazz, event->context);

  //printf("event %s on %p\n", pn_event_type_name(event->type), event->context);

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
    pn_class_decref(event->clazz, event->context);
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
  event->clazz = NULL;
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
    err = pn_class_inspect(event->clazz, event->context, dst);
    if (err) return err;
  }

  return pn_string_addf(dst, ")");
}

#define pn_event_hashcode NULL
#define pn_event_compare NULL

pn_event_t *pn_event(void)
{
  static const pn_class_t clazz = PN_CLASS(pn_event);
  pn_event_t *event = (pn_event_t *) pn_class_new(&clazz, sizeof(pn_event_t));
  return event;
}

pn_event_type_t pn_event_type(pn_event_t *event)
{
  return event->type;
}

const pn_class_t *pn_event_class(pn_event_t *event)
{
  assert(event);
  return event->clazz;
}

void *pn_event_context(pn_event_t *event)
{
  assert(event);
  return event->context;
}

const char *pn_event_type_name(pn_event_type_t type)
{
  switch (type) {
  case PN_EVENT_NONE:
    return "PN_EVENT_NONE";
  case PN_CONNECTION_INIT:
    return "PN_CONNECTION_INIT";
  case PN_CONNECTION_BOUND:
    return "PN_CONNECTION_BOUND";
  case PN_CONNECTION_UNBOUND:
    return "PN_CONNECTION_UNBOUND";
  case PN_CONNECTION_REMOTE_OPEN:
    return "PN_CONNECTION_REMOTE_OPEN";
  case PN_CONNECTION_OPEN:
    return "PN_CONNECTION_OPEN";
  case PN_CONNECTION_REMOTE_CLOSE:
    return "PN_CONNECTION_REMOTE_CLOSE";
  case PN_CONNECTION_CLOSE:
    return "PN_CONNECTION_CLOSE";
  case PN_CONNECTION_FINAL:
    return "PN_CONNECTION_FINAL";
  case PN_SESSION_INIT:
    return "PN_SESSION_INIT";
  case PN_SESSION_REMOTE_OPEN:
    return "PN_SESSION_REMOTE_OPEN";
  case PN_SESSION_OPEN:
    return "PN_SESSION_OPEN";
  case PN_SESSION_REMOTE_CLOSE:
    return "PN_SESSION_REMOTE_CLOSE";
  case PN_SESSION_CLOSE:
    return "PN_SESSION_CLOSE";
  case PN_SESSION_FINAL:
    return "PN_SESSION_FINAL";
  case PN_LINK_INIT:
    return "PN_LINK_INIT";
  case PN_LINK_REMOTE_OPEN:
    return "PN_LINK_REMOTE_OPEN";
  case PN_LINK_OPEN:
    return "PN_LINK_OPEN";
  case PN_LINK_REMOTE_CLOSE:
    return "PN_LINK_REMOTE_CLOSE";
  case PN_LINK_DETACH:
    return "PN_LINK_DETACH";
  case PN_LINK_REMOTE_DETACH:
    return "PN_LINK_REMOTE_DETACH";
  case PN_LINK_CLOSE:
    return "PN_LINK_CLOSE";
  case PN_LINK_FLOW:
    return "PN_LINK_FLOW";
  case PN_LINK_FINAL:
    return "PN_LINK_FINAL";
  case PN_DELIVERY:
    return "PN_DELIVERY";
  case PN_TRANSPORT:
    return "PN_TRANSPORT";
  case PN_TRANSPORT_ERROR:
    return "PN_TRANSPORT_ERROR";
  case PN_TRANSPORT_HEAD_CLOSED:
    return "PN_TRANSPORT_HEAD_CLOSED";
  case PN_TRANSPORT_TAIL_CLOSED:
    return "PN_TRANSPORT_TAIL_CLOSED";
  case PN_TRANSPORT_CLOSED:
    return "PN_TRANSPORT_CLOSED";
  }

  return "<unrecognized>";
}
