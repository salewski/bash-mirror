/* list.c - Functions for manipulating linked lists of objects. */

/* Copyright (C) 1996-2026 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

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
*/

#include "config.h"

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "shell.h"

/* A global variable which acts as a sentinel for an `error' list return. */
GENERIC_LIST global_error_list;

#ifdef INCLUDE_UNUSED
/* Call FUNCTION on every member of LIST, a generic list. */
void
list_walk (GENERIC_LIST *list, sh_glist_func_t *function)
{
  for ( ; list; list = list->next)
    if ((*function) (list) < 0)
      return;
}

/* Call FUNCTION on every string in WORDS. */
void
wlist_walk (WORD_LIST *words, sh_icpfunc_t *function)
{
  for ( ; words; words = words->next)
    if ((*function) (words->word->word) < 0)
      return;
}
#endif /* INCLUDE_UNUSED */

/* Reverse the chain of structures in LIST.  Output the new head
   of the chain.  You should always assign the output value of this
   function to something, or you will lose the chain. */
void *
list_reverse (void *head)
{
  void *next, *prev, *list;

  prev = NULL;
  next = NULL;
  list = head;

  /* We rely on the fact that the C standard guarantees that structures are
     not padded: the first member of the struct has the same address as the
     struct itself. So *(void **)X is effectively X->next as long as next is
     the first member of struct X. We'll see if this gets past gcc's strict
     aliasing rules. */
  while (list != NULL)
    {
      next = *(void **)list;		/* effectively list->next */
      *(void **)list = prev;		/* effectively set list->next */

      prev = list;
      list = next;
    }

  return (prev);
}

#ifdef INCLUDE_UNUSED
/* Return the number of elements in LIST, a generic list. */
int
list_length (GENERIC_LIST *list)
{
  int i;

  for (i = 0; list; list = list->next, i++);
  return (i);
}
#endif

int
wlist_length (WORD_LIST *list)
{
  int i;

  for (i = 0; list; list = list->next, i++);
  return (i);
}

#if defined (INCLUDE_UNUSED)
/* Append TAIL to HEAD.  Return the header of the list. */
GENERIC_LIST *
list_append (GENERIC_LIST *head, GENERIC_LIST *tail)
{
  register GENERIC_LIST *t_head;

  if (head == 0)
    return (tail);

  for (t_head = head; t_head->next; t_head = t_head->next)
    ;
  t_head->next = tail;
  return (head);
}
#endif

WORD_LIST *
wlist_append (WORD_LIST *head, WORD_LIST *tail)
{
  register WORD_LIST *t_head;

  if (head == 0)
    return (tail);

  for (t_head = head; t_head->next; t_head = t_head->next)
    ;
  t_head->next = tail;
  return (head);
}

#ifdef INCLUDE_UNUSED
/* Delete the element of LIST which satisfies the predicate function COMPARER.
   Returns the element that was deleted, so you can dispose of it, or -1 if
   the element wasn't found.  COMPARER is called with the list element and
   then ARG.  Note that LIST contains the address of a variable which points
   to the list.  You might call this function like this:

   SHELL_VAR *elt = list_remove (&variable_list, check_var_has_name, "foo");
   dispose_variable (elt);
*/
GENERIC_LIST *
list_remove (GENERIC_LIST **list, sh_gcp_func_t *comparer, char *arg)
{
  register GENERIC_LIST *prev, *temp;

  for (prev = (GENERIC_LIST *)NULL, temp = *list; temp; prev = temp, temp = temp->next)
    {
      if ((*comparer) (temp, arg))
	{
	  if (prev)
	    prev->next = temp->next;
	  else
	    *list = temp->next;
	  return (temp);
	}
    }
  return ((GENERIC_LIST *)&global_error_list);
}
#endif
