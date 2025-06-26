#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct event_t {
  uint32_t pid;
  uint64_t ts;
  char comm[128];
};

static volatile bool running = true;
static volatile int counter = 0;

static void sig_handler(int sig) { running = false; }

static void handle_event(void *ctx, int cpu, void *data, unsigned int data_sz) {
  struct event_t *event = data;
  counter += 1;
  printf("{ \"ts\":%lu, \"pid\":%u,\"command\":\"%s\", \"count\":",event->ts, event->pid, event->comm);
  printf("%d}\n", counter);
}

struct bpf_link *handle_event_link(char *event, char *type,
                                   struct bpf_program *prog) {
  struct bpf_link *link;
  if (!strcmp(type, "kprobe")) {
    link = bpf_program__attach_kprobe(prog, false, event);
    if (!link) {
      printf("failed to attach kprobe \n");
      return NULL;
    }
  } else if (!strcmp(type, "trace")) {
    link = bpf_program__attach_tracepoint(prog, "syscalls", event);
    if (!link) {
      printf("failed to attach tracepoint \n");
      return NULL;
    }
  } else {
    printf("%s is not a thing \n", type);
    return NULL;
  }

  return link;
}

int main(int argc, char *argv[]) {
  struct bpf_object *obj;
  struct bpf_program *prog;
  struct bpf_link *link;
  struct bpf_map *map;
  struct perf_buffer *pb;
  int err;
  int opt;

  char event[128] = {0};
  char etype[128] = {0};

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  while ((opt = getopt(argc, argv, "e:t:")) != -1) {
    switch (opt) {
    case 'e': {
      strcpy(event, optarg);
      break;
    }
    case 't': {
      strcpy(etype, optarg);
      break;
    }
    default:
      printf("invalid param %c \n", opt);
    }
  }
  if (strlen(event) < 1) {
    printf("must have event");
    return 1;
  }
  if (strlen(etype) < 1) {
    printf("must have event type");
    return 1;
  }

  printf("tracking event: %s", event);

  obj = bpf_object__open_file("arc_evict_tracer.o", NULL);
  if (!obj) {
    printf("Failed to open BPF object \n");
  };

  err = bpf_object__load(obj);
  if (err) {
    printf("failed to open BPF object %d\n", err);
    bpf_object__close(obj);
    return 1;
  }

  prog = bpf_object__find_program_by_name(obj, event);
  if (!prog) {
    printf("failed to find program %s \n", event);
    bpf_object__close(obj);
    return 1;
  }

  link = handle_event_link(event, etype, prog);
  if (!link) {
    bpf_object__close(obj);
    return 1;
  }

  map = bpf_object__find_map_by_name(obj, "events");
  if (!map) {
    printf("failed to find events map");
  }

  pb = perf_buffer__new(bpf_map__fd(map), 64, handle_event, NULL, NULL, NULL);
  if (!pb) {
    printf("Failed to create perf buffer\n");
    return 1;
  }

  printf("eBPF program loaded. Tracing calls... Press Ctrl+C to exit\n");

  while (running) {
    err = perf_buffer__poll(pb, 100);
    if (err < 0 && err != -EINTR) {

      printf("Error polling perf buffer %d\n", err);
      break;
    }
  }
  printf("Kill signal caught. Exiting.");
  perf_buffer__free(pb);
  bpf_link__destroy(link);
  bpf_object__close(obj);
  return 0;
}
