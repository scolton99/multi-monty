#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>

#define TOTAL_THREADS    32
#define TOTAL_PLAYS      1000000000000UL
#define SMOOTHING_FACTOR 0.05

uint64_t plays = 0UL;
uint64_t wins  = 0UL;

struct timespec start;

typedef struct thread_args {
  uint64_t iters;
  uint16_t tid;
  uint32_t seed;
} thread_args_t;

typedef struct thread_data {
  uint64_t plays;
  uint64_t wins;
  uint64_t goal;
} thread_data_t;

double avg_speed;
uint64_t last_plays;
struct timespec last_time;

uint8_t rand_reveal(uint32_t* rand_seed, uint8_t correct, uint8_t first_pick) {
  uint8_t opts[3];

  uint8_t j = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    if (i != correct && i != first_pick)
      opts[j++] = i;
  }

  uint8_t pick = rand_r(rand_seed) % j;

  return opts[pick];
}

uint8_t sw(uint8_t first_pick, uint8_t revealed) {
  for (uint8_t i = 0; i < 3; ++i) {
    if (i != first_pick && i != revealed)
      return i;
  }

  exit(1);
}

bool monty_hall(uint32_t* rand_seed) {
  uint8_t correct = rand_r(rand_seed) % 3;

  uint8_t first_pick = rand_r(rand_seed) % 3;
  uint8_t revealed_door = rand_reveal(rand_seed, correct, first_pick);

  uint8_t second_pick = sw(first_pick, revealed_door);

  return correct == second_pick;
}

void* thread_func(void* arg) {
  thread_args_t* thread_args = (thread_args_t*) arg;
  uint16_t tid = thread_args->tid;

  uint64_t thread_plays = 0, thread_wins = 0;

  for (uint64_t i = 0; i < thread_args->iters; ++i) {
    bool won = monty_hall(&(thread_args->seed));

    ++thread_plays;
    if (won)
      ++thread_wins;

    if (i % 100000 == 0) {
      atomic_fetch_add(&plays, thread_plays);
      atomic_fetch_add(&wins,  thread_wins);

      thread_plays = 0;
      thread_wins = 0;
    }
  }

  atomic_fetch_add(&plays, thread_plays);
  atomic_fetch_add(&wins,  thread_wins);

  pthread_exit(NULL);
}

void print_status() {
  struct timespec current_time;
  clock_gettime(CLOCK_MONOTONIC, &current_time);

  uint64_t current_plays = plays, current_wins = wins;
  // for (uint16_t i = 0; i < TOTAL_THREADS; ++i) {
  //   uint64_t thread_plays = thread_state[i].plays, thread_wins = thread_state[i].wins;
  //   current_plays += thread_plays;
  //   current_wins  += thread_wins;
  //   printf("Thread %2d - Goal: %13lu, Plays: %13lu, Wins: %13lu\n", i + 1, thread_state[i].goal, thread_plays, thread_wins);
  // }

  uint64_t delta_plays = current_plays - last_plays;

  uint16_t ret = /* TOTAL_THREADS + */ 2;
  printf("Goal: %15lu, Plays: %15lu, Wins: %15lu\n", TOTAL_PLAYS, current_plays, current_wins);
  printf("Current win percent: %4.2f\n", 100 * ((double) current_wins / current_plays));

  double elapsed = current_time.tv_sec - last_time.tv_sec;
  elapsed += (current_time.tv_nsec - last_time.tv_nsec) / 1000000000.0;

  double speed = delta_plays / elapsed;
  if (avg_speed == 0.0)
    avg_speed = speed;

  avg_speed = SMOOTHING_FACTOR * speed + (1.0 - SMOOTHING_FACTOR) * avg_speed;

  double total_elapsed = current_time.tv_sec - start.tv_sec;
  total_elapsed += (current_time.tv_nsec - start.tv_nsec) / 1000000000.0;
  
  printf("Recent rate:  %11.0f\n", speed);
  printf("Overall rate: %11.0f\n", current_plays / total_elapsed);
  printf("Avg rate:     %11.0f\n", avg_speed);
  printf("Time:         %11.0f\n", total_elapsed);
  printf("Rem:          %11.0f\n", (TOTAL_PLAYS - current_plays) / avg_speed);

  last_time = current_time;
  last_plays = current_plays;
}

int main() {
  pthread_t threads[TOTAL_THREADS];
  thread_args_t thread_args[TOTAL_THREADS];

  clock_gettime(CLOCK_MONOTONIC, &start);
  last_time = start;
  last_plays = 0;
  avg_speed = 0.0;

  uint32_t start_seed = time(NULL);
  uint64_t iters_so_far = 0UL;
  uint64_t iters_per_std_thread = TOTAL_PLAYS / TOTAL_THREADS;
  for (uint16_t i = 0; i < TOTAL_THREADS; ++i) {
    thread_args[i].tid = i;
    thread_args[i].seed = i + start_seed;

    uint64_t thread_iters;
    if (i == TOTAL_THREADS - 1)
      thread_iters = TOTAL_PLAYS - iters_so_far;
    else
      thread_iters = iters_per_std_thread;

    iters_so_far += thread_iters;
    thread_args[i].iters = thread_iters;

    pthread_create(&threads[i], NULL, thread_func, (void*) &thread_args[i]);
  }

  while (plays < TOTAL_PLAYS) {
    print_status();
    printf("\033[7A");
    usleep(500000);
  }
  
  for (int i = 0; i < TOTAL_THREADS; ++i)
    pthread_join(threads[i], NULL);

  return 0;
}