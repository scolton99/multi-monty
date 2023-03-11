#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>

uint64_t plays = 0UL;
uint64_t wins  = 0UL;

const uint64_t total_plays   = 1000000000UL;
const uint8_t  total_threads = 32;

typedef struct thread_args {
  uint64_t iters;
} thread_args_t;

uint8_t rand_reveal(uint8_t correct, uint8_t first_pick) {
  uint8_t opts[3];

  uint8_t j = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    if (i != correct && i != first_pick)
      opts[j++] = i;
  }

  uint8_t pick = rand() % j;

  return opts[pick];
}

uint8_t sw(uint8_t first_pick, uint8_t revealed) {
  for (uint8_t i = 0; i < 3; ++i) {
    if (i != first_pick && i != revealed)
      return i;
  }

  exit(1);
}

bool monty_hall() {
  uint8_t correct = rand() % 3;

  uint8_t first_pick = rand() % 3;
  uint8_t revealed_door = rand_reveal(correct, first_pick);

  uint8_t second_pick = sw(first_pick, revealed_door);

  return correct == second_pick;
}

void* thread_func(void* arg) {
  thread_args_t* thread_args = (thread_args_t*) arg;

  for (int i = 0; i < thread_args->iters; ++i) {
    bool won = monty_hall();
    atomic_fetch_add(&plays, 1);
    if (won)
      atomic_fetch_add(&wins, 1);
  }
}

void print_status() {
  printf("Plays: %lu, Wins: %lu\r", plays, wins);
}

int main() {
  pthread_t threads[total_threads];
  thread_args_t thread_args[total_threads];

  uint64_t iters_so_far = 0UL;
  uint64_t iters_per_std_thread = total_plays / total_threads;
  for (int i = 0; i < total_threads; ++i) {
    if (i == total_threads - 1) {
      thread_args[i].iters = total_plays - iters_so_far;
      iters_so_far += total_plays - iters_so_far;
    } else {
      thread_args[i].iters = iters_per_std_thread;
      iters_so_far += iters_per_std_thread;
    }

    pthread_create(&threads[i], NULL, thread_func, (void*) &thread_args[i]);
  }

  if (iters_so_far != total_plays)
    exit(2);

  while (plays < total_plays) {
    print_status();
    usleep(33);
  }

  print_status();

  for (int i = 0; i < total_threads; ++i)
    pthread_join(threads[i], NULL);

  printf("\n");

  return 0;
}