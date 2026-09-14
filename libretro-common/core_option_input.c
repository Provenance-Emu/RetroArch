/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (core_option_input.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <libretro_core_option_input.h>

#define COI_ATOMS_MAX     16
#define COI_ALTS_MAX       8
#define COI_HOSTNAME_MAX 253
#define COI_IPV6_MAX      45
#define COI_NAME_MAX      31
#define COI_DEPTH_MAX      8

enum coi_kind
{
   COI_LIT = 0,
   COI_CLASS,
   COI_NAMED,
   COI_UINT_RANGE
};

struct coi_piece
{
   enum coi_kind kind;
   unsigned char lit;
   unsigned char class_bits[32];
   bool negate;
   unsigned min_rep;
   unsigned max_rep;
   char name[COI_NAME_MAX + 1];
   unsigned long umin;
   unsigned long umax;
};

static bool coi_match_pattern(
      const char *pattern, const char *value, bool full, size_t *consumed,
      const struct retro_core_option_input_atom *extra, unsigned depth);

static unsigned coi_clamp_max_length(unsigned max_length)
{
   if (max_length == 0 || max_length > RETRO_CORE_OPTION_INPUT_VALUE_MAX)
      return RETRO_CORE_OPTION_INPUT_VALUE_MAX;
   return max_length;
}

double retro_core_option_input_effective_step(
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

bool retro_core_option_input_parse_int(const char *value, long *out)
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

bool retro_core_option_input_parse_uint(const char *value, unsigned long *out)
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

bool retro_core_option_input_parse_float(const char *value, double *out)
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

static unsigned coi_count_decimals(const char *value)
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

static bool coi_near_step(double value, double min, double step)
{
   double k;
   double snapped;

   if (step <= 0.0)
      return true;

   k       = (value - min) / step;
   snapped = min + floor(k + 0.5) * step;
   return fabs(value - snapped) <= (step * 1e-6 + 1e-9);
}

static bool RETRO_CALLCONV coi_validate_ipv6(const char *value)
{
   const char *p;
   int groups          = 0;
   int compressed      = 0;
   const char *hextet;

   if (!value || !*value)
      return false;
   if (strchr(value, '.') || strchr(value, '%'))
      return false;
   if (strlen(value) > COI_IPV6_MAX)
      return false;

   p = value;

   if (p[0] == ':' && p[1] == ':')
   {
      compressed = 1;
      p         += 2;
      if (!*p)
         return true;
   }
   else if (*p == ':')
      return false;

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
            break;
      }
      else if (!*p)
         return false;
   }

   if (compressed)
   {
      if (groups > 7)
         return false;
   }
   else if (groups != 8)
      return false;

   return groups >= 1 || compressed;
}

static bool coi_hostname_label_ok(const char *start, const char *end)
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

static bool RETRO_CALLCONV coi_validate_hostname(const char *value)
{
   const char *p;
   const char *label;
   size_t total;

   if (!value || !*value)
      return false;

   total = strlen(value);
   if (total > COI_HOSTNAME_MAX)
      return false;

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
         if (!coi_hostname_label_ok(label, p))
            return false;
         p++;
         label = p;
         if (!*p)
            return false;
         continue;
      }
      p++;
   }

   return coi_hostname_label_ok(label, p);
}

static bool coi_is_leap(int year)
{
   return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static bool RETRO_CALLCONV coi_validate_date(const char *value)
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
   if (month == 2 && coi_is_leap(year))
      mdays = 29;

   return day <= mdays;
}

static const struct retro_core_option_input_atom coi_builtins[] = {
   { "ipv4",
     "{uint:0-255}\\.{uint:0-255}\\.{uint:0-255}\\.{uint:0-255}",
     NULL },
   { "port", "{uint:1-65535}", NULL },
   { "ipv6",     NULL, coi_validate_ipv6 },
   { "hostname", NULL, coi_validate_hostname },
   { "date",     NULL, coi_validate_date },
   { NULL, NULL, NULL }
};

static const struct retro_core_option_input_atom *coi_lookup(
      const char *name, const struct retro_core_option_input_atom *extra)
{
   const struct retro_core_option_input_atom *p;

   if (!name || !*name)
      return NULL;

   if (extra)
   {
      for (p = extra; p->name; p++)
      {
         if (p->name[0] && !strcmp(p->name, name))
            return p;
      }
   }

   for (p = coi_builtins; p->name; p++)
   {
      if (!strcmp(p->name, name))
         return p;
   }

   return NULL;
}

static bool coi_parse_uint_atom(
      const char *name, unsigned long *umin, unsigned long *umax)
{
   const char *p;
   char *end = NULL;
   unsigned long a;
   unsigned long b;

   if (!name || strncmp(name, "uint:", 5) != 0 || !umin || !umax)
      return false;

   p = name + 5;
   if (!isdigit((unsigned char)*p))
      return false;

   a = strtoul(p, &end, 10);
   if (!end || end == p || *end != '-')
      return false;

   p = end + 1;
   if (!isdigit((unsigned char)*p))
      return false;

   b = strtoul(p, &end, 10);
   if (!end || end == p || *end != '\0' || b < a)
      return false;

   *umin = a;
   *umax = b;
   return true;
}

static bool coi_uint_prefix(
      const char *s, unsigned long umin, unsigned long umax, size_t *consumed)
{
   size_t slen;
   size_t max_try;
   size_t n;
   char buf[12];
   unsigned long uv;

   if (!s || !consumed)
      return false;

   slen = strlen(s);
   max_try = slen;
   if (max_try > 10)
      max_try = 10;

   for (n = max_try; n >= 1; n--)
   {
      memcpy(buf, s, n);
      buf[n] = '\0';
      if (retro_core_option_input_parse_uint(buf, &uv)
            && uv >= umin && uv <= umax)
      {
         *consumed = n;
         return true;
      }
   }

   return false;
}

static bool coi_fn_prefix(
      retro_core_option_input_validate_t fn, const char *s, size_t *consumed)
{
   size_t slen;
   size_t max_try;
   size_t n;
   char buf[RETRO_CORE_OPTION_INPUT_VALUE_MAX + 1];

   if (!fn || !s || !consumed)
      return false;

   slen = strlen(s);
   max_try = slen;
   if (max_try > RETRO_CORE_OPTION_INPUT_VALUE_MAX)
      max_try = RETRO_CORE_OPTION_INPUT_VALUE_MAX;

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

static void coi_class_set(unsigned char *bits, unsigned char c)
{
   bits[c >> 3] |= (unsigned char)(1u << (c & 7));
}

static bool coi_class_has(const unsigned char *bits, unsigned char c)
{
   return (bits[c >> 3] & (unsigned char)(1u << (c & 7))) != 0;
}

static bool coi_parse_quant(
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
      if (*p == '}' || !isdigit((unsigned char)*p))
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

static bool coi_compile_sequence(
      const char *pattern, size_t plen,
      struct coi_piece *atoms, unsigned *atom_count)
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
      struct coi_piece *a;

      if (count >= COI_ATOMS_MAX)
         return false;

      a = &atoms[count];
      memset(a, 0, sizeof(*a));
      a->min_rep = 1;
      a->max_rep = 1;

      if (*p == '|' || *p == '(' || *p == ')')
         return false;
      if (*p == '.')
         return false;

      if (*p == '{')
      {
         const char *nstart;
         size_t nlen;

         p++;
         nstart = p;
         while (p < end && *p != '}')
         {
            unsigned char c = (unsigned char)*p;
            if (!(isalnum(c) || c == '_' || c == ':' || c == '-'))
               return false;
            p++;
         }
         if (p >= end || *p != '}')
            return false;
         nlen = (size_t)(p - nstart);
         if (nlen == 0 || nlen > COI_NAME_MAX)
            return false;
         memcpy(a->name, nstart, nlen);
         a->name[nlen] = '\0';
         if (coi_parse_uint_atom(a->name, &a->umin, &a->umax))
            a->kind = COI_UINT_RANGE;
         else
            a->kind = COI_NAMED;
         p++;
         if (p < end && (*p == '?' || *p == '*' || *p == '+'
                  || (*p == '{' && isdigit((unsigned char)p[1]))))
            return false;
      }
      else if (*p == '[')
      {
         p++;
         a->kind = COI_CLASS;
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
               unsigned c;
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
               for (c = c1; c <= c2; c++)
                  coi_class_set(a->class_bits, (unsigned char)c);
            }
            else
               coi_class_set(a->class_bits, c1);
         }
         if (p >= end || *p != ']')
            return false;
         p++;
         if (!coi_parse_quant(&p, &a->min_rep, &a->max_rep))
            return false;
      }
      else
      {
         a->kind = COI_LIT;
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

         if (!coi_parse_quant(&p, &a->min_rep, &a->max_rep))
            return false;
      }

      count++;
   }

   *atom_count = count;
   return count > 0;
}

static bool coi_match_one(const struct coi_piece *a, unsigned char c)
{
   bool in_class;

   if (a->kind == COI_LIT)
      return a->lit == c;
   if (a->kind != COI_CLASS)
      return false;

   in_class = coi_class_has(a->class_bits, c);
   return a->negate ? !in_class : in_class;
}

static bool coi_named_prefix(
      const char *name, const char *s, size_t *consumed,
      const struct retro_core_option_input_atom *extra, unsigned depth)
{
   const struct retro_core_option_input_atom *atom;
   unsigned long umin;
   unsigned long umax;

   if (coi_parse_uint_atom(name, &umin, &umax))
      return coi_uint_prefix(s, umin, umax, consumed);

   atom = coi_lookup(name, extra);
   if (!atom)
      return false;

   if (atom->validate)
      return coi_fn_prefix(atom->validate, s, consumed);

   if (atom->pattern)
      return coi_match_pattern(atom->pattern, s, false, consumed, extra, depth + 1);

   return false;
}

static bool coi_match_atoms(
      const struct coi_piece *atoms, unsigned atom_count,
      const char *value, size_t pos, size_t *pos_out,
      const struct retro_core_option_input_atom *extra, unsigned depth)
{
   unsigned ai;
   size_t p = pos;
   size_t vlen;

   if (!atoms || !value || !pos_out)
      return false;

   vlen = strlen(value);

   for (ai = 0; ai < atom_count; ai++)
   {
      const struct coi_piece *a = &atoms[ai];

      if (a->kind == COI_NAMED || a->kind == COI_UINT_RANGE)
      {
         size_t consumed = 0;

         if (p > vlen)
            return false;
         if (a->kind == COI_UINT_RANGE)
         {
            if (!coi_uint_prefix(value + p, a->umin, a->umax, &consumed))
               return false;
         }
         else if (!coi_named_prefix(a->name, value + p, &consumed, extra, depth))
            return false;
         p += consumed;
      }
      else
      {
         unsigned matched = 0;

         while (matched < a->max_rep && p < vlen
               && coi_match_one(a, (unsigned char)value[p]))
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

static bool coi_match_alternative(
      const char *alt, size_t alt_len, const char *value, bool full,
      size_t *consumed, const struct retro_core_option_input_atom *extra,
      unsigned depth)
{
   const char *opt_open  = NULL;
   const char *main_end;
   size_t main_len;
   size_t opt_len = 0;
   struct coi_piece main_atoms[COI_ATOMS_MAX];
   struct coi_piece opt_atoms[COI_ATOMS_MAX];
   unsigned main_count = 0;
   unsigned opt_count  = 0;
   size_t pos          = 0;
   size_t pos2         = 0;
   size_t vlen;

   if (!alt || !value || alt_len == 0)
      return false;

   vlen = strlen(value);

   if (alt_len >= 4 && alt[alt_len - 1] == '?' && alt[alt_len - 2] == ')')
   {
      const char *q;
      int d = 0;

      for (q = alt + (alt_len - 2); q >= alt; q--)
      {
         if (*q == ')')
            d++;
         else if (*q == '(')
         {
            d--;
            if (d == 0)
            {
               opt_open = q;
               break;
            }
         }
      }

      if (!opt_open || d != 0)
         return false;
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

   if (!coi_compile_sequence(alt, main_len, main_atoms, &main_count))
      return false;

   if (!coi_match_atoms(main_atoms, main_count, value, 0, &pos, extra, depth))
      return false;

   if (opt_open)
   {
      if (opt_len == 0)
         return false;
      if (!coi_compile_sequence(opt_open + 1, opt_len, opt_atoms, &opt_count))
         return false;

      if (pos < vlen)
      {
         if (!coi_match_atoms(opt_atoms, opt_count, value, pos, &pos2, extra, depth))
         {
            if (full)
               return false;
         }
         else
            pos = pos2;
      }
   }

   if (full && pos != vlen)
      return false;
   if (consumed)
      *consumed = pos;
   return true;
}

static bool coi_match_pattern(
      const char *pattern, const char *value, bool full, size_t *consumed,
      const struct retro_core_option_input_atom *extra, unsigned depth)
{
   const char *p;
   const char *alt_start;
   size_t plen;
   size_t vlen;
   unsigned alts = 0;

   if (!pattern || !value)
      return false;
   if (depth > COI_DEPTH_MAX)
      return false;

   plen = strlen(pattern);
   vlen = strlen(value);
   if (plen > RETRO_CORE_OPTION_INPUT_PATTERN_MAX)
      return false;
   if (vlen > RETRO_CORE_OPTION_INPUT_VALUE_MAX)
      return false;

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

      if (alts >= COI_ALTS_MAX)
         return false;

      if (coi_match_alternative(alt_start, (size_t)(p - alt_start),
               value, full, consumed, extra, depth))
         return true;

      alts++;
      if (!*p)
         break;
      p++;
      alt_start = p;
   }

   return false;
}

static bool coi_validate_string(
      const struct retro_core_option_input *in, const char *value)
{
   size_t len;
   size_t i;
   unsigned max_len;

   if (!in || !value)
      return false;

   len     = strlen(value);
   max_len = coi_clamp_max_length(in->max_length);

   if (len < in->min_length || len > max_len)
      return false;

   for (i = 0; i < len; i++)
   {
      unsigned char c = (unsigned char)value[i];

      if (in->allowed_chars)
      {
         if (!strchr(in->allowed_chars, (int)c))
            return false;
      }
      else if (!((c >= 0x20 && c != 0x7f) || c >= 0x80))
         return false;
   }

   return true;
}

bool retro_core_option_input_validate_with_atoms(
      const struct retro_core_option_input *in,
      const struct retro_core_option_input_atom *atoms,
      const char *value)
{
   long iv;
   unsigned long uv;
   double fv;
   size_t len;
   unsigned max_len;

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
         return coi_near_step((double)iv, in->min,
               retro_core_option_input_effective_step(in));

      case RETRO_CORE_OPTION_INPUT_UINT:
         if (!retro_core_option_input_parse_uint(value, &uv))
            return false;
         if ((double)uv < in->min || (double)uv > in->max)
            return false;
         return coi_near_step((double)uv, in->min,
               retro_core_option_input_effective_step(in));

      case RETRO_CORE_OPTION_INPUT_FLOAT:
         if (!retro_core_option_input_parse_float(value, &fv))
            return false;
         if (fv < in->min || fv > in->max)
            return false;
         if (in->decimals > 0 && coi_count_decimals(value) > in->decimals)
            return false;
         return coi_near_step(fv, in->min,
               retro_core_option_input_effective_step(in));

      case RETRO_CORE_OPTION_INPUT_STRING:
         return coi_validate_string(in, value);

      case RETRO_CORE_OPTION_INPUT_CUSTOM:
         if (!in->pattern)
            return false;
         len     = strlen(value);
         max_len = coi_clamp_max_length(in->max_length);
         if (len < in->min_length || len > max_len)
            return false;
         return coi_match_pattern(in->pattern, value, true, NULL, atoms, 0);

      default:
         return false;
   }
}

bool retro_core_option_input_validate(
      const struct retro_core_option_input *in, const char *value)
{
   return retro_core_option_input_validate_with_atoms(in, NULL, value);
}
