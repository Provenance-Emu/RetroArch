#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

/*
 ********************************
 * VERSION: 2.1 (typed inputs)
 ********************************
 *
 * Demonstrates RETRO_ENVIRONMENT_SET_CORE_OPTION_INPUTS:
 * ADDRESS (hostname|ipv4|ipv6), UINT port, float, CUSTOM hex,
 * and CUSTOM host(:port)? composition — plus a discrete fallback
 * for older frontends.
 *
 * - 2.2: ADDRESS / IPV6 / HOSTNAME + composable {atom} patterns
 * - 2.1: Typed freeform inputs via SET_CORE_OPTION_INPUTS
 * - 2.0: Core options v2 categories
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Option definitions used when the frontend supports typed inputs.
 * values[] still required (at least a default); extras are presets.
 * --------------------------------------------------------------------------- */

struct retro_core_option_v2_category option_cats_typed[] = {
   {
      "network",
      "Network",
      "Server address and related settings."
   },
   { NULL, NULL, NULL },
};

struct retro_core_option_v2_definition option_defs_typed[] = {
   {
      "mycore_server_host",
      "Network > Server Host",
      "Server Host",
      "Hostname, IPv4, or IPv6 of the remote server (no port).",
      NULL,
      "network",
      {
         { "127.0.0.1", "localhost IPv4" },
         { "localhost", NULL },
         { "::1", "localhost IPv6" },
         { NULL, NULL },
      },
      "127.0.0.1"
   },
   {
      "mycore_server_port",
      "Network > Server Port",
      "Server Port",
      "UDP/TCP port (1-65535).",
      NULL,
      "network",
      {
         { "26760", "26760 (default)" },
         { "26761", NULL },
         { NULL, NULL },
      },
      "26760"
   },
   {
      "mycore_endpoint",
      "Network > Endpoint",
      "Endpoint",
      "Hostname or IPv4 with optional :port (CUSTOM composition).",
      NULL,
      "network",
      {
         { "127.0.0.1:26760", NULL },
         { "example.com", NULL },
         { NULL, NULL },
      },
      "127.0.0.1:26760"
   },
   {
      "mycore_gain",
      "Network > Gain (dB)",
      "Gain (dB)",
      "Floating-point gain from -80.0 to 12.0 in 0.5 steps.",
      NULL,
      "network",
      {
         { "0.0", "0.0 dB" },
         { "-6.0", "-6.0 dB" },
         { NULL, NULL },
      },
      "0.0"
   },
   {
      "mycore_serial",
      "Network > Serial",
      "Serial",
      "Hex serial ID (1-8 digits).",
      NULL,
      "network",
      {
         { "DEADBEEF", NULL },
         { "00", NULL },
         { NULL, NULL },
      },
      "DEADBEEF"
   },
   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};

struct retro_core_options_v2 options_typed = {
   option_cats_typed,
   option_defs_typed
};

static const struct retro_core_option_input option_inputs[] = {
   {
      "mycore_server_host",
      RETRO_CORE_OPTION_INPUT_ADDRESS,
      0, 0, 0, 0, 0, 0, NULL, NULL
   },
   {
      "mycore_server_port",
      RETRO_CORE_OPTION_INPUT_UINT,
      1.0, 65535.0, 1.0, 0, 0, 0, NULL, NULL
   },
   {
      "mycore_endpoint",
      RETRO_CORE_OPTION_INPUT_CUSTOM,
      0, 0, 0, 0, 1, 64, NULL,
      "{hostname}(:{port})?|{ipv4}(:{port})?"
   },
   {
      "mycore_gain",
      RETRO_CORE_OPTION_INPUT_FLOAT,
      -80.0, 12.0, 0.5, 1, 0, 0, NULL, NULL
   },
   {
      "mycore_serial",
      RETRO_CORE_OPTION_INPUT_CUSTOM,
      0, 0, 0, 0, 1, 8, NULL, "[0-9A-Fa-f]{1,8}"
   },
   { NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL }
};

/* ---------------------------------------------------------------------------
 * Fallback definitions for frontends without typed inputs.
 * IP is split into octets (same pattern as Dolphin DSU).
 * --------------------------------------------------------------------------- */

struct retro_core_option_v2_definition option_defs_fallback[] = {
   {
      "mycore_server_ip_octet1",
      "Network > Server IP Octet 1",
      "Server IP Octet 1",
      "First octet (legacy frontend fallback).",
      NULL,
      "network",
      {
         { "127", NULL },
         { "192", NULL },
         { NULL, NULL },
      },
      "127"
   },
   {
      "mycore_server_ip_octet2",
      "Network > Server IP Octet 2",
      "Server IP Octet 2",
      "Second octet (legacy frontend fallback).",
      NULL,
      "network",
      {
         { "0", NULL },
         { "168", NULL },
         { NULL, NULL },
      },
      "0"
   },
   {
      "mycore_server_ip_octet3",
      "Network > Server IP Octet 3",
      "Server IP Octet 3",
      "Third octet (legacy frontend fallback).",
      NULL,
      "network",
      {
         { "0", NULL },
         { "1", NULL },
         { NULL, NULL },
      },
      "0"
   },
   {
      "mycore_server_ip_octet4",
      "Network > Server IP Octet 4",
      "Server IP Octet 4",
      "Fourth octet (legacy frontend fallback).",
      NULL,
      "network",
      {
         { "1", NULL },
         { "10", NULL },
         { NULL, NULL },
      },
      "1"
   },
   {
      "mycore_server_port",
      "Network > Server Port",
      "Server Port",
      "Limited port presets on legacy frontends.",
      NULL,
      "network",
      {
         { "26760", "26760 (default)" },
         { "26761", NULL },
         { "26762", NULL },
         { NULL, NULL },
      },
      "26760"
   },
   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};

struct retro_core_options_v2 options_fallback = {
   option_cats_typed,
   option_defs_fallback
};

/**
 * libretro_set_core_options:
 *
 * Registers core options. If the frontend supports typed inputs
 * (SET_CORE_OPTION_INPUTS probe returns true), registers freeform
 * IP/port/gain/serial options and attaches input descriptors.
 * Otherwise falls back to discrete value lists / octet split.
 */
static INLINE void libretro_set_core_options(retro_environment_t environ_cb,
      bool *categories_supported)
{
   unsigned version = 0;
   bool typed_inputs_supported = false;

   if (!environ_cb || !categories_supported)
      return;

   *categories_supported = false;

   if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
      version = 0;

   /* Probe typed-input support without registering descriptors yet. */
   typed_inputs_supported =
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTION_INPUTS, NULL);

   if (version >= 2)
   {
      if (typed_inputs_supported)
      {
         *categories_supported = environ_cb(
               RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options_typed);
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTION_INPUTS,
               (void *)option_inputs);
      }
      else
      {
         *categories_supported = environ_cb(
               RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options_fallback);
      }
   }
   else if (version >= 1)
   {
      /* v1 path omitted for brevity in this sample; use typed or
       * fallback definition arrays converted by the core as needed. */
      (void)typed_inputs_supported;
   }
}

#ifdef __cplusplus
}
#endif

#endif
