#ifndef LIBRETRO_CORE_OPTION_INPUT_H__
#define LIBRETRO_CORE_OPTION_INPUT_H__

/**
 * Macros and frontend validator for typed core option inputs.
 *
 * The matcher lives in libretro-common/core_option_input.c.
 * Cores only need the DEF_* / PATTERN_* macros.
 *
 * @see retro_core_option_input
 * @see retro_core_option_input_set
 * @see RETRO_ENVIRONMENT_SET_CORE_OPTION_INPUTS
 */

#include <libretro.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RETRO_CORE_OPTION_INPUT_PATTERN_IPV4      "{ipv4}"
#define RETRO_CORE_OPTION_INPUT_PATTERN_IPV6      "{ipv6}"
#define RETRO_CORE_OPTION_INPUT_PATTERN_HOSTNAME  "{hostname}"
#define RETRO_CORE_OPTION_INPUT_PATTERN_PORT      "{port}"
#define RETRO_CORE_OPTION_INPUT_PATTERN_DATE      "{date}"
#define RETRO_CORE_OPTION_INPUT_PATTERN_ADDRESS   "{hostname}|{ipv4}|{ipv6}"
#define RETRO_CORE_OPTION_INPUT_PATTERN_HOST_PORT \
      "{hostname}(:{port})?|{ipv4}(:{port})?"
#define RETRO_CORE_OPTION_INPUT_PATTERN_HEX8      "[0-9A-Fa-f]{8}"

#define RETRO_CORE_OPTION_INPUT_DEF_UINT(key, minv, maxv, stepv) \
   { (key), RETRO_CORE_OPTION_INPUT_UINT, (minv), (maxv), (stepv), 0, 0, 0, NULL, NULL }
#define RETRO_CORE_OPTION_INPUT_DEF_FLOAT(key, minv, maxv, stepv, dec) \
   { (key), RETRO_CORE_OPTION_INPUT_FLOAT, (minv), (maxv), (stepv), (dec), 0, 0, NULL, NULL }
#define RETRO_CORE_OPTION_INPUT_DEF_PORT(key) \
      RETRO_CORE_OPTION_INPUT_DEF_UINT((key), 1.0, 65535.0, 1.0)
#define RETRO_CORE_OPTION_INPUT_DEF_PERCENT(key) \
      RETRO_CORE_OPTION_INPUT_DEF_UINT((key), 0.0, 100.0, 1.0)
#define RETRO_CORE_OPTION_INPUT_DEF_VOLUME_DB(key) \
      RETRO_CORE_OPTION_INPUT_DEF_FLOAT((key), -80.0, 12.0, 1.0, 1)
#define RETRO_CORE_OPTION_INPUT_DEF_CUSTOM(key, pat) \
   { (key), RETRO_CORE_OPTION_INPUT_CUSTOM, 0, 0, 0, 0, 0, 0, NULL, (pat) }
#define RETRO_CORE_OPTION_INPUT_DEF_IPV4(key) \
      RETRO_CORE_OPTION_INPUT_DEF_CUSTOM((key), RETRO_CORE_OPTION_INPUT_PATTERN_IPV4)
#define RETRO_CORE_OPTION_INPUT_DEF_IPV6(key) \
      RETRO_CORE_OPTION_INPUT_DEF_CUSTOM((key), RETRO_CORE_OPTION_INPUT_PATTERN_IPV6)
#define RETRO_CORE_OPTION_INPUT_DEF_HOSTNAME(key) \
      RETRO_CORE_OPTION_INPUT_DEF_CUSTOM((key), RETRO_CORE_OPTION_INPUT_PATTERN_HOSTNAME)
#define RETRO_CORE_OPTION_INPUT_DEF_ADDRESS(key) \
      RETRO_CORE_OPTION_INPUT_DEF_CUSTOM((key), RETRO_CORE_OPTION_INPUT_PATTERN_ADDRESS)
#define RETRO_CORE_OPTION_INPUT_DEF_HOST_PORT(key) \
      RETRO_CORE_OPTION_INPUT_DEF_CUSTOM((key), RETRO_CORE_OPTION_INPUT_PATTERN_HOST_PORT)
#define RETRO_CORE_OPTION_INPUT_DEF_DATE(key) \
      RETRO_CORE_OPTION_INPUT_DEF_CUSTOM((key), RETRO_CORE_OPTION_INPUT_PATTERN_DATE)
#define RETRO_CORE_OPTION_INPUT_DEF_HEX8(key) \
   { (key), RETRO_CORE_OPTION_INPUT_CUSTOM, 0, 0, 0, 0, 8, 8, NULL, \
         RETRO_CORE_OPTION_INPUT_PATTERN_HEX8 }

double retro_core_option_input_effective_step(
      const struct retro_core_option_input *in);
bool retro_core_option_input_parse_int(const char *value, long *out);
bool retro_core_option_input_parse_uint(const char *value, unsigned long *out);
bool retro_core_option_input_parse_float(const char *value, double *out);

bool retro_core_option_input_validate(
      const struct retro_core_option_input *in, const char *value);
bool retro_core_option_input_validate_with_atoms(
      const struct retro_core_option_input *in,
      const struct retro_core_option_input_atom *atoms,
      const char *value);

#ifdef __cplusplus
}
#endif

#endif
