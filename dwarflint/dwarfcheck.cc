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

// Sharp 0.0% coverage (i.e. not a single address byte is covered)
const int cov_00 = 0;

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
  bool is_dbg_line_opt = opt_check_dbg_lines.seen();
  bool is_dbg_var_opt = opt_check_dbg_variables.seen();
  int num_of_errs = 0;
  if (is_dbg_line_opt)
    std::cout << "****CHECKING DEBUG LINES****\n";
  else if (is_dbg_var_opt)
    std::cout << "****CHECKING DEBUG VARIABLES****\n";

  for (all_dies_iterator<dwarf> iter = all_dies_iterator<dwarf> (dw);
       iter != all_dies_iterator<dwarf> (); ++iter)
  {
    dwarf::debug_info_entry const &die = *iter;
    if (is_dbg_line_opt) {
      bool bFormalParameter = die.tag () == DW_TAG_formal_parameter;
      bool bLocalVar = die.tag () == DW_TAG_variable;
	  bool bSubprogram = die.tag () == DW_TAG_subprogram;
	  if (!bFormalParameter && !bLocalVar && !bSubprogram)
        continue;

	  Dwarf_Die die_c_ln,
	  *die_c = dwarf_offdie (c_dw, die.offset (), &die_c_ln);
	  if (die_c == NULL)
		std::cout << "there is no die!!" << std::endl;
	  dwarf::debug_info_entry::attributes_type const &attrs
	  = die.attributes ();

	  if (attrs.find (DW_AT_decl_line) == attrs.end ()
		  || attrs.find (DW_AT_decl_file) == attrs.end()) {
        num_of_errs++;
		const char *name = dwarf_diename(&die_c_ln);
		if (name) {
          if(strlen(name) > 1 && name[0] == '_' && name[1] == '_')
            continue;
          if (bFormalParameter || bLocalVar)
            std::cout << "Variable <" << name << "> does not have line or file set!!!" << std::endl;
          else
            std::cout << "Function <" << name << "> does not have line or file set!!!" << std::endl;
		}
	  }
  } else if (is_dbg_var_opt) {
      bool bFormalParameter = die.tag () == DW_TAG_formal_parameter;
      bool bLocalVar = die.tag () == DW_TAG_variable;
         
      if(!bFormalParameter && !bLocalVar)
	    continue;

	Dwarf_Die die_c_var,
	*die_c = dwarf_offdie (c_dw, die.offset (), &die_c_var);
	if (die_c == NULL)
		std::cout << "there is no die!!" << std::endl;
	dwarf::debug_info_entry::attributes_type const &attrs
	= die.attributes ();
        
	Dwarf_Attribute locattr_mem,
	  *locattr = dwarf_attr_integrate (die_c, DW_AT_location, &locattr_mem);
        
	if (attrs.find (DW_AT_external) != attrs.end () && locattr == NULL)
	  continue;

  std::vector<dwarf::debug_info_entry> const &die_stack = iter.stack ();
  dwarf::debug_info_entry const &parent = *(die_stack.rbegin () + 1);

  Dwarf_Die die_c_fun;
  const char *fname;
  if(dwarf_offdie (c_dw, parent.offset (), &die_c_fun) != NULL)
  {
     fname = dwarf_diename(&die_c_fun);
     
  }

	const char *name = dwarf_diename(&die_c_var);
	if (name)
    if(strlen(name) > 1 && name[0] == '_' && name[1] == '_')
      continue;
    std::cout << "Function <" << fname << ">" << " ";
	  std::cout << "variable <" << name << ">:" << std::endl;

        int coverage;
        Dwarf_Op *expr;
        size_t len;

    if (attrs.find (DW_AT_const_value) != attrs.end ())
        coverage = 100;
    else if (locattr == NULL)
	      coverage = cov_00;
    else if (dwarf_getlocation (locattr, &expr, &len) == 0)
	      coverage = (len == 0) ? cov_00 : 100;
    else
    {
      try
      {
        dwarf::ranges ranges (find_ranges (die_stack));
        size_t length = 0;
        size_t covered = 0;
        size_t nlocs = 10;
        Dwarf_Op *exprs[nlocs];
        size_t exprlens[nlocs];

        for (dwarf::ranges::const_iterator rit = ranges.begin ();
        rit != ranges.end (); ++rit)
        {
          Dwarf_Addr low = (*rit).first;
          Dwarf_Addr high = (*rit).second;
          length += high - low;

          for (Dwarf_Addr addr = low; addr < high; ++addr)
          {
            int got = dwarf_getlocation_addr (locattr, addr,
              exprs, exprlens, nlocs);
            if (got < 0)
              throw ::error (std::string ("dwarf_getlocation_addr: ")
               + dwarf_errmsg (-1));
            for (int i = 0; i < got; ++i)
            {
              if (exprlens[i] > 0)
              {
                covered++;
                break;
              }
            }
          }
        }

        if (length == 0)
          throw ::error ("zero-length range");

        if (covered == 0)
          coverage = cov_00;
        else
          coverage = 100 * covered / length;
      }
      catch (::error const &e)
      {
        std::cout << "error: " << die_locus (die) << ": "
        << e.what () << '.' << std::endl;
        continue;
      }
    }

       //Report the result.
       std::cout << "	-location coverage:" << coverage << "\%" << std::endl;
    }
  }

  if (is_dbg_line_opt && num_of_errs == 0)
    std::cout << "\n\n All declare lines and files are set properly! \n\n";
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
