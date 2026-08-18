#ifndef LIBRETRO_CORE_OPTION_INPUT_H__
#define LIBRETRO_CORE_OPTION_INPUT_H__

/**
 * Header-only helpers for validating typed core option values.
 *
 * No heap allocation. No regex engine. Safe on hostile pattern/value data.
 *
 * @see retro_core_option_input
 * @see RETRO_ENVIRONMENT_SET_CORE_OPTION_INPUTS
 */

#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RETRO_CORE_OPTION_INPUT_ATOMS_MAX 16

/* ---- internal helpers -------------------------------------------------- */

static INLINE unsigned retro_core_option_input_clamp_max_length(
      unsigned max_length)
{
   if (max_length == 0 || max_length > RETRO_CORE_OPTION_INPUT_VALUE_MAX)
      return RETRO_CORE_OPTION_INPUT_VALUE_MAX;
   return max_length;
}

static INLINE double retro_core_option_input_effective_step(
      const struct retro_core_option_input *in)
{
   unsigned i;
   double step;

   if (!in)
      return 1.0;

   if (in->step > 0.0)
      return in->step;

   if (in->type == RETRO_CORE_OPTION_INPUT_FLOAT)
   {
      step = 1.0;
      for (i = 0; i < in->decimals && i < 12; i++)
         step *= 0.1;
      return step;
   }

   return 1.0;
}

static INLINE bool retro_core_option_input_parse_int(
      const char *value, long *out)
{
   char *end = NULL;
   long v;

   if (!value || !*value || !out)
      return false;

   /* Optional leading '-', then digits. No '+', no whitespace. */
   if (*value == '-')
   {
      if (!isdigit((unsigned char)value[1]))
         return false;
   }
   else if (!isdigit((unsigned char)*value))
      return false;

   v = strtol(value, &end, 10);
   if (!end || end == value || *end != '\0')
      return false;

   *out = v;
   return true;
}

static INLINE bool retro_core_option_input_parse_uint(
      const char *value, unsigned long *out)
{
   char *end = NULL;
   unsigned long v;

   if (!value || !*value || !out)
      return false;

   /* No signs, no octal/hex prefixes. */
   if (!isdigit((unsigned char)*value))
      return false;
   if (value[0] == '0' && value[1] != '\0')
      return false; /* reject leading zeros except bare "0" */

   v = strtoul(value, &end, 10);
   if (!end || end == value || *end != '\0')
      return false;

   *out = v;
   return true;
}

static INLINE bool retro_core_option_input_parse_float(
      const char *value, double *out)
{
   char *end = NULL;
   double v;
   const char *p;
   unsigned frac = 0;
   bool saw_dot  = false;

   if (!value || !*value || !out)
      return false;

   p = value;
   if (*p == '-' || *p == '+')
      p++;
   if (!*p)
      return false;

   for (; *p; p++)
   {
      if (*p == '.')
      {
         if (saw_dot)
            return false;
         saw_dot = true;
         continue;
      }
      if (!isdigit((unsigned char)*p))
         return false;
      if (saw_dot)
         frac++;
   }

   v = strtod(value, &end);
   if (!end || end == value || *end != '\0')
      return false;

   *out = v;
   /* Caller checks decimals separately when needed. */
   (void)frac;
   return true;
}

static INLINE unsigned retro_core_option_input_count_decimals(
      const char *value)
{
   const char *dot;
   unsigned n = 0;

   if (!value)
      return 0;
   dot = strchr(value, '.');
   if (!dot)
      return 0;
   for (dot++; *dot && isdigit((unsigned char)*dot); dot++)
      n++;
   return n;
}

static INLINE bool retro_core_option_input_near_step(
      double value, double min, double step)
{
   double k;
   double snapped;

   if (step <= 0.0)
      return true;

   k       = (value - min) / step;
   snapped = min + floor(k + 0.5) * step;
   return fabs(value - snapped) <= (step * 1e-6 + 1e-9);
}

static INLINE bool retro_core_option_input_validate_ipv4(const char *value)
{
   unsigned i;
   unsigned octet;
   const char *p;
   const char *start;

   if (!value || !*value)
      return false;

   p = value;
   for (i = 0; i < 4; i++)
   {
      if (i > 0)
      {
         if (*p != '.')
            return false;
         p++;
      }

      if (!isdigit((unsigned char)*p))
         return false;

      start  = p;
      octet  = 0;
      while (isdigit((unsigned char)*p))
      {
         /* No leading zeros except single 0. */
         if (p > start && *start == '0')
            return false;
         octet = octet * 10u + (unsigned)(*p - '0');
         if (octet > 255)
            return false;
         p++;
         if ((size_t)(p - start) > 3)
            return false;
      }
      if (p == start)
         return false;
   }

   return *p == '\0';
}

static INLINE bool retro_core_option_input_is_leap(int year)
{
   return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static INLINE bool retro_core_option_input_validate_date(const char *value)
{
   int year, month, day;
   int mdays;
   static const int days_in_month[] =
         { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

   if (!value)
      return false;

   /* Strict YYYY-MM-DD, digits only in fields. */
   if (strlen(value) != 10)
      return false;
   if (value[4] != '-' || value[7] != '-')
      return false;
   if (!isdigit((unsigned char)value[0]) || !isdigit((unsigned char)value[1])
         || !isdigit((unsigned char)value[2]) || !isdigit((unsigned char)value[3])
         || !isdigit((unsigned char)value[5]) || !isdigit((unsigned char)value[6])
         || !isdigit((unsigned char)value[8]) || !isdigit((unsigned char)value[9]))
      return false;

   year  = (value[0] - '0') * 1000 + (value[1] - '0') * 100
         + (value[2] - '0') * 10 + (value[3] - '0');
   month = (value[5] - '0') * 10 + (value[6] - '0');
   day   = (value[8] - '0') * 10 + (value[9] - '0');

   if (year < 1 || month < 1 || month > 12 || day < 1)
      return false;

   mdays = days_in_month[month];
   if (month == 2 && retro_core_option_input_is_leap(year))
      mdays = 29;

   return day <= mdays;
}

static INLINE bool retro_core_option_input_char_allowed(
      unsigned char c, const char *allowed_chars)
{
   if (allowed_chars)
      return strchr(allowed_chars, (int)c) != NULL;

   /* Printable ASCII, or UTF-8 continuation / lead (>= 0x80). */
   if (c >= 0x20 && c != 0x7f)
      return true;
   if (c >= 0x80)
      return true;
   return false;
}

static INLINE bool retro_core_option_input_validate_string(
      const struct retro_core_option_input *in, const char *value)
{
   size_t len;
   size_t i;
   unsigned max_len;

   if (!in || !value)
      return false;

   len     = strlen(value);
   max_len = retro_core_option_input_clamp_max_length(in->max_length);

   if (len < in->min_length || len > max_len)
      return false;

   for (i = 0; i < len; i++)
   {
      if (!retro_core_option_input_char_allowed(
               (unsigned char)value[i], in->allowed_chars))
         return false;
   }

   return true;
}

/* ---- CUSTOM possessive pattern matcher --------------------------------- */

enum retro_core_option_input_atom_kind
{
   RETRO_CORE_OPTION_INPUT_ATOM_LIT = 0,
   RETRO_CORE_OPTION_INPUT_ATOM_CLASS
};

struct retro_core_option_input_atom
{
   enum retro_core_option_input_atom_kind kind;
   unsigned char lit;
   unsigned char class_bits[32]; /* 256-bit charset */
   bool negate;
   unsigned min_rep;
   unsigned max_rep; /* UINT_MAX-ish capped; 0xFFFFFFFFu = unbounded * */
};

static INLINE void retro_core_option_input_class_set(
      unsigned char *bits, unsigned char c)
{
   bits[c >> 3] |= (unsigned char)(1u << (c & 7));
}

static INLINE bool retro_core_option_input_class_has(
      const unsigned char *bits, unsigned char c)
{
   return (bits[c >> 3] & (unsigned char)(1u << (c & 7))) != 0;
}

static INLINE bool retro_core_option_input_parse_quant(
      const char **pp, unsigned *min_rep, unsigned *max_rep)
{
   const char *p = *pp;

   *min_rep = 1;
   *max_rep = 1;

   if (!p || !*p)
      return true;

   if (*p == '?')
   {
      *min_rep = 0;
      *max_rep = 1;
      *pp = p + 1;
      return true;
   }
   if (*p == '*')
   {
      *min_rep = 0;
      *max_rep = 0xFFFFFFFFu;
      *pp = p + 1;
      return true;
   }
   if (*p == '+')
   {
      *min_rep = 1;
      *max_rep = 0xFFFFFFFFu;
      *pp = p + 1;
      return true;
   }
   if (*p == '{')
   {
      unsigned n = 0;
      unsigned m = 0;
      p++;
      if (!isdigit((unsigned char)*p))
         return false;
      while (isdigit((unsigned char)*p))
      {
         n = n * 10u + (unsigned)(*p - '0');
         if (n > RETRO_CORE_OPTION_INPUT_VALUE_MAX)
            return false;
         p++;
      }
      if (*p == '}')
      {
         *min_rep = n;
         *max_rep = n;
         *pp = p + 1;
         return true;
      }
      if (*p != ',')
         return false;
      p++;
      if (*p == '}')
      {
         /* {n,} unbounded — reject for safety (use * with care or {n,m}) */
         return false;
      }
      if (!isdigit((unsigned char)*p))
         return false;
      while (isdigit((unsigned char)*p))
      {
         m = m * 10u + (unsigned)(*p - '0');
         if (m > RETRO_CORE_OPTION_INPUT_VALUE_MAX)
            return false;
         p++;
      }
      if (*p != '}' || m < n)
         return false;
      *min_rep = n;
      *max_rep = m;
      *pp = p + 1;
      return true;
   }

   return true;
}

static INLINE bool retro_core_option_input_compile_pattern(
      const char *pattern,
      struct retro_core_option_input_atom *atoms,
      unsigned *atom_count)
{
   const char *p;
   unsigned count = 0;

   if (!pattern || !atoms || !atom_count)
      return false;

   if (strlen(pattern) > RETRO_CORE_OPTION_INPUT_PATTERN_MAX)
      return false;

   p = pattern;
   while (*p)
   {
      struct retro_core_option_input_atom *a;

      if (count >= RETRO_CORE_OPTION_INPUT_ATOMS_MAX)
         return false;

      a = &atoms[count];
      memset(a, 0, sizeof(*a));
      a->min_rep = 1;
      a->max_rep = 1;

      if (*p == '|')
         return false;
      if (*p == '(' || *p == ')')
         return false;
      if (*p == '.')
      {
         /* Wildcard '.' is rejected; use a class or literal '\.'. */
         return false;
      }

      if (*p == '[')
      {
         bool first = true;
         p++;
         a->kind = RETRO_CORE_OPTION_INPUT_ATOM_CLASS;
         if (*p == '^')
         {
            a->negate = true;
            p++;
         }
         while (*p && *p != ']')
         {
            unsigned char c1;
            unsigned char c2;

            if (*p == '\\')
            {
               p++;
               if (!*p)
                  return false;
               c1 = (unsigned char)*p++;
            }
            else
               c1 = (unsigned char)*p++;

            if (*p == '-' && p[1] && p[1] != ']')
            {
               p++;
               if (*p == '\\')
               {
                  p++;
                  if (!*p)
                     return false;
                  c2 = (unsigned char)*p++;
               }
               else
                  c2 = (unsigned char)*p++;

               if (c2 < c1)
                  return false;
               {
                  unsigned c;
                  for (c = c1; c <= c2; c++)
                     retro_core_option_input_class_set(a->class_bits,
                           (unsigned char)c);
               }
            }
            else
               retro_core_option_input_class_set(a->class_bits, c1);

            first = false;
            (void)first;
         }
         if (*p != ']')
            return false;
         p++;
      }
      else
      {
         a->kind = RETRO_CORE_OPTION_INPUT_ATOM_LIT;
         if (*p == '\\')
         {
            p++;
            if (!*p)
               return false;
            /* Only \\ \[ \] are defined; other escapes take the next char. */
            a->lit = (unsigned char)*p++;
         }
         else if (*p == ']' || *p == '{' || *p == '}' || *p == '?'
               || *p == '*' || *p == '+')
            return false;
         else
            a->lit = (unsigned char)*p++;
      }

      if (!retro_core_option_input_parse_quant(&p, &a->min_rep, &a->max_rep))
         return false;

      count++;
   }

   *atom_count = count;
   return count > 0;
}

static INLINE bool retro_core_option_input_atom_match_one(
      const struct retro_core_option_input_atom *a, unsigned char c)
{
   bool in_class;

   if (a->kind == RETRO_CORE_OPTION_INPUT_ATOM_LIT)
      return a->lit == c;

   in_class = retro_core_option_input_class_has(a->class_bits, c);
   return a->negate ? !in_class : in_class;
}

static INLINE bool retro_core_option_input_match_pattern(
      const char *pattern, const char *value)
{
   struct retro_core_option_input_atom atoms[RETRO_CORE_OPTION_INPUT_ATOMS_MAX];
   unsigned atom_count = 0;
   unsigned ai;
   const char *p;
   size_t vlen;

   if (!pattern || !value)
      return false;

   vlen = strlen(value);
   if (vlen > RETRO_CORE_OPTION_INPUT_VALUE_MAX)
      return false;

   if (!retro_core_option_input_compile_pattern(pattern, atoms, &atom_count))
      return false;

   p = value;
   for (ai = 0; ai < atom_count; ai++)
   {
      struct retro_core_option_input_atom *a = &atoms[ai];
      unsigned matched = 0;

      /* Possessive: take as many as max_rep allows. */
      while (matched < a->max_rep && *p
            && retro_core_option_input_atom_match_one(a, (unsigned char)*p))
      {
         p++;
         matched++;
      }

      if (matched < a->min_rep)
         return false;
   }

   return *p == '\0';
}

static INLINE bool retro_core_option_input_validate_custom(
      const struct retro_core_option_input *in, const char *value)
{
   size_t len;
   unsigned max_len;

   if (!in || !value || !in->pattern)
      return false;

   len     = strlen(value);
   max_len = retro_core_option_input_clamp_max_length(in->max_length);

   if (len < in->min_length || len > max_len)
      return false;

   return retro_core_option_input_match_pattern(in->pattern, value);
}

/* ---- public API -------------------------------------------------------- */

/**
 * Validate @value against typed input descriptor @in.
 *
 * Returns true if @value is legal for @in.
 * Never allocates. Safe for untrusted @in->pattern / @value.
 */
static INLINE bool retro_core_option_input_validate(
      const struct retro_core_option_input *in, const char *value)
{
   long iv;
   unsigned long uv;
   double fv;

   if (!in || !value)
      return false;

   if (strlen(value) > RETRO_CORE_OPTION_INPUT_VALUE_MAX)
      return false;

   switch (in->type)
   {
      case RETRO_CORE_OPTION_INPUT_INT:
         if (!retro_core_option_input_parse_int(value, &iv))
            return false;
         if ((double)iv < in->min || (double)iv > in->max)
            return false;
         return retro_core_option_input_near_step((double)iv, in->min,
               retro_core_option_input_effective_step(in));

      case RETRO_CORE_OPTION_INPUT_UINT:
         if (!retro_core_option_input_parse_uint(value, &uv))
            return false;
         if ((double)uv < in->min || (double)uv > in->max)
            return false;
         return retro_core_option_input_near_step((double)uv, in->min,
               retro_core_option_input_effective_step(in));

      case RETRO_CORE_OPTION_INPUT_FLOAT:
         if (!retro_core_option_input_parse_float(value, &fv))
            return false;
         if (fv < in->min || fv > in->max)
            return false;
         if (in->decimals > 0
               && retro_core_option_input_count_decimals(value) > in->decimals)
            return false;
         return retro_core_option_input_near_step(fv, in->min,
               retro_core_option_input_effective_step(in));

      case RETRO_CORE_OPTION_INPUT_STRING:
         return retro_core_option_input_validate_string(in, value);

      case RETRO_CORE_OPTION_INPUT_IPV4:
         return retro_core_option_input_validate_ipv4(value);

      case RETRO_CORE_OPTION_INPUT_DATE:
         return retro_core_option_input_validate_date(value);

      case RETRO_CORE_OPTION_INPUT_CUSTOM:
         return retro_core_option_input_validate_custom(in, value);

      default:
         return false;
   }
}

#ifdef __cplusplus
}
#endif

#endif /* LIBRETRO_CORE_OPTION_INPUT_H__ */
