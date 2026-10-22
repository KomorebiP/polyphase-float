/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Core-only example: text ownership and a PPFIF round trip. */
#include <polyphase_float.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t value = {0}, restored = {0};
    ppf_exp_t exponent = 0;
    char *digits = NULL, *formatted = NULL;
    FILE *record = NULL;
    int status = EXIT_FAILURE;

    if (ppf_context_init(&ctx) != 0) return status;
    if (ppf_init2_ctx(&value, 80, &ctx) != 0 ||
        ppf_init2_ctx(&restored, 17, &ctx) != 0) goto cleanup;
    if (ppf_set_str(&value, "1.25", 10, PPF_RNDN, &ctx) != 0)
        goto cleanup;

    digits = ppf_get_str(NULL, &exponent, 10, 5, &value, PPF_RNDN, &ctx);
    if (!digits || strcmp(digits, "12500") != 0 || exponent != 1)
        goto cleanup;
    if (ppf_asprintf(&formatted, "value=%.2RNf", &value) < 0 ||
        !formatted || strcmp(formatted, "value=1.25") != 0) goto cleanup;

    record = tmpfile();
    if (!record || ppf_fpif_export(record, &value) != 0 ||
        fflush(record) != 0 || fseek(record, 0L, SEEK_SET) != 0)
        goto cleanup;
    if (ppf_fpif_import(&restored, record, &ctx) != 0 ||
        ppf_cmp(&restored, &value, &ctx) != 0 ||
        ppf_get_prec(&restored) != ppf_get_prec(&value)) goto cleanup;
    if (fclose(record) != 0) { record = NULL; goto cleanup; }
    record = NULL;

    if (printf("digits=%s exponent=%" PRId64 "\n", digits, exponent) < 0 ||
        puts(formatted) == EOF ||
        puts("PPFIF round trip: value and precision preserved") == EOF)
        goto cleanup;
    status = EXIT_SUCCESS;

cleanup:
    if (record) (void)fclose(record);
    free(formatted);
    ppf_free_str(digits);
    ppf_clear(&restored);
    ppf_clear(&value);
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    if (status != EXIT_SUCCESS) fputs("I/O example failed\n", stderr);
    return status;
}
