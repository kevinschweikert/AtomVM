/***************************************************************************
 *   Copyright 2019 by Fred Dushin <fred@dushin.net>                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#define _GNU_SOURCE

#include "defaultatoms.h"
#include "nifs.h"
#include "memory.h"
#include "term.h"

#include <esp_system.h>

//#define ENABLE_TRACE
#include "trace.h"

#define VALIDATE_VALUE(value, verify_function) \
    if (UNLIKELY(!verify_function((value)))) { \
        argv[0] = ERROR_ATOM; \
        argv[1] = BADARG_ATOM; \
        return term_invalid_term(); \
    }

#define RAISE_ERROR(error_type_atom) \
    ctx->x[0] = ERROR_ATOM; \
    ctx->x[1] = (error_type_atom); \
    return term_invalid_term();

static const char *const esp_rst_unknown_atom   = "\xF"  "esp_rst_unknown";
static const char *const esp_rst_poweron        = "\xF"  "esp_rst_poweron";
static const char *const esp_rst_ext            = "\xB"  "esp_rst_ext";
static const char *const esp_rst_sw             = "\xA"  "esp_rst_sw";
static const char *const esp_rst_panic          = "\xD"  "esp_rst_panic";
static const char *const esp_rst_int_wdt        = "\xF"  "esp_rst_int_wdt";
static const char *const esp_rst_task_wdt       = "\x10" "esp_rst_task_wdt";
static const char *const esp_rst_wdt            = "\xB"  "esp_rst_wdt";
static const char *const esp_rst_deepsleep      = "\x11" "esp_rst_deepsleep";
static const char *const esp_rst_brownout       = "\x10" "esp_rst_brownout";
static const char *const esp_rst_sdio           = "\xC"  "esp_rst_sdio";
//                                                        123456789ABCDEF01

//
// NIFs
//

static term nif_esp_random(Context *ctx, int argc, term argv[])
{
    uint32_t r = esp_random();
    if (UNLIKELY(memory_ensure_free(ctx, BOXED_INT_SIZE) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }
    return term_make_boxed_int(r, ctx);
}

static term nif_esp_random_bytes(Context *ctx, int argc, term argv[])
{
    VALIDATE_VALUE(argv[0], term_is_integer);

    avm_int_t len = term_to_int(argv[0]);
    if (len < 0) {
        RAISE_ERROR(BADARG_ATOM);
    }
    if (len == 0) {
        if (UNLIKELY(memory_ensure_free(ctx, term_binary_data_size_in_terms(0) + BINARY_HEADER_SIZE) != MEMORY_GC_OK)) {
            RAISE_ERROR(OUT_OF_MEMORY_ATOM);
        }
        term binary = term_from_literal_binary(NULL, len, ctx);
        return binary;
    } else {
        uint8_t *buf = malloc(len);
        if (UNLIKELY(IS_NULL_PTR(buf))) {
            RAISE_ERROR(OUT_OF_MEMORY_ATOM);
        }
        esp_fill_random(buf, len);
        if (UNLIKELY(memory_ensure_free(ctx, term_binary_data_size_in_terms(len) + BINARY_HEADER_SIZE) != MEMORY_GC_OK)) {
            RAISE_ERROR(OUT_OF_MEMORY_ATOM);
        }
        term binary = term_from_literal_binary(buf, len, ctx);
        free(buf);
        return binary;
    }
}

static term nif_esp_restart(Context *ctx, int argc, term argv[])
{
    esp_restart();
    return OK_ATOM;
}

static term nif_esp_reset_reason(Context *ctx, int argc, term argv[])
{
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_UNKNOWN:
            return context_make_atom(ctx, esp_rst_unknown_atom);
        case ESP_RST_POWERON:
            return context_make_atom(ctx, esp_rst_poweron);
        case ESP_RST_EXT:
            return context_make_atom(ctx, esp_rst_ext);
        case ESP_RST_SW:
            return context_make_atom(ctx, esp_rst_sw);
        case ESP_RST_PANIC:
            return context_make_atom(ctx, esp_rst_panic);
        case ESP_RST_INT_WDT:
            return context_make_atom(ctx, esp_rst_int_wdt);
        case ESP_RST_TASK_WDT:
            return context_make_atom(ctx, esp_rst_task_wdt);
        case ESP_RST_WDT:
            return context_make_atom(ctx, esp_rst_wdt);
        case ESP_RST_DEEPSLEEP:
            return context_make_atom(ctx, esp_rst_deepsleep);
        case ESP_RST_BROWNOUT:
            return context_make_atom(ctx, esp_rst_brownout);
        case ESP_RST_SDIO:
            return context_make_atom(ctx, esp_rst_sdio);
        default:
            return UNDEFINED_ATOM;
    }
}

//
// NIF structures and distpatch
//

static const struct Nif esp_random_nif =
{
    .base.type = NIFFunctionType,
    .nif_ptr = nif_esp_random
};
static const struct Nif esp_random_bytes_nif =
{
    .base.type = NIFFunctionType,
    .nif_ptr = nif_esp_random_bytes
};
static const struct Nif esp_restart_nif =
{
    .base.type = NIFFunctionType,
    .nif_ptr = nif_esp_restart
};
static const struct Nif esp_reset_reason_nif =
{
    .base.type = NIFFunctionType,
    .nif_ptr = nif_esp_reset_reason
};

const struct Nif *platform_nifs_get_nif(const char *nifname)
{
    TRACE("Resolving platform nif %s ...\n", nifname);
    if (strcmp("esp:random/0", nifname) == 0) {
        return &esp_random_nif;
    }
    if (strcmp("esp:random_bytes/1", nifname) == 0) {
        return &esp_random_bytes_nif;
    }
    if (strcmp("esp:restart/0", nifname) == 0) {
        return &esp_restart_nif;
    }
    if (strcmp("esp:reset_reason/0", nifname) == 0) {
        return &esp_reset_reason_nif;
    }
    TRACE("Platform nif %s not found.\n", nifname);
    return NULL;
}
