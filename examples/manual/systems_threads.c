/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Core + POSIX threads: shared immutable input, private mutable state. */
#include <polyphase_float.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const ppf_t *input;
    int success;
} worker_arguments;

static void *square_worker(void *opaque) {
    worker_arguments *args = opaque;
    ppf_context_t ctx;
    ppf_t result = {0};
    if (ppf_context_init(&ctx) != 0) return NULL;
    if (ppf_init2_ctx(&result, 80, &ctx) == 0 &&
        ppf_sqr(&result, args->input, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR &&
        ppf_cmp_ui(&result, 49, &ctx) == 0)
        args->success = 1;
    ppf_clear(&result);
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    return NULL;
}

int main(void) {
    ppf_context_t ctx;
    ppf_t input = {0};
    pthread_t threads[2];
    worker_arguments args[2] = {{&input, 0}, {&input, 0}};
    unsigned started = 0;
    int status = EXIT_FAILURE;

    if (ppf_context_init(&ctx) != 0) return status;
    if (ppf_init2_ctx(&input, 80, &ctx) != 0 ||
        ppf_set_ui(&input, 7, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        goto cleanup;
    for (unsigned i = 0; i < 2; ++i) {
        if (pthread_create(&threads[i], NULL, square_worker, &args[i]) != 0)
            break;
        ++started;
    }
    for (unsigned i = 0; i < started; ++i) {
        /* Do not release a shared input unless every reader has joined. */
        if (pthread_join(threads[i], NULL) != 0) {
            fputs("could not join a worker\n", stderr);
            return EXIT_FAILURE;
        }
    }
    if (started == 2 && args[0].success && args[1].success)
        status = EXIT_SUCCESS;

cleanup:
    ppf_clear(&input);
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    if (status == EXIT_SUCCESS) {
        if (puts("2 workers read one immutable input; both produced 49") == EOF)
            status = EXIT_FAILURE;
    } else {
        fputs("thread example failed\n", stderr);
    }
    return status;
}
