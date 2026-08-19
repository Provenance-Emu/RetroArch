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
| `CUSTOM` | A pattern over named atoms |

A port is `UINT` 1–65535. An IP is `CUSTOM` `{ipv4}`. A date is `CUSTOM` `{date}`.
Cores add their own atoms instead of new enum cases.

## Cookbook

```c
#include <libretro_core_option_input.h>

static bool RETRO_CALLCONV validate_mac(const char *value)
{
   /* no alloc, no environ */
   (void)value;
   return false;
}

static const struct retro_core_option_input_atom atoms[] = {
   { "hexid", "[0-9A-Fa-f]{1,8}", NULL },
   { "mac", NULL, validate_mac },
   { NULL, NULL, NULL }
};

static const struct retro_core_option_input inputs[] = {
   RETRO_CORE_OPTION_INPUT_DEF_ADDRESS("core_host"),
   RETRO_CORE_OPTION_INPUT_DEF_PORT("core_port"),
   RETRO_CORE_OPTION_INPUT_DEF_CUSTOM("core_serial", "{hexid}"),
   RETRO_CORE_OPTION_INPUT_DEF_CUSTOM("core_nic", "{mac}"),
   { NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL }
};

static const struct retro_core_option_input_set set = { inputs, atoms };

environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTION_INPUTS, (void *)&set);
```

Frontends that implement this env call validate `{hexid}` / `{mac}` without knowing those names. Presentation is stepper for INT/UINT/FLOAT, OSK otherwise.

## Atoms

Built-in (data or a small C validator in the frontend):

| Name | Rule |
| --- | --- |
| `{uint:min-max}` | unsigned, no leading zeros |
| `{ipv4}` | `{uint:0-255}.{uint:0-255}.{uint:0-255}.{uint:0-255}` |
| `{port}` | `{uint:1-65535}` |
| `{ipv6}` | validator |
| `{hostname}` | validator |
| `{date}` | `YYYY-MM-DD` validator |

Core atoms: exactly one of `pattern` or `validate`. `validate` must not allocate or call `environ`.

Pattern language (not PCRE): literals, `[...]`, `?` `*` `+` `{n}` `{n,m}`, named atoms, top-level `|`, one trailing `(...)?`. Caps: pattern 64 bytes, value 256 bytes.

IPv6 plus `:port` is ambiguous. Use a separate `UINT` port option.

See `example_inputs/libretro_core_options.h`.
