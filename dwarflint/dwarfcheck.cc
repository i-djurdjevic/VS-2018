/*
   Copyright (C) 2010, 2011 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "highlevel_check.hh"
#include "all-dies-it.hh"
#include "option.hh"
#include "pri.hh"
#include "files.hh"

#include <libintl.h>

#include <sstream>
#include <bitset>
#include <string>
#include <unistd.h>

using elfutils::dwarf;

#define DIE_OPTSTRING				\
  "}[,...]"

global_opt<void_option> opt_check_dbg_lines
  ("Report bugs (if any) for generated debug lines \
    in .debug_lines DWARF section.",
   "check-debug-lines");

global_opt<void_option> opt_check_dbg_variables
  ("Report bugs (if any) for generated debug locations \
in .debug_loc or .debug_info DWARF sections about variables.",
   "check-debug-vars");

// where.c needs to know how to format certain wheres.  The module
// doesn't know that we don't use these :)
extern "C"
bool
show_refs ()
{
  return false;
}

#define DIE_TYPES		\
  TYPE(single_addr)		\
  TYPE(artificial)		\
  TYPE(inlined)			\
  TYPE(inlined_subroutine)	\
  TYPE(no_coverage)		\
  TYPE(mutable)			\
  TYPE(immutable)

struct tabrule
{
  int start;
  int step;
  tabrule (int a_start, int a_step)
    : start (a_start), step (a_step)
  {}
  bool operator < (tabrule const &other) const {
    return start < other.start;
  }
};

// Sharp 0.0% coverage (i.e. not a single address byte is covered)
const int cov_00 = -1;

struct tabrules_t
  : public std::vector<tabrule>
{
  tabrules_t (std::string const &rule)
  {
    std::stringstream ss;
    ss << rule;

    std::string item;
    while (std::getline (ss, item, ','))
      {
	if (item.empty ())
	  continue;
	int start;
	int step;
	char const *ptr = item.c_str ();

	if (item.length () >= 3
	    && std::strncmp (ptr, "0.0", 3) == 0)
	  {
	    start = cov_00;
	    ptr += 3;
	  }
	else
	  start = std::strtol (ptr, const_cast<char **> (&ptr), 10);

	if (*ptr == 0)
	  step = 0;
	else
	  {
	    if (*ptr != ':')
	      {
		step = 0;
		goto garbage;
	      }
	    else
	      ptr++;

	    step = std::strtol (ptr, const_cast<char **> (&ptr), 10);
	    if (*ptr != 0)
	    garbage:
	      std::cerr << "Ignoring garbage at the end of the rule item: '"
			<< ptr << '\'' << std::endl;
	  }

	push_back (tabrule (start, step));
      }

    push_back (tabrule (100, 0));
    std::sort (begin (), end ());
  }

  void next ()
  {
    if (at (0).step == 0)
      erase (begin ());
    else
      {
	if (at (0).start == cov_00)
	  at (0).start = 0;
	at (0).start += at (0).step;
	if (size () > 1)
	  {
	    if (at (0).start > at (1).start)
	      erase (begin ());
	    while (size () > 1
		   && at (0).start == at (1).start)
	      erase (begin ());
	  }
      }
  }

  bool match (int value) const
  {
    return at (0).start == value;
  }
};

#define TYPE(T) dt_##T,
  enum die_type_e
    {
      DIE_TYPES
      dt__count
    };
#undef TYPE

class die_type_matcher
  : public std::bitset<dt__count>
{
  class invalid {};
  std::pair<die_type_e, bool>
  parse (std::string &desc)
  {
    bool val = true;
    if (desc == "")
      throw invalid ();

#define TYPE(T)					\
    if (desc == #T)				\
      return std::make_pair (dt_##T, val);
    DIE_TYPES
#undef TYPE

      throw invalid ();
  }

public:
  die_type_matcher (std::string const &rule)
  {
    std::stringstream ss;
    ss << rule;

    std::string item;
    while (std::getline (ss, item, ','))
      try
	{
	  std::pair<die_type_e, bool> const &ig = parse (item);
	  set (ig.first, ig.second);
	}
      catch (invalid &i)
	{
	  std::cerr << "Invalid die type: " << item << std::endl;
	}
  }
};

class mutability_t
{
  bool _m_is_mutable;
  bool _m_is_immutable;

public:
  mutability_t ()
    : _m_is_mutable (false)
    , _m_is_immutable (false)
  {
  }

  void set (bool what)
  {
    if (what)
      _m_is_mutable = true;
    else
      _m_is_immutable = true;
  }

  void set_both ()
  {
    set (true);
    set (false);
  }

  void locexpr (Dwarf_Op *expr, size_t len)
  {
    // We scan the expression looking for DW_OP_{bit_,}piece
    // operators which mark ends of sub-expressions to us.
    bool m = false;
    for (size_t i = 0; i < len; ++i)
      switch (expr[i].atom)
	{
	case DW_OP_implicit_value:
	case DW_OP_stack_value:
	  m = true;
	  break;

	case DW_OP_bit_piece:
	case DW_OP_piece:
	  set (m);
	  m = false;
	  break;
	};
    set (m);
  }

  bool is_mutable () const { return _m_is_mutable; }
  bool is_immutable () const { return _m_is_immutable; }
};

struct error
  : public std::runtime_error
{
  explicit error (std::string const &what_arg)
    : std::runtime_error (what_arg)
  {}
};

// Look through the stack of parental dies and return the non-empty
// ranges instance closest to the stack top (i.e. die_stack.end ()).
dwarf::ranges
find_ranges (std::vector<dwarf::debug_info_entry> const &die_stack)
{
  for (auto it = die_stack.rbegin (); it != die_stack.rend (); ++it)
    if (!it->ranges ().empty ())
      return it->ranges ();
  throw error ("no ranges for this DIE");
}

bool
is_inlined (dwarf::debug_info_entry const &die)
{
  dwarf::debug_info_entry::attributes_type::const_iterator it
    = die.attributes ().find (DW_AT_inline);
  if (it != die.attributes ().end ())
    {
      char const *name = (*it).second.dwarf_constant ().name ();
      return std::strcmp (name, "declared_inlined") == 0
	|| std::strcmp (name, "inlined") == 0;
    }
  return false;
}

void
process(Dwarf *c_dw, dwarf const &dw)
{
  if (opt_check_dbg_lines.seen()) {
    std::cout << "****CHECKING DEBUG LINES****\n";
  } else if (opt_check_dbg_variables.seen()) {
    std::cout << "****CHECKING DEBUG VARIABLES****\n";
  }
}

int
main(int argc, char *argv[])
{
  std::cout << "WE ARE RUNNING dwarfcheck !!!!!\n";
  /* Set locale.  */
  setlocale (LC_ALL, "");

  /* Initialize the message catalog.  */
  textdomain (PACKAGE_TARNAME);

  /* Parse and process arguments.  */
  argppp argp (global_opts ());
  int remaining;
  argp.parse (argc, argv, 0, &remaining);

  if (remaining == argc)
    {
      fputs (gettext ("Missing file name.\n"), stderr);
      argp.help (stderr, ARGP_HELP_SEE | ARGP_HELP_EXIT_ERR,
		 program_invocation_short_name);
      std::exit (1);
    }

  bool only_one = remaining + 1 == argc;
  do
    {
      try
	{
	  char const *fname = argv[remaining];
	  if (!only_one)
	    std::cout << std::endl << fname << ":" << std::endl;

	  int fd = files::open (fname);
	  Dwfl *dwfl = files::open_dwfl ();
	  Dwarf *c_dw = files::open_dwarf (dwfl, fname, fd);
	  dwarf dw = files::open_dwarf (c_dw);

	  process (c_dw, dw);

	  close (fd);
	  dwfl_end (dwfl);
	}
      catch (std::runtime_error &e)
	{
	  std::cerr << "error: "
		    << e.what () << '.' << std::endl;
	  continue;
	}
    }
  while (++remaining < argc);
}
