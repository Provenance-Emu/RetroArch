# Typed core option inputs

Still core options v2. `GET_CORE_OPTIONS_VERSION` stays at 2.
Probe `RETRO_ENVIRONMENT_SET_CORE_OPTION_INPUTS` with `NULL`.
If it returns false, keep discrete `values[]` lists.

Values stay strings (`GET_VARIABLE` / `SET_VARIABLE` / `*.opt`).

## Types

| Type | Use when |
| --- | --- |
| `INT` / `UINT` / `FLOAT` | A number with min/max/step |
| `STRING` | Free text (`min_length` / `max_length` / `allowed_chars`) |
| `DATE` | Calendar date `YYYY-MM-DD` |
| `CUSTOM` | Everything else: a pattern |

Do not add more enum cases. A port is `UINT` 1–65535. An IP is `CUSTOM` `{ipv4}`.

## Cookbook

```c
#include <libretro_core_option_input.h>

static const struct retro_core_option_input inputs[] = {
   RETRO_CORE_OPTION_INPUT_DEF_ADDRESS("core_host"),
   RETRO_CORE_OPTION_INPUT_DEF_PORT("core_port"),
   RETRO_CORE_OPTION_INPUT_DEF_HOST_PORT("core_endpoint"),
   RETRO_CORE_OPTION_INPUT_DEF_PERCENT("core_opacity"),
   RETRO_CORE_OPTION_INPUT_DEF_VOLUME_DB("core_gain"),
   RETRO_CORE_OPTION_INPUT_DEF_HEX8("core_serial"),
   RETRO_CORE_OPTION_INPUT_DEF_CUSTOM("core_serial_var", "[0-9A-Fa-f]{1,8}"),
   { NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL }
};
```

`values[]` on the v2 definition is still required (at least a default). Extra entries are presets.

## CUSTOM patterns

Not PCRE. Caps: pattern 64 bytes, value 256 bytes.

- literals, `[...]` classes
- `?` `*` `+` `{n}` `{n,m}`
- named atoms: `{ipv4}` `{ipv6}` `{hostname}` `{port}`
- top-level `|`
- one trailing `(...)?`

Shared patterns (same atoms the matcher already uses):

| Macro | Pattern |
| --- | --- |
| `PATTERN_IPV4` | `{ipv4}` |
| `PATTERN_IPV6` | `{ipv6}` |
| `PATTERN_HOSTNAME` | `{hostname}` |
| `PATTERN_ADDRESS` | `{hostname}\|{ipv4}\|{ipv6}` |
| `PATTERN_HOST_PORT` | `{hostname}(:{port})?\|{ipv4}(:{port})?` |
| `PATTERN_HEX8` | `[0-9A-Fa-f]{8}` |

IPv6 plus `:port` is ambiguous on a bare string. Use a separate `UINT` port option.

See `example_inputs/libretro_core_options.h`.
