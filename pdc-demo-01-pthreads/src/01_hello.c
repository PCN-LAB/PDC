/*
 * Demo 01 -- pthreads, the smallest honest version.
 *
 *   * create / join
 *   * every thread gets its own stack but shares the heap and globals
 *   * a race, shown, then fixed with a mutex
 *   * what this machine actually looks like underneath
 *
 * Build:  make
 * Run:    ./bin/01_hello [-t THREADS] [-i INCREMENTS]
 */
#include "common.h"
#include "plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>

typedef struct {
    int      id;
    long     iters;
    long    *shared_unsafe;
    long    *shared_safe;
    pthread_mutex_t *lock;
    long     local;
    double   cpu_sec;
} arg_t;

static void *worker(void *p)
{
    arg_t *a = (arg_t *)p;

    for (long i = 0; i < a->iters; i++) {
        (*a->shared_unsafe)++;             /* data race, on purpose */
        pthread_mutex_lock(a->lock);
        (*a->shared_safe)++;               /* correct, but serialised */
        pthread_mutex_unlock(a->lock);
        a->local++;                        /* private, no sharing at all */
    }
    a->cpu_sec = pdc_thread_cpu_sec();
    return NULL;
}

int main(int argc, char **argv)
{
    pdc_bind_stdout();
    int  nthreads = pdc_arg_int(argc, argv, "-t", 4);
    long iters    = (long)pdc_arg_int(argc, argv, "-i", 200000);
    if (nthreads < 1) nthreads = 1;

    pdc_topo_t topo;
    pdc_topology(&topo);

    printf("\n=== Demo 01: pthreads basics =====================================\n\n");
    pdc_topology_print(&topo);
    printf("\n  threads          : %d\n", nthreads);
    printf("  increments/thread: %ld\n\n", iters);

    long shared_unsafe = 0, shared_safe = 0;
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    pthread_t *th = calloc((size_t)nthreads, sizeof(pthread_t));
    arg_t     *ar = calloc((size_t)nthreads, sizeof(arg_t));
    if (!th || !ar) { perror("calloc"); return 1; }

    double t0 = pdc_wall_sec();
    for (int i = 0; i < nthreads; i++) {
        ar[i].id            = i;
        ar[i].iters         = iters;
        ar[i].shared_unsafe = &shared_unsafe;
        ar[i].shared_safe   = &shared_safe;
        ar[i].lock          = &lock;
        if (pthread_create(&th[i], NULL, worker, &ar[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }
    for (int i = 0; i < nthreads; i++) pthread_join(th[i], NULL);
    double wall = pdc_wall_sec() - t0;
    double cpu  = pdc_proc_cpu_sec();

    long expected = (long)nthreads * iters;

    printf("  per-thread CPU time\n");
    for (int i = 0; i < nthreads; i++)
        printf("    thread %-2d      : %7.3f s CPU, %ld private increments\n",
               ar[i].id, ar[i].cpu_sec, ar[i].local);

    printf("\n  expected total   : %ld\n", expected);
    printf("  unsynchronised   : %ld   <-- lost %ld updates (%.1f%%)\n",
           shared_unsafe, expected - shared_unsafe,
           100.0 * (double)(expected - shared_unsafe) / (double)expected);
    printf("  mutex-protected  : %ld   <-- correct\n", shared_safe);

    printf("\n  wall time        : %.3f s\n", wall);
    printf("  process CPU time : %.3f s\n", cpu);
    printf("  CPU / wall       : %.2f  (how many cores were busy on average)\n",
           wall > 0 ? cpu / wall : 0.0);

    printf("\n  Talking points:\n");
    printf("    - CPU/wall > 1 is the only proof so far that anything ran in parallel.\n");
    printf("    - The lost updates are not a bug in pthreads. ++ is load-add-store,\n");
    printf("      and nothing makes those three steps atomic across cores.\n");
    printf("    - The mutex fixes correctness and destroys scalability: every\n");
    printf("      thread queues on one lock. Demo 03 measures what that costs.\n\n");

    pthread_mutex_destroy(&lock);
    free(th); free(ar);
    return 0;
}
