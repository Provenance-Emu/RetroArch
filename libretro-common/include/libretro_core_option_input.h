#ifndef LIBRETRO_CORE_OPTION_INPUT_H__
#define LIBRETRO_CORE_OPTION_INPUT_H__

/**
 * Header-only helpers for validating typed core option values.
 *
 * No heap allocation. No regex engine. Safe on hostile pattern/value data.
 *
 * Numbers use INT/UINT/FLOAT (min/max/step). Addresses and similar
 * shapes are CUSTOM patterns over named atoms:
 *   {ipv4} {ipv6} {hostname} {port}
 *
 * Common patterns and DEF_* initializer macros are below.
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

#define RETRO_CORE_OPTION_INPUT_ATOMS_MAX     16
#define RETRO_CORE_OPTION_INPUT_ALTS_MAX       8
#define RETRO_CORE_OPTION_INPUT_HOSTNAME_MAX 253
#define RETRO_CORE_OPTION_INPUT_IPV6_MAX      45
#define RETRO_CORE_OPTION_INPUT_IPV4_MAX      15
#define RETRO_CORE_OPTION_INPUT_PORT_MAX       5

/* Named-atom patterns. Use with type CUSTOM. */
#define RETRO_CORE_OPTION_INPUT_PATTERN_IPV4      "{ipv4}"
#define RETRO_CORE_OPTION_INPUT_PATTERN_IPV6      "{ipv6}"
#define RETRO_CORE_OPTION_INPUT_PATTERN_HOSTNAME  "{hostname}"
#define RETRO_CORE_OPTION_INPUT_PATTERN_PORT      "{port}"
#define RETRO_CORE_OPTION_INPUT_PATTERN_ADDRESS   "{hostname}|{ipv4}|{ipv6}"
#define RETRO_CORE_OPTION_INPUT_PATTERN_HOST_PORT \
      "{hostname}(:{port})?|{ipv4}(:{port})?"
#define RETRO_CORE_OPTION_INPUT_PATTERN_HEX8      "[0-9A-Fa-f]{8}"

/* Drop-in rows for a retro_core_option_input[] (C89). */
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
#define RETRO_CORE_OPTION_INPUT_DEF_HEX8(key) \
   { (key), RETRO_CORE_OPTION_INPUT_CUSTOM, 0, 0, 0, 0, 8, 8, NULL, \
         RETRO_CORE_OPTION_INPUT_PATTERN_HEX8 }
#define RETRO_CORE_OPTION_INPUT_DEF_DATE(key) \
   { (key), RETRO_CORE_OPTION_INPUT_DATE, 0, 0, 0, 0, 0, 0, NULL, NULL }

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

   if (!isdigit((unsigned char)*value))
      return false;
   if (value[0] == '0' && value[1] != '\0')
      return false;

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
   bool saw_dot = false;

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
   }

   v = strtod(value, &end);
   if (!end || end == value || *end != '\0')
      return false;

   *out = v;
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

/* ---- address atoms ----------------------------------------------------- */

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

      start = p;
      octet = 0;
      while (isdigit((unsigned char)*p))
      {
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

/**
 * Strict IPv6 (RFC 4291 textual form without zone id or IPv4-mapped tail).
 * Accepts full and compressed (::) forms. Rejects '%', '.', and empty.
 */
static INLINE bool retro_core_option_input_validate_ipv6(const char *value)
{
   const char *p;
   int groups          = 0;
   int compressed      = 0; /* 1 if :: seen */
   int after_compress  = 0;
   const char *hextet;

   if (!value || !*value)
      return false;

   /* Reject IPv4-mapped / zone id early. */
   if (strchr(value, '.') || strchr(value, '%'))
      return false;

   if (strlen(value) > RETRO_CORE_OPTION_INPUT_IPV6_MAX)
      return false;

   p = value;

   /* Leading :: */
   if (p[0] == ':' && p[1] == ':')
   {
      compressed = 1;
      p         += 2;
      if (!*p)
         return true; /* "::" alone = all zeros */
   }
   else if (*p == ':')
      return false; /* single leading colon */

   while (*p)
   {
      hextet = p;
      if (!isxdigit((unsigned char)*p))
         return false;

      while (isxdigit((unsigned char)*p))
      {
         p++;
         if ((size_t)(p - hextet) > 4)
            return false;
      }

      groups++;
      if (compressed)
         after_compress++;

      if (!*p)
         break;

      if (*p != ':')
         return false;
      p++;

      if (*p == ':')
      {
         if (compressed)
            return false;
         compressed = 1;
         p++;
         if (!*p)
            break; /* trailing :: */
      }
      else if (!*p)
         return false; /* trailing single colon */
   }

   if (compressed)
   {
      /* At most 7 explicit groups when compressed. */
      if (groups > 7)
         return false;
   }
   else
   {
      if (groups != 8)
         return false;
   }

   (void)after_compress;
   return groups >= 1 || compressed;
}

static INLINE bool retro_core_option_input_hostname_label_ok(
      const char *start, const char *end)
{
   size_t len = (size_t)(end - start);
   const char *q;

   if (len < 1 || len > 63)
      return false;
   if (start[0] == '-' || end[-1] == '-')
      return false;

   for (q = start; q < end; q++)
   {
      unsigned char c = (unsigned char)*q;
      if (!(isalnum(c) || c == '-'))
         return false;
   }
   return true;
}

static INLINE bool retro_core_option_input_validate_hostname(const char *value)
{
   const char *p;
   const char *label;
   size_t total;

   if (!value || !*value)
      return false;

   total = strlen(value);
   if (total > RETRO_CORE_OPTION_INPUT_HOSTNAME_MAX)
      return false;

   /* No scheme, path, spaces, underscores. */
   if (strchr(value, '/') || strchr(value, ':') || strchr(value, ' ')
         || strchr(value, '_') || strchr(value, '@'))
      return false;

   if (value[0] == '.' || value[total - 1] == '.')
      return false;

   p     = value;
   label = p;
   while (*p)
   {
      if (*p == '.')
      {
         if (!retro_core_option_input_hostname_label_ok(label, p))
            return false;
         p++;
         label = p;
         if (!*p)
            return false;
         continue;
      }
      p++;
   }

   return retro_core_option_input_hostname_label_ok(label, p);
}

static INLINE bool retro_core_option_input_validate_port(const char *value)
{
   unsigned long uv;

   if (!retro_core_option_input_parse_uint(value, &uv))
      return false;
   return uv >= 1 && uv <= 65535;
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

/* ---- named-atom prefix match (longest valid) --------------------------- */

static INLINE bool retro_core_option_input_atom_prefix(
      const char *name, const char *s, size_t *consumed)
{
   size_t slen;
   size_t max_try;
   size_t n;
   char buf[RETRO_CORE_OPTION_INPUT_HOSTNAME_MAX + 1];
   bool (*fn)(const char *) = NULL;

   if (!name || !s || !consumed)
      return false;

   slen = strlen(s);

   if (!strcmp(name, "ipv4"))
   {
      fn      = retro_core_option_input_validate_ipv4;
      max_try = RETRO_CORE_OPTION_INPUT_IPV4_MAX;
   }
   else if (!strcmp(name, "ipv6"))
   {
      fn      = retro_core_option_input_validate_ipv6;
      max_try = RETRO_CORE_OPTION_INPUT_IPV6_MAX;
   }
   else if (!strcmp(name, "hostname"))
   {
      fn      = retro_core_option_input_validate_hostname;
      max_try = RETRO_CORE_OPTION_INPUT_HOSTNAME_MAX;
   }
   else if (!strcmp(name, "port"))
   {
      fn      = retro_core_option_input_validate_port;
      max_try = RETRO_CORE_OPTION_INPUT_PORT_MAX;
   }
   else
      return false;

   if (max_try > slen)
      max_try = slen;

   /* Longest valid prefix. */
   for (n = max_try; n >= 1; n--)
   {
      memcpy(buf, s, n);
      buf[n] = '\0';
      if (fn(buf))
      {
         *consumed = n;
         return true;
      }
   }

   return false;
}

/* ---- CUSTOM possessive pattern matcher --------------------------------- */

enum retro_core_option_input_atom_kind
{
   RETRO_CORE_OPTION_INPUT_ATOM_LIT = 0,
   RETRO_CORE_OPTION_INPUT_ATOM_CLASS,
   RETRO_CORE_OPTION_INPUT_ATOM_NAMED
};

struct retro_core_option_input_atom
{
   enum retro_core_option_input_atom_kind kind;
   unsigned char lit;
   unsigned char class_bits[32];
   bool negate;
   unsigned min_rep;
   unsigned max_rep;
   char name[16]; /* named atom */
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
   /* Quantifier {n} / {n,m} only when '{' is followed by a digit.
    * '{name}' is a named atom for the next compile step. */
   if (*p == '{' && isdigit((unsigned char)p[1]))
   {
      unsigned n = 0;
      unsigned m = 0;
      p++;
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
         return false;
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

static INLINE bool retro_core_option_input_compile_sequence(
      const char *pattern, size_t plen,
      struct retro_core_option_input_atom *atoms,
      unsigned *atom_count)
{
   const char *p;
   const char *end;
   unsigned count = 0;

   if (!pattern || !atoms || !atom_count)
      return false;

   p   = pattern;
   end = pattern + plen;

   while (p < end)
   {
      struct retro_core_option_input_atom *a;

      if (count >= RETRO_CORE_OPTION_INPUT_ATOMS_MAX)
         return false;

      a = &atoms[count];
      memset(a, 0, sizeof(*a));
      a->min_rep = 1;
      a->max_rep = 1;

      if (*p == '|' || *p == '(' || *p == ')')
         return false;
      if (*p == '.')
         return false; /* use \. for literal dot */

      if (*p == '{')
      {
         const char *nstart;
         size_t nlen;

         p++;
         nstart = p;
         while (p < end && *p != '}')
         {
            if (!isalnum((unsigned char)*p) && *p != '_')
               return false;
            p++;
         }
         if (p >= end || *p != '}')
            return false;
         nlen = (size_t)(p - nstart);
         if (nlen == 0 || nlen >= sizeof(a->name))
            return false;
         memcpy(a->name, nstart, nlen);
         a->name[nlen] = '\0';
         if (strcmp(a->name, "ipv4") && strcmp(a->name, "ipv6")
               && strcmp(a->name, "hostname") && strcmp(a->name, "port"))
            return false;
         a->kind = RETRO_CORE_OPTION_INPUT_ATOM_NAMED;
         p++;
         /* Named atoms are not quantified in v1. */
         if (p < end && (*p == '?' || *p == '*' || *p == '+'
                  || (*p == '{' && isdigit((unsigned char)p[1]))))
            return false;
      }
      else if (*p == '[')
      {
         p++;
         a->kind = RETRO_CORE_OPTION_INPUT_ATOM_CLASS;
         if (p < end && *p == '^')
         {
            a->negate = true;
            p++;
         }
         while (p < end && *p != ']')
         {
            unsigned char c1;
            unsigned char c2;

            if (*p == '\\')
            {
               p++;
               if (p >= end)
                  return false;
               c1 = (unsigned char)*p++;
            }
            else
               c1 = (unsigned char)*p++;

            if (p < end && *p == '-' && (p + 1) < end && p[1] != ']')
            {
               p++;
               if (*p == '\\')
               {
                  p++;
                  if (p >= end)
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
         }
         if (p >= end || *p != ']')
            return false;
         p++;
         if (!retro_core_option_input_parse_quant(&p, &a->min_rep, &a->max_rep))
            return false;
      }
      else
      {
         a->kind = RETRO_CORE_OPTION_INPUT_ATOM_LIT;
         if (*p == '\\')
         {
            p++;
            if (p >= end)
               return false;
            a->lit = (unsigned char)*p++;
         }
         else if (*p == ']' || *p == '}' || *p == '?'
               || *p == '*' || *p == '+')
            return false;
         else
            a->lit = (unsigned char)*p++;

         if (!retro_core_option_input_parse_quant(&p, &a->min_rep, &a->max_rep))
            return false;
      }

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
   if (a->kind != RETRO_CORE_OPTION_INPUT_ATOM_CLASS)
      return false;

   in_class = retro_core_option_input_class_has(a->class_bits, c);
   return a->negate ? !in_class : in_class;
}

/**
 * Match atom sequence against @value starting at @pos.
 * On success sets *pos_out to the new offset. Returns false on failure.
 */
static INLINE bool retro_core_option_input_match_atoms(
      const struct retro_core_option_input_atom *atoms,
      unsigned atom_count,
      const char *value,
      size_t pos,
      size_t *pos_out)
{
   unsigned ai;
   size_t p = pos;
   size_t vlen;

   if (!atoms || !value || !pos_out)
      return false;

   vlen = strlen(value);

   for (ai = 0; ai < atom_count; ai++)
   {
      const struct retro_core_option_input_atom *a = &atoms[ai];

      if (a->kind == RETRO_CORE_OPTION_INPUT_ATOM_NAMED)
      {
         size_t consumed = 0;

         if (p > vlen)
            return false;
         if (!retro_core_option_input_atom_prefix(a->name, value + p, &consumed))
            return false;
         p += consumed;
      }
      else
      {
         unsigned matched = 0;

         while (matched < a->max_rep && p < vlen
               && retro_core_option_input_atom_match_one(a,
                     (unsigned char)value[p]))
         {
            p++;
            matched++;
         }

         if (matched < a->min_rep)
            return false;
      }
   }

   *pos_out = p;
   return true;
}

/**
 * Match one alternative: MAIN or MAIN(OPT)?.
 * Pattern slice is [start, end).
 */
static INLINE bool retro_core_option_input_match_alternative(
      const char *alt, size_t alt_len, const char *value)
{
   const char *opt_open  = NULL;
   const char *main_end;
   size_t main_len;
   size_t opt_len = 0;
   struct retro_core_option_input_atom main_atoms[RETRO_CORE_OPTION_INPUT_ATOMS_MAX];
   struct retro_core_option_input_atom opt_atoms[RETRO_CORE_OPTION_INPUT_ATOMS_MAX];
   unsigned main_count = 0;
   unsigned opt_count  = 0;
   size_t pos          = 0;
   size_t pos2         = 0;
   size_t vlen;

   if (!alt || !value || alt_len == 0)
      return false;

   vlen = strlen(value);

   /* Detect single trailing (…)? optional group. */
   if (alt_len >= 4 && alt[alt_len - 1] == '?' && alt[alt_len - 2] == ')')
   {
      const char *q;
      int depth = 0;

      for (q = alt + (alt_len - 2); q >= alt; q--)
      {
         if (*q == ')')
            depth++;
         else if (*q == '(')
         {
            depth--;
            if (depth == 0)
            {
               opt_open = q;
               break;
            }
         }
      }

      if (!opt_open || depth != 0)
         return false;
      /* Only allow the optional group at the very end. */
      if (opt_open + 1 >= alt + alt_len - 2)
         return false;
   }

   if (opt_open)
   {
      main_end = opt_open;
      main_len = (size_t)(main_end - alt);
      opt_len  = (size_t)((alt + alt_len - 2) - (opt_open + 1));
   }
   else
   {
      main_end = alt + alt_len;
      main_len = alt_len;
   }

   if (main_len == 0)
      return false;

   if (!retro_core_option_input_compile_sequence(alt, main_len,
            main_atoms, &main_count))
      return false;

   if (!retro_core_option_input_match_atoms(main_atoms, main_count,
            value, 0, &pos))
      return false;

   if (opt_open)
   {
      if (opt_len == 0)
         return false;
      if (!retro_core_option_input_compile_sequence(opt_open + 1, opt_len,
               opt_atoms, &opt_count))
         return false;

      /* Possessive: try optional first when remaining input exists. */
      if (pos < vlen)
      {
         if (retro_core_option_input_match_atoms(opt_atoms, opt_count,
                  value, pos, &pos2) && pos2 == vlen)
            return true;
         return false;
      }

      return pos == vlen;
   }

   return pos == vlen;
}

static INLINE bool retro_core_option_input_match_pattern(
      const char *pattern, const char *value)
{
   const char *p;
   const char *alt_start;
   size_t plen;
   size_t vlen;
   unsigned alts = 0;

   if (!pattern || !value)
      return false;

   plen = strlen(pattern);
   vlen = strlen(value);
   if (plen > RETRO_CORE_OPTION_INPUT_PATTERN_MAX)
      return false;
   if (vlen > RETRO_CORE_OPTION_INPUT_VALUE_MAX)
      return false;

   /* Split on top-level '|' (not inside [] or {}). */
   p         = pattern;
   alt_start = p;
   while (1)
   {
      int in_class = 0;
      int in_brace = 0;

      while (*p)
      {
         if (*p == '\\' && p[1])
         {
            p += 2;
            continue;
         }
         if (!in_brace && *p == '[')
            in_class = 1;
         else if (in_class && *p == ']')
            in_class = 0;
         else if (!in_class && *p == '{')
            in_brace = 1;
         else if (in_brace && *p == '}')
            in_brace = 0;
         else if (!in_class && !in_brace && *p == '|')
            break;
         p++;
      }

      if (alts >= RETRO_CORE_OPTION_INPUT_ALTS_MAX)
         return false;

      if (retro_core_option_input_match_alternative(alt_start,
               (size_t)(p - alt_start), value))
         return true;

      alts++;
      if (!*p)
         break;
      p++;
      alt_start = p;
   }

   return false;
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
