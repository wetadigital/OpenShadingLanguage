// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include "automata.h"

OSL_NAMESPACE_BEGIN


namespace lpexp {

// This is just a pair of states, see the use of the function genAuto in LPexp
// for a justification of this type that we use throughout all the regexp code
typedef std::pair<NdfAutomata::State*, NdfAutomata::State*> FirstLast;

/// LPexp atom type for the getType method
typedef enum { CAT, OR, SYMBOL, WILDCARD, REPEAT, NREPEAT } Regtype;



/// Base class for a light path expression
//
/// Light path expressions are arranged as an abstract syntax tree. All the
/// nodes in that tree satisfy this interface that basically makes the automate
/// generation easy and clear.
///
/// The node types for this tree are:
///     CAT:      Concatenation of regexps like abcde or (abcde)
///     OR:        Ored union of two or more expressions like a|b|c|d
///     SYMBOL    Just a symbol like G or 'customlabel'
///     WILDCARD The wildcard regexp for . or [^GS]
///     REPEAT    Generic unlimited repetition of the child expression (exp)*
///     NREPEAT  Bounded repetition of the child expression like (exp){n,m}
///
class LPexp {
public:
    virtual ~LPexp() {};

    /// Generate automata states for this subtree
    ///
    /// This method recursively builds all the needed automata states of
    /// the tree rooted by this node and returns the begin and end states
    /// for it. That means that if it were the only thing in the automata,
    /// making retvalue.first initial state and retvalue.second final state,
    /// would be the right thing to do.
    ///
    virtual FirstLast genAuto(NdfAutomata& automata) const = 0;
    /// Get the type for this node
    virtual Regtype getType() const = 0;
    /// For the parser's convenience. It is easy to implement things like a+
    /// as aa*. So the amount of regexp classes gets reduced. For doing that
    /// it needs an abstract clone function
    virtual LPexp* clone() const = 0;
};



/// LPexp concatenation
class Cat final : public LPexp {
public:
    virtual ~Cat();
    void append(LPexp* regexp);
    virtual FirstLast genAuto(NdfAutomata& automata) const;
    virtual Regtype getType() const { return CAT; };
    virtual LPexp* clone() const;

protected:
    std::list<LPexp*> m_children;
};



/// Basic symbol like G or 'customlabel'
class Symbol final : public LPexp {
public:
    Symbol(ustring sym) { m_sym = sym; };
    virtual ~Symbol() {};

    virtual FirstLast genAuto(NdfAutomata& automata) const;
    virtual Regtype getType() const { return SYMBOL; };
    virtual LPexp* clone() const { return new Symbol(*this); };

protected:
    // All symbols are unique ustrings
    ustring m_sym;
};



/// Wildcard regexp
///
/// Named like this to avoid confusion with the automata Wildcard class
class Wildexp final : public LPexp {
public:
    Wildexp(SymbolSet& minus) : m_wildcard(minus) {};
    virtual ~Wildexp() {};

    virtual FirstLast genAuto(NdfAutomata& automata) const;
    virtual Regtype getType() const { return WILDCARD; };
    virtual LPexp* clone() const { return new Wildexp(*this); };

protected:
    // And internally we use the automata's Wildcard type
    Wildcard m_wildcard;
};



/// Ored list of expressions
class Orlist final : public LPexp {
public:
    virtual ~Orlist();
    void append(LPexp* regexp);
    virtual FirstLast genAuto(NdfAutomata& automata) const;
    virtual Regtype getType() const { return OR; };
    virtual LPexp* clone() const;

protected:
    std::list<LPexp*> m_children;
};



// Unlimited repeat: (exp)*
class Repeat final : public LPexp {
public:
    Repeat(LPexp* child) : m_child(child) {};
    virtual ~Repeat() { delete m_child; };
    virtual FirstLast genAuto(NdfAutomata& automata) const;
    virtual Regtype getType() const { return REPEAT; };
    virtual LPexp* clone() const { return new Repeat(m_child->clone()); };

protected:
    LPexp* m_child;
};



// Bounded repeat: (exp){m,n}
class NRepeat final : public LPexp {
public:
    NRepeat(LPexp* child, int min, int max)
        : m_child(child), m_min(min), m_max(max) {};
    virtual ~NRepeat() { delete m_child; };
    virtual FirstLast genAuto(NdfAutomata& automata) const;
    virtual Regtype getType() const { return NREPEAT; };
    virtual LPexp* clone() const
    {
        return new NRepeat(m_child->clone(), m_min, m_max);
    };

protected:
    LPexp* m_child;
    int m_min, m_max;
};



/// Toplevel rule definition
///
/// Note that although it has almost the same interface, this is not
/// a LPexp. It actually binds a light path expression to a certain rule.
/// Making the begin state initial and the end state final. It can't be
/// nested in other light path expressions, it is the root of the tree.
class Rule {
public:
    Rule(LPexp* child, void* rule) : m_child(child), m_rule(rule) {};
    virtual ~Rule() { delete m_child; };
    void genAuto(NdfAutomata& automata) const;

protected:
    LPexp* m_child;
    // Anonymous pointer to the associated object for this rule
    void* m_rule;
};

}  // namespace lpexp

OSL_NAMESPACE_END
