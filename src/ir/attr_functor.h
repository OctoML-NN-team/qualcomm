/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file attr_functor.h
 * \brief A way to define arbitrary function signature
 *        with dispatch on common attributes.
 *
 * Common attributes include:
 *  - int, float, str constants
 *  - array of attributes
 *  - map of attributes
 */
#ifndef TVM_IR_ATTR_FUNCTOR_H_
#define TVM_IR_ATTR_FUNCTOR_H_

#include <tvm/node/functor.h>
#include <utility>

namespace tvm {

template <typename FType>
class AttrFunctor;

#define ATTR_FUNCTOR_DEFAULT                                        \
  { return VisitAttrDefault_(op, std::forward<Args>(args)...); }


#define ATTR_FUNCTOR_DISPATCH(OP)                                       \
  vtable.template set_dispatch<OP>(                                     \
      [](const ObjectRef& n, TSelf* self, Args... args) {                 \
        return self->VisitAttr_(static_cast<const OP*>(n.get()),  \
                                std::forward<Args>(args)...);           \
      });                                                               \

// A functor for common attribute information.
template <typename R, typename... Args>
class AttrFunctor<R(const ObjectRef& n, Args...)> {
 private:
  using TSelf = AttrFunctor<R(const ObjectRef& n, Args...)>;
  using FType = tvm::NodeFunctor<R(const ObjectRef& n, TSelf* self, Args...)>;

 public:
  /*! \brief the result type of this functor */
  using result_type = R;
  /*!
   * \brief The functor call.
   * \param n The expression node.
   * \param args Additional arguments.
   * \return The result of the call
   */
  virtual R VisitAttr(const ObjectRef& n, Args... args) {
    static FType vtable = InitVTable();
    if (vtable.can_dispatch(n)) {
      return vtable(n, this, std::forward<Args>(args)...);
    } else {
      return VisitAttrDefault_(n.get(), std::forward<Args>(args)...);
    }
  }
  virtual R VisitAttrDefault_(const Object* node, Args... args) = 0;
  virtual R VisitAttr_(const ArrayNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const StrMapNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::IntImmNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::FloatImmNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::StringImmNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  // deep comparison of symbolic integer expressions.
  virtual R VisitAttr_(const VarNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const SizeVarNode* op, Args... args) {
    return VisitAttr_(static_cast<const VarNode*>(op), std::forward<Args>(args)...);
  }
  virtual R VisitAttr_(const ir::AddNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::SubNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::MulNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::DivNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::ModNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::FloorDivNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::FloorModNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::MinNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::MaxNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::GENode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::GTNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::LTNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::LENode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::EQNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::NENode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::AndNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::OrNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::NotNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::CastNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::CallNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;
  virtual R VisitAttr_(const ir::SelectNode* op, Args... args) ATTR_FUNCTOR_DEFAULT;

 private:
  // initialize the vtable.
  static FType InitVTable() {
    using namespace ir;
    FType vtable;
    // Set dispatch
    ATTR_FUNCTOR_DISPATCH(StrMapNode);
    ATTR_FUNCTOR_DISPATCH(ArrayNode);
    ATTR_FUNCTOR_DISPATCH(IntImmNode);
    ATTR_FUNCTOR_DISPATCH(FloatImmNode);
    ATTR_FUNCTOR_DISPATCH(StringImmNode);
    ATTR_FUNCTOR_DISPATCH(VarNode);
    ATTR_FUNCTOR_DISPATCH(SizeVarNode);
    ATTR_FUNCTOR_DISPATCH(AddNode);
    ATTR_FUNCTOR_DISPATCH(SubNode);
    ATTR_FUNCTOR_DISPATCH(MulNode);
    ATTR_FUNCTOR_DISPATCH(DivNode);
    ATTR_FUNCTOR_DISPATCH(ModNode);
    ATTR_FUNCTOR_DISPATCH(FloorDivNode);
    ATTR_FUNCTOR_DISPATCH(FloorModNode);
    ATTR_FUNCTOR_DISPATCH(MinNode);
    ATTR_FUNCTOR_DISPATCH(MaxNode);
    ATTR_FUNCTOR_DISPATCH(GENode);
    ATTR_FUNCTOR_DISPATCH(GTNode);
    ATTR_FUNCTOR_DISPATCH(LENode);
    ATTR_FUNCTOR_DISPATCH(LTNode);
    ATTR_FUNCTOR_DISPATCH(EQNode);
    ATTR_FUNCTOR_DISPATCH(NENode);
    ATTR_FUNCTOR_DISPATCH(AndNode);
    ATTR_FUNCTOR_DISPATCH(OrNode);
    ATTR_FUNCTOR_DISPATCH(NotNode);
    ATTR_FUNCTOR_DISPATCH(CastNode);
    ATTR_FUNCTOR_DISPATCH(CallNode);
    ATTR_FUNCTOR_DISPATCH(SelectNode);
    return vtable;
  }
};

class AttrsEqualHandler :
      protected AttrFunctor<bool(const ObjectRef&, const ObjectRef&)> {
 public:
  /*!
   * \brief Check if lhs equals rhs
   * \param lhs The left operand.
   * \param rhs The right operand.
   */
  bool Equal(const ObjectRef& lhs, const ObjectRef& rhs);

 protected:
  bool VisitAttrDefault_(const Object* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ArrayNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const StrMapNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::IntImmNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::FloatImmNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::StringImmNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::AddNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::SubNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::MulNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::DivNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::ModNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::FloorDivNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::FloorModNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::MinNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::MaxNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::GENode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::GTNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::LTNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::LENode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::EQNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::NENode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::AndNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::OrNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::NotNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::CastNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::CallNode* lhs, const ObjectRef& other) final;
  bool VisitAttr_(const ir::SelectNode* lhs, const ObjectRef& other) final;
};

class AttrsHashHandler :
      protected AttrFunctor<size_t(const ObjectRef&)> {
 public:
  /*!
   * \brief Get hash value of node
   * \param node The node to be hashed.
   */
  size_t Hash(const ObjectRef& node) {
    if (!node.defined()) return 0;
    return this->VisitAttr(node);
  }

 protected:
  size_t VisitAttrDefault_(const Object* lhs) final;
  size_t VisitAttr_(const ir::IntImmNode* lhs) final;
  size_t VisitAttr_(const ir::FloatImmNode* lhs) final;
  size_t VisitAttr_(const ir::StringImmNode* lhs) final;
  size_t VisitAttr_(const ArrayNode* lhs) final;
  size_t VisitAttr_(const StrMapNode* lhs) final;
  size_t VisitAttr_(const ir::AddNode* op) final;
  size_t VisitAttr_(const ir::SubNode* op) final;
  size_t VisitAttr_(const ir::MulNode* op) final;
  size_t VisitAttr_(const ir::DivNode* op) final;
  size_t VisitAttr_(const ir::ModNode* op) final;
  size_t VisitAttr_(const ir::FloorDivNode* op) final;
  size_t VisitAttr_(const ir::FloorModNode* op) final;
  size_t VisitAttr_(const ir::MinNode* op) final;
  size_t VisitAttr_(const ir::MaxNode* op) final;
  size_t VisitAttr_(const ir::GENode* op) final;
  size_t VisitAttr_(const ir::GTNode* op) final;
  size_t VisitAttr_(const ir::LENode* op) final;
  size_t VisitAttr_(const ir::LTNode* op) final;
  size_t VisitAttr_(const ir::EQNode* op) final;
  size_t VisitAttr_(const ir::NENode* op) final;
  size_t VisitAttr_(const ir::AndNode* op) final;
  size_t VisitAttr_(const ir::OrNode* op) final;
  size_t VisitAttr_(const ir::NotNode* op) final;
  size_t VisitAttr_(const ir::CastNode* op) final;
  size_t VisitAttr_(const ir::CallNode* op) final;
  size_t VisitAttr_(const ir::SelectNode* op) final;
  /*!
   * \brief alias of dmlc::HashCombine
   * \param lhs The first hash value.
   * \param rhs The second hash value.
   */
  static size_t Combine(size_t lhs, size_t rhs) {
    return dmlc::HashCombine(lhs, rhs);
  }
};
}  // namespace tvm
#endif  // TVM_IR_ATTR_FUNCTOR_H_
