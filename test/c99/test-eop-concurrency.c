#include <pthread.h>
#include <stdio.h>

#include "novas.h"

#define THREAD_COUNT 8
#define REQUEST_COUNT 16

typedef struct {
  novas_eop expected[REQUEST_COUNT];
  int id;
  int status;
} worker_state;

static pthread_mutex_t gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_condition = PTHREAD_COND_INITIALIZER;
static int ready_count;
static int is_released;

static int is_same_eop(const novas_eop *actual, const novas_eop *expected) {
  return actual->series == expected->series && actual->leap == expected->leap && actual->xp == expected->xp && actual->yp == expected->yp
        && actual->dut1 == expected->dut1;
}

static int configure_eop(const char *eop_url, const char *leap_path) {
  if(novas_set_leap_list(leap_path))
    return 1;
  return novas_set_eop_url(EOP_C04_IAU2000_0UTC, 2020, eop_url);
}

static void *fetch_eop(void *arg) {
  worker_state *state = (worker_state *) arg;
  int i;

  pthread_mutex_lock(&gate_mutex);
  ready_count++;
  pthread_cond_broadcast(&gate_condition);
  while(is_released == 0) {
    pthread_cond_wait(&gate_condition, &gate_mutex);
  }
  pthread_mutex_unlock(&gate_mutex);

  for(i = 0; i < REQUEST_COUNT; i++) {
    novas_eop eop;
    double jd = 2440000.0 + 100.0 * state->id + i;
    if(novas_fetch_eop(jd, 0, &eop) || !is_same_eop(&eop, &state->expected[i])) {
      state->status = 1;
      return NULL;
    }
  }

  return NULL;
}

int main(void) {
  pthread_t threads[THREAD_COUNT];
  worker_state states[THREAD_COUNT] = {{0}};
  char eop_url[4096];
  char leap_path[4096];
  int i;
  int n;
  int status = 0;

  n = snprintf(eop_url, sizeof(eop_url), "file://%s/EOP_20u24_C04_one_file_1962-now.txt", RESOURCES);
  if(n < 0 || sizeof(eop_url) <= (size_t) n)
    return 1;

  n = snprintf(leap_path, sizeof(leap_path), "%s/leap-seconds.list", RESOURCES);
  if(n < 0 || sizeof(leap_path) <= (size_t) n)
    return 1;

  if(configure_eop(eop_url, leap_path))
    return 1;

  for(i = 0; i < THREAD_COUNT; i++) {
    int request;
    states[i].id = i;
    for(request = 0; request < REQUEST_COUNT; request++) {
      double jd = 2440000.0 + 100.0 * i + request;
      if(novas_fetch_eop(jd, 0, &states[i].expected[request]))
        return 1;
    }
  }

  novas_reset_eop();
  if(configure_eop(eop_url, leap_path))
    return 1;

  for(i = 0; i < THREAD_COUNT; i++) {
    if(pthread_create(&threads[i], NULL, fetch_eop, &states[i]))
      return 1;
  }

  pthread_mutex_lock(&gate_mutex);
  while(ready_count < THREAD_COUNT) {
    pthread_cond_wait(&gate_condition, &gate_mutex);
  }
  is_released = 1;
  pthread_cond_broadcast(&gate_condition);
  pthread_mutex_unlock(&gate_mutex);

  for(i = 0; i < THREAD_COUNT; i++) {
    pthread_join(threads[i], NULL);
    status |= states[i].status;
  }

  novas_reset_eop();
  return status;
}
