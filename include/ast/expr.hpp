/*-------------------------------------------------------------------------------------------------
| This file is distributed under the MIT License.
| See accompanying file /LICENSE for details.
| Author(s): Matthew Amy
*------------------------------------------------------------------------------------------------*/
#pragma once

#include "ast/kinds.hpp"
#include "ast/ast_node.hpp"
#include "ast/types.hpp"

#include <memory>
#include <string>
#include <vector>

#include <math.h>

namespace synthewareQ {
namespace ast {

  enum class bop {
    add,
    sub,
    div,
    mult,
    pow,
    eq,
  };

  enum class uop {
    sin,
    cos,
    tan,
    exp,
    ln,
    sqrt,
    minus,
    plus,
  };

  /*! \brief Base class of expressions */
  class Expr : public AST_node {
  public:
    Expr(Location loc, ast_nodes kind)
      : AST_node(loc, kind)
    {}

    virtual ~Expr() = 0;
  };
  Expr::~Expr() {}


  /*! \brief Binary expressions */
  class Expr_bop final : public Expr {
  private:
    std::unique_ptr<Expr> left_;
    bop op_;
    std::unique_ptr<Expr> right_;

    Expr_bop(Location loc, Expr* left, bop op, Expr* right)
      : Expr(loc, ast_nodes::expr_bop)
      , left_(left)
      , op_(op)
      , right_(right)
    {}

  public:
    static Expr_bop* create(Location loc, Expr* left, bop op, Expr* right) {
      return new Expr_bop(loc, left, op, right);
    }

    bop get_op() const {
      return op_;
    }

    bool is(bop op) const {
      return op_ == op;
    }

    Expr* get_left() {
      return left_.get();
    }

    void set_left(Expr* left) {
      left_.reset(left);
    }

    Expr* get_right() {
      return right_.get();
    }

    void set_right(Expr* right) {
      right_.reset(right);
    }

  };


  /*! \brief Unary expressions */
  class Expr_uop final : public Expr {
  private:
    uop op_;
    std::unique_ptr<Expr> subexpr_;

    Expr_uop(Location loc, uop op, Expr* subexpr)
      : Expr(loc, ast_nodes::expr_uop)
      , op_(op)
      , subexpr_(subexpr)
    {}
    
  public:
    static Expr_uop* create(Location loc, uop op, Expr* subexpr) {
      return new Expr_uop(loc, op, subexpr);
    }

	uop op() const {
      return op_;
	}

	bool is(uop op) const {
	  return op_ == op;
	}

    Expr* get_subexpr() {
      return subexpr_.get();
    }

    void set_subexpr(Expr* subexpr) {
      subexpr_.reset(subexpr);
    }

  };


  /*! \brief PI */
  class Expr_pi final : public Expr {
  private:
    Expr_pi(Location loc)
      : Expr(loc, ast_nodes::expr_pi)
    {}

  public:
    static Expr_pi* create(Location loc) {
      return new Expr_pi(loc);
    }

    double numeric_value() const
    {
      return M_PI;
    }

  };


  /*! \brief Integer literal */
  class Expr_int final : public Expr {
  private:
	int32_t value_;

    Expr_int(Location loc, int32_t value)
      : Expr(loc, ast_nodes::expr_int)
    {}

  public:
    static Expr_int* create(Location loc, int32_t value) {
      return new Expr_int(loc, value);
    }

    int32_t numeric_value() const {
      return value_;
    }

  };


  /*! \brief Real-valued literal */
  class Expr_real final : public Expr {
  private:
    double value_;

    Expr_real(Location loc, double value)
      : Expr(loc, ast_nodes::expr_real)
    {}

  public:
    static Expr_real* create(Location loc, double value) {
      return new Expr_real(loc, value);
    }

    double numeric_value() const {
      return value_;
    }

  };


  /* \brief Variable expressions */
  class Expr_var final : public Expr {
  private:
    std::string_view name_;
    std::unique_ptr<Type> type_;

    Expr_var(Location loc, std::string_view name, Type* type)
      : Expr(loc, ast_nodes::expr_var)
      , name_(name)
      , type_(type)
    {}
    
  public:
    static Expr_var* create(Location loc, std::string_view name, Type* type) {
      return new Expr_var(loc, name, type);
    }

    std::string_view name() const {
      return name_;
    }

    bool known_type() const {
      return type_.get() != nullptr;
    }

    Type* get_type() {
      return type_.get();
    }

    void set_type(Type* type) {
      type_.reset(type);
    }

  };

  /* \brief Offsets into a register */
  class Expr_offset final : public Expr {
  private:
    std::unique_ptr<Expr_var> var_;
    uint32_t offset_;

    Expr_offset(Location loc, Expr_var* var, uint32_t offset)
      : Expr(loc, ast_nodes::expr_offset)
      , var_(var)
      , offset_(offset)
    {}
    
  public:
    static Expr_offset* create(Location loc, Expr_var* var, uint32_t offset) {
      return new Expr_offset(loc, var, offset);
    }

    std::string_view base_name() const {
      if (Expr_var* var = var_.get()) {
        return var->name();
      } else {
        return "";
      }
    }

    Expr_var* get_var() {
      return var_.get();
    }

    void set_var(Expr_var* var) {
      var_.reset(var);
    }

    uint32_t get_offset() const
    {
      return offset_;
    }

    void set_offset(uint32_t offset)
    {
      offset_ = offset;
    }

  };


  /* \brief List of expressions */
  class Expr_list : public AST_node {
  private:
    using expr_list_t = std::vector<std::unique_ptr<Expr> >;
    expr_list_t list_;

    Expr_list(Location loc)
      : AST_node(loc, ast_nodes::expr_list)
    {}

  public:
    static Expr_list* create(Location loc) {
      return new Expr_list(loc);
    }

    class iterator : expr_list_t::iterator {
    friend class Expr_list;
    public:
      using internal_t = expr_list_t::iterator;
      using value_type = Expr;
      using pointer = Expr*;
      using reference = Expr&;
      
      iterator(const internal_t &it) : internal_t(it) {}

      reference operator*() const {
        return *(this->internal_t::operator*());
      }

      pointer operator->() const {
        return this->internal_t::operator*().get();
      }

      reference operator[](size_t n) const {
        return *(this->internal_t::operator[](n));
      }

      iterator& operator++() {
        this->internal_t::operator++();
        return *this;
      }

      iterator operator++(int) {
        auto tmp = *this;
        this->internal_t::operator++();
        return tmp;
      }
    };
      
    iterator begin() {
      return iterator(list_.begin());
    }

    iterator end() {
      return iterator(list_.end());
    }

    void push_back(Expr* expr) {
      list_.push_back(std::unique_ptr<Expr>(expr));
    }

    void insert(iterator it, Expr* expr) {
      list_.insert(static_cast<expr_list_t::iterator>(it), std::unique_ptr<Expr>(expr));
    }
  };

} // namespace qasm
} // namespace tweedledee