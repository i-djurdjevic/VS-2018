/*
   Copyright (C) 2011 Red Hat, Inc.
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

#ifndef _DWARFLINT_DIE_LOCUS_H_
#define _DWARFLINT_DIE_LOCUS_H_

#include "locus.hh"
#include "../libdw/c++/dwarf"

namespace locus_simple_fmt
{
  char const *cu_n ();
};

typedef fixed_locus<sec_info,
		    locus_simple_fmt::cu_n,
		    locus_simple_fmt::dec> cu_locus;

class die_locus
  : public locus
{
  Dwarf_Off _m_offset;
  int _m_attrib_name;

public:
  explicit die_locus (Dwarf_Off offset = -1, int attrib_name = -1)
    : _m_offset (offset)
    , _m_attrib_name (attrib_name)
  {}

  template <class T>
  explicit die_locus (T const &die, int attrib_name = -1)
    : _m_offset (die.offset ())
    , _m_attrib_name (attrib_name)
  {}

  void
  set_attrib_name (int attrib_name)
  {
    _m_attrib_name = attrib_name;
  }

  std::string format (bool brief = false) const;
};

#endif /* _DWARFLINT_DIE_LOCUS_H_ */
