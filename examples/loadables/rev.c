/* rev - reverse lines in a file or files character by character */

/*
 * Copyright (c) 1987, 1992 The Regents of the University of California.
 * Copyright (C) 2026 Free Software Foundation, Inc.

	 Bash is free software: you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation, either version 3 of the License, or
	 (at your option) any later version.

	 Bash is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.

	 You should have received a copy of the GNU General Public License
	 along with Bash.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Modified for Linux by Charles Hannum (mycroft@gnu.ai.mit.edu)
 *                   and Brian Koehmstedt (bpk@gnu.ai.mit.edu)
 *
 * Wed Sep 14 22:26:00 1994: Patch from bjdouma <bjdouma@xs4all.nl> to handle
 *                           last line that has no newline correctly.
 * 3-Jun-1998: Patched by Nicolai Langfeldt to work better on Linux:
 *	Handle any-length-lines.  Code copied from util-linux' setpwnam.c
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 *	added Native Language Support
 * 1999-09-19 Bruno Haible <haible@clisp.cons.org>
 *	modified to work correctly in multi-byte locales
 * July 2010 - Davidlohr Bueso <dave@gnu.org>
 *      Fixed memory leaks (including Linux signal handling)
 *      Added some memory allocation error handling
 *      Lowered the default buffer size to 256, instead of 512 bytes
 *      Changed tab indentation to 8 chars for better reading the code
 * 2026/03/24 02:17:26: Duncan Roe (duncan_roe@optusnet.com.au)
 *                      Increase speed by using read(2) and processing
 *                      multi-byte characters locally.
 *                      Initial version only handles UTF-8 encoding.
 * 2026/04/04 01:52:47: Duncan Roe (duncan_roe@optusnet.com.au)
 *                      Convert into a bash loadable builtin.
 */

/* Headers */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shmbutil.h"
#include "loadables.h"
#include <array.h>                 /* Has to go after stdint & loadables (!) */

/* Macros */

#define SYSCALL(x, y) do x = y; while(x == -1 && errno == EINTR)
#define PUTC(x) if (v) *buf++ = x; else fputc(x, stdout)

/* ********************************* getlen ********************************* */

static int
getlen(char *last_trlg_byte, int num_bytes_left)
/* Get the length of a UTF-8 sequence */
/*
 * If last_trlg_byte is indeed the last byte of a valid UTF-8 multibyte
 * sequence, return the length of that sequence. Otherwise return 1.
 *
 * There can be up to 3 trailing bytes, which must start '10'b and carry 6 bits
 * of data. The header byte starts with as many 1 bits as there are bytes in the
 * sequence, followed by a 0 bit. The rest of the byte carries data.
 * As an example, a 4-byte sequence starts '11110'b leaving 3 bits for data.
 * 3 trailing bytes carry 6 bits each for a total of 21 bits.
 * UTF-16 can only encode 20 bits, so there are very few 21-bit codepoints.
 */
{

  const char mask[5] = { 0200, 0300, 0340, 0360, 0370 };
  char *p = last_trlg_byte;
  int n;                           /* Bytes in header + trailer(s) */
  int i;

  if ((*p-- & mask[1]) != mask[0])
    goto not_utf_8;
  n = 2;
  for (i = num_bytes_left >= 3 ? 3 : num_bytes_left; i > 0; i--, p--, n++)
  {                                /* 3 more bytes max */
    if ((*p & mask[1]) != mask[0])
    {
      if ((*p & mask[n]) == mask[n - 1])
        return n;
      else
        goto not_utf_8;
    }                              /* if ((*p-- & mask[1]) != mask[0]) */
  }

not_utf_8:
  return 1;
}                                  /* getlen() */

/* ****************************** reverse_line ****************************** */

static void
reverse_line(SHELL_VAR *v, arrayind_t *ind, char *line, size_t len,
  int outputsep, char sep)
{
  char *p, *q;
  char *buf;
  int i, j;
#if defined (ARRAY_VARS)
  if (v)
  {
    /*
     * Bypass extra copies and malloc / free calls by getting a shell var
     * with NULL value and putting an allocated buffer in it.
     */
    bind_array_element (v, (*ind)++, (char *)NULL, 0);
    buf = xmalloc(len + 1);        /* +1 for NUL */
    (((ARRAY *)v->value)->lastref)->value = buf;
    buf[len] = '\0';
  }                                /* if (v) */
#endif

  if (locale_utf8locale)
  {
    for (i = len, p = line + len - 1; i > 0; i--, p--)
    {
      if (*p & 0200)
      {
        j = getlen(p, i);
        p = q = p - (j - 1);       /* p-> 1st byte of seq */
        i -= (j - 1);              /* Reduce num left by num trlg bytes */
        for (; j > 0; j--)
          PUTC(*q++);
      }                            /* if (*p & 0200) */
      else
        PUTC(*p);
    }                        /* for (i = len, p = line + len - 1; i > 0; i--) */
  }                                /* if (locale_utf8locale) */
  else
  {
    for (i = len, p = line + len - 1; i > 0; i--)
      PUTC(*p--);
  }                                /* if (locale_utf8locale) else */
  if (outputsep)
    PUTC(sep);
}                                  /* reverse_line() */

/* ****************************** rev_internal ****************************** */

static int
rev_internal(WORD_LIST *list)
{
  int unbuffered_read;
  char *array_name;
  arrayind_t ind;
  int outputsep;
  WORD_LIST *l;
  SHELL_VAR *v;
  size_t llen;
  char *line;
  size_t n;
  int rval;
  char sep;
  int opt;
  int fd;

  v = 0;
  rval = EXECUTION_SUCCESS;

  array_name = 0;
  sep = '\n';
  ind = 0;

  reset_internal_getopt();
  while ((opt = internal_getopt(list, "0:a:h")) != -1)
    switch (opt)
    {
      case '0':
        sep = '\0';
        break;
      case 'a':
#if defined (ARRAY_VARS)
        array_name = list_optarg;
        break;
#else
        builtin_error("arrays not available");
        return (EX_USAGE);
#endif
        CASE_HELPOPT;
      default:
        builtin_usage();
        return (EX_USAGE);
    }

  if (array_name && (valid_identifier(array_name) == 0))
  {
    sh_invalidid(array_name);
    return (EXECUTION_FAILURE);
  }

#if defined (ARRAY_VARS)
  if (array_name)
  {
    v = builtin_find_indexed_array(array_name, 1);
    if (v == 0)
      return (EXECUTION_FAILURE);
  }
#endif

  l = loptend;
  line = 0;
  llen = 0;

  do
  {
/* for each file */

    if (l == 0)
      fd = 0;
    else
      SYSCALL(fd, open(l->word->word, O_RDONLY));
    if (fd == -1)
    {
      file_error(l->word->word);
      rval = EXECUTION_FAILURE;
      goto next_file;
    }

#ifndef __CYGWIN__
    unbuffered_read = (lseek(fd, 0L, SEEK_CUR) < 0) && (errno == ESPIPE);
#else
    unbuffered_read = 1;
#endif

/* Read from input */
    while ((n = zgetline(fd, &line, &llen, sep, unbuffered_read)) != -1)
    {
      QUIT;
      if (line[n] == sep)
        outputsep = 1;
      else
      {
        outputsep = 0;
        n++;           /* Work around zgetline behaviour on unterminated line */
      }
      reverse_line(v, &ind, line, n, outputsep, sep);
    }                              /* while ((n = zgetline(...) !=-1) */
    if (fd != 0)
      close(fd);

  next_file:
    QUIT;
    if (l)
      l = l->next;
  }                                /* do */
  while (l);

  free(line);
  return rval;
}                                  /* rev_internal() */

/* ********************************** main ********************************** */

int
rev_builtin(WORD_LIST *list)
{
  return rev_internal(list);
}                                  /* main() */

char *rev_doc[] = {
  "Reverse lines characterwise.",
  "",
  "Copy the lines of the specified files to standard output,",
  "or assign them to the indexed array ARRAY starting at index 0,",
  "reversing the order of characters in every line.",
  "If no files are specified, standard input is read.",
  "",
  "When -0 is specified, use the byte '\\0' as line separator.",
  "",
  "When -a is specified, assign each reversed line"
    "to successive elements of ARRAY,",
  "beginning at 0.",
  "The lines rev assigns to ARRAY are identical to the lines it would",
  "write to the standard output if -a were not supplied.",
  "",
  "This utility processes UTF-8 without using a wide-character buffer.",
  (char *)NULL
};

struct builtin rev_struct = {
  "rev",                           /* builtin name */
  rev_builtin,                     /* function implementing the builtin */
  BUILTIN_ENABLED,                 /* initial flags for builtin */
  rev_doc,                         /* array of long documentation strings */
  "rev [-0] [-a ARRAY] [file ...]", /* usage synopsis; becomes short_doc */
  0                                /* reserved for internal use */
};
