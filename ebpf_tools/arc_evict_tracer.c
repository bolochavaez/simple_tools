#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <linux/sched.h>
#include <stdint.h>


struct event_t {
  uint32_t pid;
  char comm[16];
};

struct {
  __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
  __uint(key_size, sizeof(uint32_t));
  __uint(value_size, sizeof(uint32_t));
} events SEC(".maps");


SEC("kprobe/arc_evict")
int arc_evict(void *ctx) {
  struct event_t event = {};
  event.pid = bpf_get_current_pid_tgid() >> 32;
  bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &event, sizeof(event));
  return 0;
}

char _license[] SEC("license") = "GPL";
