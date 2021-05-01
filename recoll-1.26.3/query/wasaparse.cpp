// A Bison parser, made by GNU Bison 3.0.4.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.


// First part of user declarations.
#line 1 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:404

#define YYDEBUG 1
#include "autoconfig.h"

#include <stdio.h>

#include <iostream>
#include <string>

#include "searchdata.h"
#include "wasaparserdriver.h"
#include "wasaparse.hpp"

using namespace std;

//#define LOG_PARSER
#ifdef LOG_PARSER
#define LOGP(X) {cerr << X;}
#else
#define LOGP(X)
#endif

int yylex(yy::parser::semantic_type *, yy::parser::location_type *, 
          WasaParserDriver *);
void yyerror(char const *);
static void qualify(Rcl::SearchDataClauseDist *, const string &);

static void addSubQuery(WasaParserDriver *d,
                        Rcl::SearchData *sd, Rcl::SearchData *sq)
{
    if (sd && sq)
        sd->addClause(
            new Rcl::SearchDataClauseSub(std::shared_ptr<Rcl::SearchData>(sq)));
}


#line 73 "y.tab.c" // lalr1.cc:404

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif



// User implementation prologue.

#line 87 "y.tab.c" // lalr1.cc:412


#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> // FIXME: INFRINGES ON USER NAME SPACE.
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K].location)
/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

# ifndef YYLLOC_DEFAULT
#  define YYLLOC_DEFAULT(Current, Rhs, N)                               \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).begin  = YYRHSLOC (Rhs, 1).begin;                   \
          (Current).end    = YYRHSLOC (Rhs, N).end;                     \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;      \
        }                                                               \
    while (/*CONSTCOND*/ false)
# endif


// Suppress unused-variable warnings by "using" E.
#define YYUSE(E) ((void) (E))

// Enable debugging if requested.
#if YYDEBUG

// A pseudo ostream that takes yydebug_ into account.
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Symbol)         \
  do {                                          \
    if (yydebug_)                               \
    {                                           \
      *yycdebug_ << Title << ' ';               \
      yy_print_ (*yycdebug_, Symbol);           \
      *yycdebug_ << std::endl;                  \
    }                                           \
  } while (false)

# define YY_REDUCE_PRINT(Rule)          \
  do {                                  \
    if (yydebug_)                       \
      yy_reduce_print_ (Rule);          \
  } while (false)

# define YY_STACK_PRINT()               \
  do {                                  \
    if (yydebug_)                       \
      yystack_print_ ();                \
  } while (false)

#else // !YYDEBUG

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Symbol)  YYUSE(Symbol)
# define YY_REDUCE_PRINT(Rule)           static_cast<void>(0)
# define YY_STACK_PRINT()                static_cast<void>(0)

#endif // !YYDEBUG

#define yyerrok         (yyerrstatus_ = 0)
#define yyclearin       (yyla.clear ())

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)


namespace yy {
#line 173 "y.tab.c" // lalr1.cc:479

  /* Return YYSTR after stripping away unnecessary quotes and
     backslashes, so that it's suitable for yyerror.  The heuristic is
     that double-quoting is unnecessary unless the string contains an
     apostrophe, a comma, or backslash (other than backslash-backslash).
     YYSTR is taken from yytname.  */
  std::string
  parser::yytnamerr_ (const char *yystr)
  {
    if (*yystr == '"')
      {
        std::string yyr = "";
        char const *yyp = yystr;

        for (;;)
          switch (*++yyp)
            {
            case '\'':
            case ',':
              goto do_not_strip_quotes;

            case '\\':
              if (*++yyp != '\\')
                goto do_not_strip_quotes;
              // Fall through.
            default:
              yyr += *yyp;
              break;

            case '"':
              return yyr;
            }
      do_not_strip_quotes: ;
      }

    return yystr;
  }


  /// Build a parser object.
  parser::parser (WasaParserDriver* d_yyarg)
    :
#if YYDEBUG
      yydebug_ (false),
      yycdebug_ (&std::cerr),
#endif
      d (d_yyarg)
  {}

  parser::~parser ()
  {}


  /*---------------.
  | Symbol types.  |
  `---------------*/

  inline
  parser::syntax_error::syntax_error (const location_type& l, const std::string& m)
    : std::runtime_error (m)
    , location (l)
  {}

  // basic_symbol.
  template <typename Base>
  inline
  parser::basic_symbol<Base>::basic_symbol ()
    : value ()
  {}

  template <typename Base>
  inline
  parser::basic_symbol<Base>::basic_symbol (const basic_symbol& other)
    : Base (other)
    , value ()
    , location (other.location)
  {
    value = other.value;
  }


  template <typename Base>
  inline
  parser::basic_symbol<Base>::basic_symbol (typename Base::kind_type t, const semantic_type& v, const location_type& l)
    : Base (t)
    , value (v)
    , location (l)
  {}


  /// Constructor for valueless symbols.
  template <typename Base>
  inline
  parser::basic_symbol<Base>::basic_symbol (typename Base::kind_type t, const location_type& l)
    : Base (t)
    , value ()
    , location (l)
  {}

  template <typename Base>
  inline
  parser::basic_symbol<Base>::~basic_symbol ()
  {
    clear ();
  }

  template <typename Base>
  inline
  void
  parser::basic_symbol<Base>::clear ()
  {
    Base::clear ();
  }

  template <typename Base>
  inline
  bool
  parser::basic_symbol<Base>::empty () const
  {
    return Base::type_get () == empty_symbol;
  }

  template <typename Base>
  inline
  void
  parser::basic_symbol<Base>::move (basic_symbol& s)
  {
    super_type::move(s);
    value = s.value;
    location = s.location;
  }

  // by_type.
  inline
  parser::by_type::by_type ()
    : type (empty_symbol)
  {}

  inline
  parser::by_type::by_type (const by_type& other)
    : type (other.type)
  {}

  inline
  parser::by_type::by_type (token_type t)
    : type (yytranslate_ (t))
  {}

  inline
  void
  parser::by_type::clear ()
  {
    type = empty_symbol;
  }

  inline
  void
  parser::by_type::move (by_type& that)
  {
    type = that.type;
    that.clear ();
  }

  inline
  int
  parser::by_type::type_get () const
  {
    return type;
  }


  // by_state.
  inline
  parser::by_state::by_state ()
    : state (empty_state)
  {}

  inline
  parser::by_state::by_state (const by_state& other)
    : state (other.state)
  {}

  inline
  void
  parser::by_state::clear ()
  {
    state = empty_state;
  }

  inline
  void
  parser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  inline
  parser::by_state::by_state (state_type s)
    : state (s)
  {}

  inline
  parser::symbol_number_type
  parser::by_state::type_get () const
  {
    if (state == empty_state)
      return empty_symbol;
    else
      return yystos_[state];
  }

  inline
  parser::stack_symbol_type::stack_symbol_type ()
  {}


  inline
  parser::stack_symbol_type::stack_symbol_type (state_type s, symbol_type& that)
    : super_type (s, that.location)
  {
    value = that.value;
    // that is emptied.
    that.type = empty_symbol;
  }

  inline
  parser::stack_symbol_type&
  parser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
    value = that.value;
    location = that.location;
    return *this;
  }


  template <typename Base>
  inline
  void
  parser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);

    // User destructor.
    switch (yysym.type_get ())
    {
            case 3: // WORD

#line 52 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:614
        {delete (yysym.value.str);}
#line 426 "y.tab.c" // lalr1.cc:614
        break;

      case 4: // QUOTED

#line 52 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:614
        {delete (yysym.value.str);}
#line 433 "y.tab.c" // lalr1.cc:614
        break;

      case 5: // QUALIFIERS

#line 52 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:614
        {delete (yysym.value.str);}
#line 440 "y.tab.c" // lalr1.cc:614
        break;

      case 23: // complexfieldname

#line 52 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:614
        {delete (yysym.value.str);}
#line 447 "y.tab.c" // lalr1.cc:614
        break;


      default:
        break;
    }
  }

#if YYDEBUG
  template <typename Base>
  void
  parser::yy_print_ (std::ostream& yyo,
                                     const basic_symbol<Base>& yysym) const
  {
    std::ostream& yyoutput = yyo;
    YYUSE (yyoutput);
    symbol_number_type yytype = yysym.type_get ();
    // Avoid a (spurious) G++ 4.8 warning about "array subscript is
    // below array bounds".
    if (yysym.empty ())
      std::abort ();
    yyo << (yytype < yyntokens_ ? "token" : "nterm")
        << ' ' << yytname_[yytype] << " ("
        << yysym.location << ": ";
    YYUSE (yytype);
    yyo << ')';
  }
#endif

  inline
  void
  parser::yypush_ (const char* m, state_type s, symbol_type& sym)
  {
    stack_symbol_type t (s, sym);
    yypush_ (m, t);
  }

  inline
  void
  parser::yypush_ (const char* m, stack_symbol_type& s)
  {
    if (m)
      YY_SYMBOL_PRINT (m, s);
    yystack_.push (s);
  }

  inline
  void
  parser::yypop_ (unsigned int n)
  {
    yystack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  parser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  parser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  parser::debug_level_type
  parser::debug_level () const
  {
    return yydebug_;
  }

  void
  parser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YYDEBUG

  inline parser::state_type
  parser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - yyntokens_] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - yyntokens_];
  }

  inline bool
  parser::yy_pact_value_is_default_ (int yyvalue)
  {
    return yyvalue == yypact_ninf_;
  }

  inline bool
  parser::yy_table_value_is_error_ (int yyvalue)
  {
    return yyvalue == yytable_ninf_;
  }

  int
  parser::parse ()
  {
    // State.
    int yyn;
    /// Length of the RHS of the rule being reduced.
    int yylen = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// The lookahead symbol.
    symbol_type yyla;

    /// The locations where the error started and ended.
    stack_symbol_type yyerror_range[3];

    /// The return value of parse ().
    int yyresult;

    // FIXME: This shoud be completely indented.  It is not yet to
    // avoid gratuitous conflicts when merging into the master branch.
    try
      {
    YYCDEBUG << "Starting parse" << std::endl;


    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear ();
    yypush_ (YY_NULLPTR, 0, yyla);

    // A new symbol was pushed on the stack.
  yynewstate:
    YYCDEBUG << "Entering state " << yystack_[0].state << std::endl;

    // Accept?
    if (yystack_[0].state == yyfinal_)
      goto yyacceptlab;

    goto yybackup;

    // Backup.
  yybackup:

    // Try to take a decision without lookahead.
    yyn = yypact_[yystack_[0].state];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    // Read a lookahead token.
    if (yyla.empty ())
      {
        YYCDEBUG << "Reading a token: ";
        try
          {
            yyla.type = yytranslate_ (yylex (&yyla.value, &yyla.location, d));
          }
        catch (const syntax_error& yyexc)
          {
            error (yyexc);
            goto yyerrlab1;
          }
      }
    YY_SYMBOL_PRINT ("Next token is", yyla);

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.type_get ();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.type_get ())
      goto yydefault;

    // Reduce or error.
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
        if (yy_table_value_is_error_ (yyn))
          goto yyerrlab;
        yyn = -yyn;
        goto yyreduce;
      }

    // Count tokens shifted since error; after three, turn off error status.
    if (yyerrstatus_)
      --yyerrstatus_;

    // Shift the lookahead token.
    yypush_ ("Shifting", yyn, yyla);
    goto yynewstate;

  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[yystack_[0].state];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;

  /*-----------------------------.
  | yyreduce -- Do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_(yystack_[yylen].state, yyr1_[yyn]);
      /* If YYLEN is nonzero, implement the default value of the
         action: '$$ = $1'.  Otherwise, use the top of the stack.

         Otherwise, the following line sets YYLHS.VALUE to garbage.
         This behavior is undocumented and Bison users should not rely
         upon it.  */
      if (yylen)
        yylhs.value = yystack_[yylen - 1].value;
      else
        yylhs.value = yystack_[0].value;

      // Compute the default @$.
      {
        slice<stack_symbol_type, stack_type> slice (yystack_, yylen);
        YYLLOC_DEFAULT (yylhs.location, slice, yylen);
      }

      // Perform the reduction.
      YY_REDUCE_PRINT (yyn);
      try
        {
          switch (yyn)
            {
  case 2:
#line 74 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    // It's possible that we end up with no query (e.g.: because just a
    // date filter was set, no terms). Allocate an empty query so that we
    // have something to set the global criteria on (this will yield a
    // Xapian search like <alldocuments> FILTER xxx
    if ((yystack_[0].value.sd) == 0)
        d->m_result = new Rcl::SearchData(Rcl::SCLT_AND, d->m_stemlang);
    else
        d->m_result = (yystack_[0].value.sd);
}
#line 695 "y.tab.c" // lalr1.cc:859
    break;

  case 3:
#line 87 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("q: query query\n");
    Rcl::SearchData *sd = 0;
    if ((yystack_[1].value.sd) || (yystack_[0].value.sd)) {
        sd = new Rcl::SearchData(Rcl::SCLT_AND, d->m_stemlang);
        addSubQuery(d, sd, (yystack_[1].value.sd));
        addSubQuery(d, sd, (yystack_[0].value.sd));
    }
    (yylhs.value.sd) = sd;
}
#line 710 "y.tab.c" // lalr1.cc:859
    break;

  case 4:
#line 98 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("q: query AND query\n");
    Rcl::SearchData *sd = 0;
    if ((yystack_[2].value.sd) || (yystack_[0].value.sd)) {
        sd = new Rcl::SearchData(Rcl::SCLT_AND, d->m_stemlang);
        addSubQuery(d, sd, (yystack_[2].value.sd));
        addSubQuery(d, sd, (yystack_[0].value.sd));
    }
    (yylhs.value.sd) = sd;
}
#line 725 "y.tab.c" // lalr1.cc:859
    break;

  case 5:
#line 109 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("query: query OR query\n");
    Rcl::SearchData *top = 0;
    if ((yystack_[2].value.sd) || (yystack_[0].value.sd)) {
       top = new Rcl::SearchData(Rcl::SCLT_OR, d->m_stemlang);
       addSubQuery(d, top, (yystack_[2].value.sd));
       addSubQuery(d, top, (yystack_[0].value.sd));
    }
    (yylhs.value.sd) = top;
}
#line 740 "y.tab.c" // lalr1.cc:859
    break;

  case 6:
#line 120 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("q: ( query )\n");
    (yylhs.value.sd) = (yystack_[1].value.sd);
}
#line 749 "y.tab.c" // lalr1.cc:859
    break;

  case 7:
#line 126 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("q: fieldexpr\n");
    Rcl::SearchData *sd = new Rcl::SearchData(Rcl::SCLT_AND, d->m_stemlang);
    if (d->addClause(sd, (yystack_[0].value.cl))) {
        (yylhs.value.sd) = sd;
    } else {
        delete sd;
        (yylhs.value.sd) = 0;
    }
}
#line 764 "y.tab.c" // lalr1.cc:859
    break;

  case 8:
#line 139 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("fe: simple fieldexpr: " << (yystack_[0].value.cl)->gettext() << endl);
    (yylhs.value.cl) = (yystack_[0].value.cl);
}
#line 773 "y.tab.c" // lalr1.cc:859
    break;

  case 9:
#line 144 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("fe: " << *(yystack_[2].value.str) << " = " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_EQUALS);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 785 "y.tab.c" // lalr1.cc:859
    break;

  case 10:
#line 152 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("fe: " << *(yystack_[2].value.str) << " : " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_CONTAINS);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 797 "y.tab.c" // lalr1.cc:859
    break;

  case 11:
#line 160 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("fe: " << *(yystack_[2].value.str) << " : " << (yystack_[0].value.rg)->gettext() << endl);
    (yystack_[0].value.rg)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.rg)->setrel(Rcl::SearchDataClause::REL_CONTAINS);
    (yylhs.value.cl) = (yystack_[0].value.rg);
    delete (yystack_[2].value.str);
}
#line 809 "y.tab.c" // lalr1.cc:859
    break;

  case 12:
#line 168 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("fe: " << *(yystack_[2].value.str) << " < " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_LT);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 821 "y.tab.c" // lalr1.cc:859
    break;

  case 13:
#line 176 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("fe: " << *(yystack_[2].value.str) << " <= " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_LTE);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 833 "y.tab.c" // lalr1.cc:859
    break;

  case 14:
#line 184 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("fe: "  << *(yystack_[2].value.str) << " > " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_GT);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 845 "y.tab.c" // lalr1.cc:859
    break;

  case 15:
#line 192 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("fe: " << *(yystack_[2].value.str) << " >= " << (yystack_[0].value.cl)->gettext() << endl);
    (yystack_[0].value.cl)->setfield(*(yystack_[2].value.str));
    (yystack_[0].value.cl)->setrel(Rcl::SearchDataClause::REL_GTE);
    (yylhs.value.cl) = (yystack_[0].value.cl);
    delete (yystack_[2].value.str);
}
#line 857 "y.tab.c" // lalr1.cc:859
    break;

  case 16:
#line 200 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("fe: - fieldexpr[" << (yystack_[0].value.cl)->gettext() << "]" << endl);
    (yystack_[0].value.cl)->setexclude(true);
    (yylhs.value.cl) = (yystack_[0].value.cl);
}
#line 867 "y.tab.c" // lalr1.cc:859
    break;

  case 17:
#line 210 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("cfn: WORD" << endl);
    (yylhs.value.str) = (yystack_[0].value.str);
}
#line 876 "y.tab.c" // lalr1.cc:859
    break;

  case 18:
#line 216 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("cfn: complexfieldname ':' WORD" << endl);
    (yylhs.value.str) = new string(*(yystack_[2].value.str) + string(":") + *(yystack_[0].value.str));
    delete (yystack_[2].value.str);
    delete (yystack_[0].value.str);
}
#line 887 "y.tab.c" // lalr1.cc:859
    break;

  case 19:
#line 225 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("Range: " << *(yystack_[2].value.str) << string(" .. ") << *(yystack_[0].value.str) << endl);
    (yylhs.value.rg) = new Rcl::SearchDataClauseRange(*(yystack_[2].value.str), *(yystack_[0].value.str));
    delete (yystack_[2].value.str);
    delete (yystack_[0].value.str);
}
#line 898 "y.tab.c" // lalr1.cc:859
    break;

  case 20:
#line 233 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("Range: " << "" << string(" .. ") << *(yystack_[0].value.str) << endl);
    (yylhs.value.rg) = new Rcl::SearchDataClauseRange("", *(yystack_[0].value.str));
    delete (yystack_[0].value.str);
}
#line 908 "y.tab.c" // lalr1.cc:859
    break;

  case 21:
#line 240 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("Range: " << *(yystack_[1].value.str) << string(" .. ") << "" << endl);
    (yylhs.value.rg) = new Rcl::SearchDataClauseRange(*(yystack_[1].value.str), "");
    delete (yystack_[1].value.str);
}
#line 918 "y.tab.c" // lalr1.cc:859
    break;

  case 22:
#line 249 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("term[" << *(yystack_[0].value.str) << "]" << endl);
    (yylhs.value.cl) = new Rcl::SearchDataClauseSimple(Rcl::SCLT_AND, *(yystack_[0].value.str));
    delete (yystack_[0].value.str);
}
#line 928 "y.tab.c" // lalr1.cc:859
    break;

  case 23:
#line 255 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    (yylhs.value.cl) = (yystack_[0].value.cl);
}
#line 936 "y.tab.c" // lalr1.cc:859
    break;

  case 24:
#line 261 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("QUOTED[" << *(yystack_[0].value.str) << "]" << endl);
    (yylhs.value.cl) = new Rcl::SearchDataClauseDist(Rcl::SCLT_PHRASE, *(yystack_[0].value.str), 0);
    delete (yystack_[0].value.str);
}
#line 946 "y.tab.c" // lalr1.cc:859
    break;

  case 25:
#line 267 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:859
    {
    LOGP("QUOTED[" << *(yystack_[1].value.str) << "] QUALIFIERS[" << *(yystack_[0].value.str) << "]" << endl);
    Rcl::SearchDataClauseDist *cl = 
        new Rcl::SearchDataClauseDist(Rcl::SCLT_PHRASE, *(yystack_[1].value.str), 0);
    qualify(cl, *(yystack_[0].value.str));
    (yylhs.value.cl) = cl;
    delete (yystack_[1].value.str);
    delete (yystack_[0].value.str);
}
#line 960 "y.tab.c" // lalr1.cc:859
    break;


#line 964 "y.tab.c" // lalr1.cc:859
            default:
              break;
            }
        }
      catch (const syntax_error& yyexc)
        {
          error (yyexc);
          YYERROR;
        }
      YY_SYMBOL_PRINT ("-> $$ =", yylhs);
      yypop_ (yylen);
      yylen = 0;
      YY_STACK_PRINT ();

      // Shift the result of the reduction.
      yypush_ (YY_NULLPTR, yylhs);
    }
    goto yynewstate;

  /*--------------------------------------.
  | yyerrlab -- here on detecting error.  |
  `--------------------------------------*/
  yyerrlab:
    // If not already recovering from an error, report this error.
    if (!yyerrstatus_)
      {
        ++yynerrs_;
        error (yyla.location, yysyntax_error_ (yystack_[0].state, yyla));
      }


    yyerror_range[1].location = yyla.location;
    if (yyerrstatus_ == 3)
      {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        // Return failure if at end of input.
        if (yyla.type_get () == yyeof_)
          YYABORT;
        else if (!yyla.empty ())
          {
            yy_destroy_ ("Error: discarding", yyla);
            yyla.clear ();
          }
      }

    // Else will try to reuse lookahead token after shifting the error token.
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:

    /* Pacify compilers like GCC when the user code never invokes
       YYERROR and the label yyerrorlab therefore never appears in user
       code.  */
    if (false)
      goto yyerrorlab;
    yyerror_range[1].location = yystack_[yylen - 1].location;
    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    goto yyerrlab1;

  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;   // Each real token shifted decrements this.
    {
      stack_symbol_type error_token;
      for (;;)
        {
          yyn = yypact_[yystack_[0].state];
          if (!yy_pact_value_is_default_ (yyn))
            {
              yyn += yyterror_;
              if (0 <= yyn && yyn <= yylast_ && yycheck_[yyn] == yyterror_)
                {
                  yyn = yytable_[yyn];
                  if (0 < yyn)
                    break;
                }
            }

          // Pop the current state because it cannot handle the error token.
          if (yystack_.size () == 1)
            YYABORT;

          yyerror_range[1].location = yystack_[0].location;
          yy_destroy_ ("Error: popping", yystack_[0]);
          yypop_ ();
          YY_STACK_PRINT ();
        }

      yyerror_range[2].location = yyla.location;
      YYLLOC_DEFAULT (error_token.location, yyerror_range, 2);

      // Shift the error token.
      error_token.state = yyn;
      yypush_ ("Shifting", error_token);
    }
    goto yynewstate;

    // Accept.
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;

    // Abort.
  yyabortlab:
    yyresult = 1;
    goto yyreturn;

  yyreturn:
    if (!yyla.empty ())
      yy_destroy_ ("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    while (1 < yystack_.size ())
      {
        yy_destroy_ ("Cleanup: popping", yystack_[0]);
        yypop_ ();
      }

    return yyresult;
  }
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack"
                 << std::endl;
        // Do not try to display the values of the reclaimed symbols,
        // as their printer might throw an exception.
        if (!yyla.empty ())
          yy_destroy_ (YY_NULLPTR, yyla);

        while (1 < yystack_.size ())
          {
            yy_destroy_ (YY_NULLPTR, yystack_[0]);
            yypop_ ();
          }
        throw;
      }
  }

  void
  parser::error (const syntax_error& yyexc)
  {
    error (yyexc.location, yyexc.what());
  }

  // Generate an error message.
  std::string
  parser::yysyntax_error_ (state_type yystate, const symbol_type& yyla) const
  {
    // Number of reported tokens (one for the "unexpected", one per
    // "expected").
    size_t yycount = 0;
    // Its maximum.
    enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
    // Arguments of yyformat.
    char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];

    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yyla) is
         if this state is a consistent state with a default action.
         Thus, detecting the absence of a lookahead is sufficient to
         determine that there is no unexpected or expected token to
         report.  In that case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is
         a consistent state with a default action.  There might have
         been a previous inconsistent state, consistent state with a
         non-default action, or user semantic action that manipulated
         yyla.  (However, yyla is currently not documented for users.)
       - Of course, the expected token list depends on states to have
         correct lookahead information, and it depends on the parser not
         to perform extra reductions after fetching a lookahead from the
         scanner and before detecting a syntax error.  Thus, state
         merging (from LALR or IELR) and default reductions corrupt the
         expected token list.  However, the list is correct for
         canonical LR with one exception: it will still contain any
         token that will not be accepted due to an error action in a
         later state.
    */
    if (!yyla.empty ())
      {
        int yytoken = yyla.type_get ();
        yyarg[yycount++] = yytname_[yytoken];
        int yyn = yypact_[yystate];
        if (!yy_pact_value_is_default_ (yyn))
          {
            /* Start YYX at -YYN if negative to avoid negative indexes in
               YYCHECK.  In other words, skip the first -YYN actions for
               this state because they are default actions.  */
            int yyxbegin = yyn < 0 ? -yyn : 0;
            // Stay within bounds of both yycheck and yytname.
            int yychecklim = yylast_ - yyn + 1;
            int yyxend = yychecklim < yyntokens_ ? yychecklim : yyntokens_;
            for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
              if (yycheck_[yyx + yyn] == yyx && yyx != yyterror_
                  && !yy_table_value_is_error_ (yytable_[yyx + yyn]))
                {
                  if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                    {
                      yycount = 1;
                      break;
                    }
                  else
                    yyarg[yycount++] = yytname_[yyx];
                }
          }
      }

    char const* yyformat = YY_NULLPTR;
    switch (yycount)
      {
#define YYCASE_(N, S)                         \
        case N:                               \
          yyformat = S;                       \
        break
        YYCASE_(0, YY_("syntax error"));
        YYCASE_(1, YY_("syntax error, unexpected %s"));
        YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
        YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
        YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
        YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
      }

    std::string yyres;
    // Argument number.
    size_t yyi = 0;
    for (char const* yyp = yyformat; *yyp; ++yyp)
      if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount)
        {
          yyres += yytnamerr_ (yyarg[yyi++]);
          ++yyp;
        }
      else
        yyres += *yyp;
    return yyres;
  }


  const signed char parser::yypact_ninf_ = -3;

  const signed char parser::yytable_ninf_ = -19;

  const signed char
  parser::yypact_[] =
  {
      31,    32,     3,    31,    33,     6,    14,    -3,    38,    -3,
      -3,    -3,     1,    -3,    -3,    31,    31,     4,    -2,     9,
      -2,    -2,    -2,    -2,    -3,     4,    -3,    -3,    -3,    16,
      18,    -3,    -3,    -3,    -3,    -3,    -3,    22,    -3,    -3
  };

  const unsigned char
  parser::yydefact_[] =
  {
       0,    22,    24,     0,     0,     0,     2,     7,     0,     8,
      23,    25,     0,    16,     1,     0,     0,     3,     0,     0,
       0,     0,     0,     0,     6,     4,     5,    22,     9,    22,
       0,    11,    10,    13,    12,    15,    14,    21,    20,    19
  };

  const signed char
  parser::yypgoto_[] =
  {
      -3,    -3,     0,    34,    -3,    -3,    37,    -3
  };

  const signed char
  parser::yydefgoto_[] =
  {
      -1,     5,    17,     7,     8,    31,     9,    10
  };

  const signed char
  parser::yytable_[] =
  {
       6,    27,     2,    12,     1,     2,    14,    15,    11,     3,
       4,    16,    29,     2,    16,    25,    26,     1,     2,    24,
      15,    38,     3,     4,    16,    39,    30,   -18,   -18,   -18,
     -18,   -18,   -18,    37,     1,     2,     1,     2,    13,     3,
       4,     0,     4,   -17,   -17,   -17,   -17,   -17,   -17,    18,
      19,    20,    21,    22,    23,    28,    32,    33,    34,    35,
      36
  };

  const signed char
  parser::yycheck_[] =
  {
       0,     3,     4,     3,     3,     4,     0,     6,     5,     8,
       9,    10,     3,     4,    10,    15,    16,     3,     4,    18,
       6,     3,     8,     9,    10,     3,    17,    11,    12,    13,
      14,    15,    16,    17,     3,     4,     3,     4,     4,     8,
       9,    -1,     9,    11,    12,    13,    14,    15,    16,    11,
      12,    13,    14,    15,    16,    18,    19,    20,    21,    22,
      23
  };

  const unsigned char
  parser::yystos_[] =
  {
       0,     3,     4,     8,     9,    20,    21,    22,    23,    25,
      26,     5,    21,    22,     0,     6,    10,    21,    11,    12,
      13,    14,    15,    16,    18,    21,    21,     3,    25,     3,
      17,    24,    25,    25,    25,    25,    25,    17,     3,     3
  };

  const unsigned char
  parser::yyr1_[] =
  {
       0,    19,    20,    21,    21,    21,    21,    21,    22,    22,
      22,    22,    22,    22,    22,    22,    22,    23,    23,    24,
      24,    24,    25,    25,    26,    26
  };

  const unsigned char
  parser::yyr2_[] =
  {
       0,     2,     1,     2,     3,     3,     3,     1,     1,     3,
       3,     3,     3,     3,     3,     3,     2,     1,     3,     3,
       2,     2,     1,     1,     1,     2
  };



  // YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
  // First, the terminals, then, starting at \a yyntokens_, nonterminals.
  const char*
  const parser::yytname_[] =
  {
  "$end", "error", "$undefined", "WORD", "QUOTED", "QUALIFIERS", "AND",
  "UCONCAT", "'('", "'-'", "OR", "EQUALS", "CONTAINS", "SMALLEREQ",
  "SMALLER", "GREATEREQ", "GREATER", "RANGE", "')'", "$accept", "topquery",
  "query", "fieldexpr", "complexfieldname", "range", "term", "qualquote", YY_NULLPTR
  };

#if YYDEBUG
  const unsigned short int
  parser::yyrline_[] =
  {
       0,    73,    73,    86,    97,   108,   119,   125,   138,   143,
     151,   159,   167,   175,   183,   191,   199,   209,   215,   224,
     232,   239,   248,   254,   260,   266
  };

  // Print the state stack on the debug stream.
  void
  parser::yystack_print_ ()
  {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator
           i = yystack_.begin (),
           i_end = yystack_.end ();
         i != i_end; ++i)
      *yycdebug_ << ' ' << i->state;
    *yycdebug_ << std::endl;
  }

  // Report on the debug stream that the rule \a yyrule is going to be reduced.
  void
  parser::yy_reduce_print_ (int yyrule)
  {
    unsigned int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
               << " (line " << yylno << "):" << std::endl;
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
                       yystack_[(yynrhs) - (yyi + 1)]);
  }
#endif // YYDEBUG

  // Symbol number corresponding to token number t.
  inline
  parser::token_number_type
  parser::yytranslate_ (int t)
  {
    static
    const token_number_type
    translate_table[] =
    {
     0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       8,    18,     2,     2,     2,     9,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,    10,    11,    12,    13,    14,    15,    16,
      17
    };
    const unsigned int user_token_number_max_ = 270;
    const token_number_type undef_token_ = 2;

    if (static_cast<int>(t) <= yyeof_)
      return yyeof_;
    else if (static_cast<unsigned int> (t) <= user_token_number_max_)
      return translate_table[t];
    else
      return undef_token_;
  }


} // yy
#line 1404 "y.tab.c" // lalr1.cc:1167
#line 278 "/home/dockes/projets/fulltext/recoll/src/query/wasaparse.ypp" // lalr1.cc:1168


#include <ctype.h>

// Look for int at index, skip and return new index found? value.
static unsigned int qualGetInt(const string& q, unsigned int cur, int *pval)
{
    unsigned int ncur = cur;
    if (cur < q.size() - 1) {
        char *endptr;
        int val = strtol(&q[cur + 1], &endptr, 10);
        if (endptr != &q[cur + 1]) {
            ncur += endptr - &q[cur + 1];
            *pval = val;
        }
    }
    return ncur;
}

static void qualify(Rcl::SearchDataClauseDist *cl, const string& quals)
{
    // cerr << "qualify(" << cl << ", " << quals << ")" << endl;
    for (unsigned int i = 0; i < quals.length(); i++) {
        //fprintf(stderr, "qual char %c\n", quals[i]);
        switch (quals[i]) {
        case 'b': 
            cl->setWeight(10.0);
            break;
        case 'c': break;
        case 'C': 
            cl->addModifier(Rcl::SearchDataClause::SDCM_CASESENS);
            break;
        case 'd': break;
        case 'D':  
            cl->addModifier(Rcl::SearchDataClause::SDCM_DIACSENS);
            break;
        case 'e': 
            cl->addModifier(Rcl::SearchDataClause::SDCM_CASESENS);
            cl->addModifier(Rcl::SearchDataClause::SDCM_DIACSENS);
            cl->addModifier(Rcl::SearchDataClause::SDCM_NOSTEMMING);
            break;
        case 'l': 
            cl->addModifier(Rcl::SearchDataClause::SDCM_NOSTEMMING);
            break;
        case 'L': break;
        case 'o':  
        {
            int slack = 10;
            i = qualGetInt(quals, i, &slack);
            cl->setslack(slack);
            //cerr << "set slack " << cl->getslack() << " done" << endl;
        }
        break;
        case 'p': 
            cl->setTp(Rcl::SCLT_NEAR);
            if (cl->getslack() == 0) {
                cl->setslack(10);
                //cerr << "set slack " << cl->getslack() << " done" << endl;
            }
            break;
        case 's': 
            cl->addModifier(Rcl::SearchDataClause::SDCM_NOSYNS);
            break;
	case 'S':
            break;
        case '.':case '0':case '1':case '2':case '3':case '4':
        case '5':case '6':case '7':case '8':case '9':
        {
            int n = 0;
            float factor = 1.0;
            if (sscanf(&(quals[i]), "%f %n", &factor, &n)) {
                if (factor != 1.0) {
                    cl->setWeight(factor);
                }
            }
            if (n > 0)
                i += n - 1;
        }
        default:
            break;
        }
    }
}


// specialstartchars are special only at the beginning of a token
// (e.g. doctor-who is a term, not 2 terms separated by '-')
static const string specialstartchars("-");
// specialinchars are special everywhere except inside a quoted string
static const string specialinchars(":=<>()");

// Called with the first dquote already read
static int parseString(WasaParserDriver *d, yy::parser::semantic_type *yylval)
{
    string* value = new string();
    d->qualifiers().clear();
    int c;
    while ((c = d->GETCHAR())) {
        switch (c) {
        case '\\':
            /* Escape: get next char */
            c = d->GETCHAR();
            if (c == 0) {
                value->push_back(c);
                goto out;
            }
            value->push_back(c);
            break;
        case '"':
            /* End of string. Look for qualifiers */
            while ((c = d->GETCHAR()) && (isalnum(c) || c == '.'))
                d->qualifiers().push_back(c);
            d->UNGETCHAR(c);
            goto out;
        default:
            value->push_back(c);
        }
    }
out:
    //cerr << "GOT QUOTED ["<<value<<"] quals [" << d->qualifiers() << "]" << endl;
    yylval->str = value;
    return yy::parser::token::QUOTED;
}


int yylex(yy::parser::semantic_type *yylval, yy::parser::location_type *, 
		  WasaParserDriver *d)
{
    if (!d->qualifiers().empty()) {
        yylval->str = new string();
        yylval->str->swap(d->qualifiers());
        return yy::parser::token::QUALIFIERS;
    }

    int c;

    /* Skip white space.  */
    while ((c = d->GETCHAR()) && isspace(c))
        continue;

    if (c == 0)
        return 0;

    if (specialstartchars.find_first_of(c) != string::npos) {
        //cerr << "yylex: return " << c << endl;
        return c;
    }

    // field-term relations, and ranges
    switch (c) {
    case '=': return yy::parser::token::EQUALS;
    case ':': return yy::parser::token::CONTAINS;
    case '<': {
        int c1 = d->GETCHAR();
        if (c1 == '=') {
            return yy::parser::token::SMALLEREQ;
        } else {
            d->UNGETCHAR(c1);
            return yy::parser::token::SMALLER;
        }
    }
    case '.': {
        int c1 = d->GETCHAR();
        if (c1 == '.') {
            return yy::parser::token::RANGE;
        } else {
            d->UNGETCHAR(c1);
            break;
        }
    }
    case '>': {
        int c1 = d->GETCHAR();
        if (c1 == '=') {
            return yy::parser::token::GREATEREQ;
        } else {
            d->UNGETCHAR(c1);
            return yy::parser::token::GREATER;
        }
    }
    case '(': case ')':
        return c;
    }
        
    if (c == '"')
        return parseString(d, yylval);

    d->UNGETCHAR(c);

    // Other chars start a term or field name or reserved word
    string* word = new string();
    while ((c = d->GETCHAR())) {
        if (isspace(c)) {
            //cerr << "Word broken by whitespace" << endl;
            break;
        } else if (specialinchars.find_first_of(c) != string::npos) {
            //cerr << "Word broken by special char" << endl;
            d->UNGETCHAR(c);
            break;
        } else if (c == '.') {
            int c1 = d->GETCHAR();
            if (c1 == '.') {
                d->UNGETCHAR(c1);
                d->UNGETCHAR(c);
                break;
            } else {
                d->UNGETCHAR(c1);
                word->push_back(c);
            }
        } else if (c == 0) {
            //cerr << "Word broken by EOF" << endl;
            break;
        } else {
            word->push_back(c);
        }
    }
    
    if (!word->compare("AND") || !word->compare("&&")) {
        delete word;
        return yy::parser::token::AND;
    } else if (!word->compare("OR") || !word->compare("||")) {
        delete word;
        return yy::parser::token::OR;
    }

//    cerr << "Got word [" << word << "]" << endl;
    yylval->str = word;
    return yy::parser::token::WORD;
}
