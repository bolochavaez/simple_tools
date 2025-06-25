#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

struct event_t {
  uint32_t pid;
  char comm[16];
};

static volatile bool running = true;

static void sig_handler(int sig) {
  running = false;
}

static void handle_event (void *ctx, int cpu,  void *data, unsigned int data_sz){
  struct event_t * event = data;
  printf("PID: %u, Command: %s\n", event->pid, event->comm);
}


int main() {
  struct bpf_object *obj;
  struct bpf_program *prog;
  struct bpf_link *link;
  struct bpf_map *map;
  struct perf_buffer *pb;
  int err;

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  obj = bpf_object__open_file("arc_evict_tracer.o", NULL);
  if(!obj) {
    printf("Failed to open BPF object \n"); 
  };

  err = bpf_object__load(obj);
  if (err){
    printf("failed to open BPF object %d\n", err);
    bpf_object__close(obj);
    return 1;
  }

  prog = bpf_object__find_program_by_name(obj, "arc_evict");
  if (!prog){
    printf("failed to find program arc_evict \n");
    bpf_object__close(obj);
    return 1;
  }


  link = bpf_program__attach_kprobe(prog, false, "arc_evict");
  if (!link){
    printf("failed to attach kprobe \n");
    bpf_object__close(obj);
    return 1;
  }


  map = bpf_object__find_map_by_name(obj, "events");
  if(!map){
    printf("failed to find events map");
  }

  pb = perf_buffer__new(bpf_map__fd(map), 64, handle_event, NULL, NULL, NULL);
  if (!pb) {
    printf("Failed to create perf buffer\n");
    return 1;

  }

  printf("eBPF program loaded. Tracing arc_evict calls... Press Ctrl+C to exit\n");

  while (running){
    err = perf_buffer__poll(pb, 100);
    if(err < 0&& err != -EINTR) {

      printf("Error polling perf buffer %d\n", err);
      break;
    }
  }

  perf_buffer__free(pb);
  bpf_link__destroy(link);
  bpf_object__close(obj);
  return 0;

}
