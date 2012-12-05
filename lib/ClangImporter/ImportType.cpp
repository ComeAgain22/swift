//===--- ImportType.cpp - Import Clang Types ------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file implements support for importing Clang types as Swift types.
//
//===----------------------------------------------------------------------===//

#include "ImporterImpl.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/Decl.h"
#include "swift/AST/Module.h"
#include "swift/AST/NameLookup.h"
#include "swift/AST/Pattern.h"
#include "swift/AST/Types.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Sema.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/TypeVisitor.h"
#include "llvm/ADT/StringExtras.h"

using namespace swift;

namespace {
  class SwiftTypeConverter : public clang::TypeVisitor<SwiftTypeConverter, Type>
  {
    ClangImporter::Implementation &Impl;

  public:
    explicit SwiftTypeConverter(ClangImporter::Implementation &impl)
      : Impl(impl) { }


#define DEPENDENT_TYPE(Class, Base)                            \
    Type Visit##Class##Type(const clang::Class##Type *) {      \
      llvm_unreachable("Dependent types cannot be converted"); \
    }
#define TYPE(Class, Base)
#include "clang/AST/TypeNodes.def"

    Type VisitBuiltinType(const clang::BuiltinType *type) {
      switch (type->getKind()) {
      case clang::BuiltinType::Void:
        return Impl.getNamedSwiftType("CVoid");

      case clang::BuiltinType::Bool:
        return Impl.getNamedSwiftType("CBool");

      case clang::BuiltinType::Char_U:
      case clang::BuiltinType::Char_S:
        return Impl.getNamedSwiftType("CChar");

      case clang::BuiltinType::UChar:
        return Impl.getNamedSwiftType("CUnsignedChar");

      case clang::BuiltinType::UShort:
        return Impl.getNamedSwiftType("CUnsignedShort");

      case clang::BuiltinType::UInt:
        return Impl.getNamedSwiftType("CUnsignedInt");

      case clang::BuiltinType::ULong:
        return Impl.getNamedSwiftType("CUnsignedLong");

      case clang::BuiltinType::ULongLong:
        return Impl.getNamedSwiftType("CUnsignedLongLong");

      case clang::BuiltinType::UInt128:
        return Impl.getNamedSwiftType("CUnsignedInt128");

      case clang::BuiltinType::WChar_S:
      case clang::BuiltinType::WChar_U:
        return Impl.getNamedSwiftType("CWideChar");

      case clang::BuiltinType::Char16:
        return Impl.getNamedSwiftType("CChar16");

      case clang::BuiltinType::Char32:
        return Impl.getNamedSwiftType("CChar32");

      case clang::BuiltinType::SChar:
        return Impl.getNamedSwiftType("CSignedChar");

      case clang::BuiltinType::Short:
        return Impl.getNamedSwiftType("CShort");

      case clang::BuiltinType::Int:
        return Impl.getNamedSwiftType("CInt");

      case clang::BuiltinType::Long:
        return Impl.getNamedSwiftType("CLong");

      case clang::BuiltinType::LongLong:
        return Impl.getNamedSwiftType("CLongLong");

      case clang::BuiltinType::Int128:
        return Impl.getNamedSwiftType("CInt128");

      case clang::BuiltinType::Float:
        return Impl.getNamedSwiftType("CFloat");

      case clang::BuiltinType::Double:
        return Impl.getNamedSwiftType("CDouble");

      // Types that cannot be mapped into Swift, and probably won't ever be.
      case clang::BuiltinType::Dependent:
      case clang::BuiltinType::ARCUnbridgedCast:
      case clang::BuiltinType::BoundMember:
      case clang::BuiltinType::BuiltinFn:
      case clang::BuiltinType::Overload:
      case clang::BuiltinType::PseudoObject:
      case clang::BuiltinType::UnknownAny:
        return Type();

      // FIXME: Types that can be mapped, but aren't yet.
      case clang::BuiltinType::Half:
      case clang::BuiltinType::LongDouble:
      case clang::BuiltinType::NullPtr:
        return Type();

      // FIXME: Objective-C types that need a mapping, but I haven't figured
      // out yet.
      case clang::BuiltinType::ObjCClass:
      case clang::BuiltinType::ObjCId:
      case clang::BuiltinType::ObjCSel:
        return Type();
      }
    }

    Type VisitComplexType(const clang::ComplexType *type) {
      // FIXME: Implement once Complex is in the library.
      return Type();
    }

    Type VisitPointerType(const clang::PointerType *type) {
      // FIXME: Function pointer types can be mapped to Swift function types
      // once we have the notion of a "thin" function that does not capture
      // anything.
      if (type->getPointeeType()->isFunctionType())
        return Type();

      // "const char *" maps to Swift's CString.
      clang::ASTContext &clangContext = Impl.getClangASTContext();
      if (clangContext.hasSameType(type->getPointeeType(),
                                   clangContext.CharTy.withConst())) {
        return Impl.getNamedSwiftType("CString");
      }
                                                
      // All other C pointers map to CPointer<T>.
      auto pointeeType = Impl.importType(type->getPointeeType());
      if (!pointeeType)
        return Type();
      return Impl.getNamedSwiftTypeSpecialization("CPointer", pointeeType);
    }

    Type VisitBlockPointerType(const clang::BlockPointerType *type) {
      // Block pointer types are mapped to function types.
      return Impl.importType(type->getPointeeType());
    }

    Type VisitReferenceType(const clang::ReferenceType *type) {
      // FIXME: Reference types are currently handled only in function types.
      // That's probably the right answer, but revisit this later.
      return Type();
    }

    Type VisitMemberPointer(const clang::MemberPointerType *type) {
      // FIXME: Member function pointers can be mapped to curried functions,
      // but only when we can express the notion of a function that does
      // not capture anything from its enclosing context.
      return Type();
    }

    Type VisitArrayType(const clang::ArrayType *type) {
      // FIXME: Array types will need to be mapped differently depending on
      // context.
      return Type();
    }

    Type VisitVectorType(const clang::VectorType *type) {
      // FIXME: We could map these.
      return Type();
    }

    Type VisitExtVectorType(const clang::ExtVectorType *type) {
      // FIXME: We could map these.
      return Type();
    }

    Type VisitFunctionProtoType(const clang::FunctionProtoType *type) {
      // C-style variadic functions cannot be called from Swift.
      if (type->isVariadic())
        return Type();

      // Import the result type.
      auto resultTy = Impl.importType(type->getResultType());
      if (!resultTy)
        return Type();

      SmallVector<TupleTypeElt, 4> params;
      for (auto param = type->arg_type_begin(),
             paramEnd = type->arg_type_end();
           param != paramEnd; ++param) {
        clang::QualType paramTy = *param;
        bool byRef = false;

        if (auto refType = paramTy->getAs<clang::ReferenceType>()) {
          byRef = true;
          paramTy = refType->getPointeeType();
        }

        auto swiftParamTy = Impl.importType(paramTy);
        if (!swiftParamTy)
          return Type();

        if (byRef)
          swiftParamTy = LValueType::get(swiftParamTy,
                                         LValueType::Qual::DefaultForType,
                                         Impl.SwiftContext);

        // FIXME: If we were walking TypeLocs, we could actually get parameter
        // names. The probably doesn't matter outside of a FuncDecl, which
        // we'll have to special-case, but it's an interesting bit of data loss.
        params.push_back(TupleTypeElt(swiftParamTy, Identifier()));
      }

      // Form the parameter tuple.
      auto paramsTy = TupleType::get(params, Impl.SwiftContext);

      // Form the function type.
      return FunctionType::get(paramsTy, resultTy, Impl.SwiftContext);
    }

    Type VisitFunctionNoProtoType(const clang::FunctionNoProtoType *type) {
      // There is no sensible way to describe functions without prototypes
      // in the Swift type system.
      return Type();
    }

    Type VisitParenType(const clang::ParenType *type) {
      auto inner = Impl.importType(type->getInnerType());
      if (!inner)
        return Type();

      return ParenType::get(Impl.SwiftContext, inner);
    }

    Type VisitTypedefType(const clang::TypedefType *type) {
      auto decl = dyn_cast_or_null<TypeDecl>(Impl.importDecl(type->getDecl()));
      if (!decl)
        return nullptr;

      return decl->getDeclaredType();
    }

    Type VisitTypeOfExpr(const clang::TypeOfExprType *type) {
      return Impl.importType(
               Impl.getClangASTContext().getCanonicalType(clang::QualType(type,
                                                                          0)));
    }

    Type VisitTypeOfType(const clang::TypeOfType *type) {
      return Impl.importType(type->getUnderlyingType());
    }

    Type VisitDecltypeType(const clang::DecltypeType *type) {
      return Impl.importType(type->getUnderlyingType());
    }

    Type VisitUnaryTransformType(const clang::UnaryTransformType *type) {
      return Impl.importType(type->getUnderlyingType());
    }

    Type VisitRecordType(const clang::RecordType *type) {
      auto decl = dyn_cast_or_null<TypeDecl>(Impl.importDecl(type->getDecl()));
      if (!decl)
        return nullptr;

      return decl->getDeclaredType();
    }

    Type VisitEnumType(const clang::EnumType *type) {
      auto decl = dyn_cast_or_null<TypeDecl>(Impl.importDecl(type->getDecl()));
      if (!decl)
        return nullptr;

      return decl->getDeclaredType();
    }

    Type VisitElaboratedType(const clang::ElaboratedType *type) {
      return Impl.importType(type->getNamedType());
    }

    Type VisitAttributedType(const clang::AttributedType *type) {
      return Impl.importType(type->getEquivalentType());
    }

    Type VisitSubstTemplateTypeParmType(
           const clang::SubstTemplateTypeParmType *type) {
      return Impl.importType(type->getReplacementType());
    }

    Type VisitTemplateSpecializationType(
           const clang::TemplateSpecializationType *type) {
      return Impl.importType(type->desugar());
    }

    Type VisitAutoType(const clang::AutoType *type) {
      return Impl.importType(type->getDeducedType());
    }

    Type VisitObjCObjectType(const clang::ObjCObjectType *type) {
      // FIXME: Objective-C types are only exposed when referenced via an
      // Objective-C object pointer type. Revisit this.
      return Type();
    }

    Type VisitObjCInterfaceType(const clang::ObjCInterfaceType *type) {
      // FIXME: Objective-C classes are only exposed when referenced via an
      // Objective-C object pointer type. Revisit this.
      return Type();
    }

    Type VisitObjCObjectPointerType(const clang::ObjCObjectPointerType *type) {
      // If this object pointer refers to an Objective-C class (possibly
      // qualified), 
      if (auto interface = type->getInterfaceDecl()) {
        auto imported = cast_or_null<ClassDecl>(Impl.importDecl(interface));
        if (!imported)
          return nullptr;

        // FIXME: Swift cannot express qualified object pointer types, e.g.,
        // NSObject<Proto>, so we drop the <Proto> part.
        return imported->getDeclaredType();
      }

      // FIXME: We fake 'id' and 'Class' by using NSObject. We need a proper
      // 'top' type for Objective-C objects.
      return Impl.getNSObjectType();
    }
  };
}

Type ClangImporter::Implementation::importType(clang::QualType type) {
  SwiftTypeConverter converter(*this);
  return converter.Visit(type.getTypePtr());
}

Type ClangImporter::Implementation::importFunctionType(
       clang::QualType resultType,
       ArrayRef<clang::ParmVarDecl *> params,
       bool isVariadic,
       SmallVectorImpl<Pattern*> &argPatterns,
       SmallVectorImpl<Pattern*> &bodyPatterns,
       clang::Selector selector,
       bool isConstructor) {
  // Cannot import variadic types.
  if (isVariadic)
    return Type();

  // Import the result type.
  auto swiftResultTy = importType(resultType);
  if (!swiftResultTy)
    return Type();

  // Import the parameters.
  SmallVector<TupleTypeElt, 4> swiftParams;
  SmallVector<TuplePatternElt, 4> argPatternElts;
  SmallVector<TuplePatternElt, 4> bodyPatternElts;
  unsigned index = 0;
  for (auto param : params) {
    auto paramTy = param->getType();
    if (paramTy->isVoidType()) {
      ++index;
      continue;
    }

    bool byRef = false;

    // C++ reference types are mapped to [byref].
    if (auto refType = paramTy->getAs<clang::ReferenceType>()) {
      byRef = true;
      paramTy = refType->getPointeeType();
    }

    auto swiftParamTy = importType(paramTy);
    if (!swiftParamTy)
      return Type();

    if (byRef)
      swiftParamTy = LValueType::get(swiftParamTy,
                                     LValueType::Qual::DefaultForType,
                                     SwiftContext);

    // Figure out the name for this parameter.
    Identifier bodyName = importName(param->getDeclName());
    Identifier name = bodyName;
    if ((index > 0 || isConstructor) && index < selector.getNumArgs()) {
      // For parameters after the first, or all parameters in a constructor,
      // the name comes from the selector.
      name = importName(selector.getIdentifierInfoForSlot(index));
    }

    // Compute the pattern to put into the body.
    auto bodyVar
      = new (SwiftContext) VarDecl(importSourceLoc(param->getLocation()),
                                   bodyName, swiftParamTy, firstClangModule);
    bodyVar->setClangDecl(param);
    Pattern *bodyPattern = new (SwiftContext) NamedPattern(bodyVar);
    bodyPattern->setType(bodyVar->getType());
    bodyPattern
      = new (SwiftContext) TypedPattern(bodyPattern,
                                        TypeLoc::withoutLoc(swiftParamTy));
    bodyPattern->setType(bodyVar->getType());
    bodyPatternElts.push_back(TuplePatternElt(bodyPattern));

    // Compute the pattern to put into the argument list, which may be
    // different (when there is a selector involved).
    auto argVar = bodyVar;
    auto argPattern = bodyPattern;
    if (bodyName != name) {
      argVar = new (SwiftContext) VarDecl(importSourceLoc(param->getLocation()),
                                          name, swiftParamTy, firstClangModule);
      argVar->setClangDecl(param);
      argPattern = new (SwiftContext) NamedPattern(argVar);
      argPattern->setType(argVar->getType());
      argPattern
        = new (SwiftContext) TypedPattern(argPattern,
                                          TypeLoc::withoutLoc(swiftParamTy));
      argPattern->setType(argVar->getType());
    }
    argPatternElts.push_back(TuplePatternElt(argPattern));

    // Add the tuple element for the function type.
    swiftParams.push_back(TupleTypeElt(swiftParamTy, name));
    ++index;
  }

  // Form the parameter tuple.
  auto paramsTy = TupleType::get(swiftParams, SwiftContext);

  // Form the body and argument patterns.
  bodyPatterns.push_back(TuplePattern::create(SwiftContext, SourceLoc(),
                                              bodyPatternElts, SourceLoc()));
  argPatterns.push_back(TuplePattern::create(SwiftContext, SourceLoc(),
                                             argPatternElts, SourceLoc()));

  // Form the function type.
  return FunctionType::get(paramsTy, swiftResultTy, SwiftContext);
}


Type ClangImporter::Implementation::getNamedSwiftType(StringRef name) {
  if (!swiftModule) {
    for (auto module : SwiftContext.LoadedModules) {
      if (module.second->Name.str() == "swift") {
        swiftModule = module.second;
        break;
      }
    }

    // The 'swift' module hasn't been loaded. There's nothing we can do.
    if (!swiftModule)
      return Type();
  }

  // Look for the type.
  UnqualifiedLookup lookup(SwiftContext.getIdentifier(name), swiftModule);
  if (auto type = lookup.getSingleTypeResult()) {
    return type->getDeclaredType();
  }

  return Type();
}

Type
ClangImporter::Implementation::
getNamedSwiftTypeSpecialization(StringRef name, ArrayRef<Type> args) {
  if (!swiftModule) {
    for (auto module : SwiftContext.LoadedModules) {
      if (module.second->Name.str() == "swift") {
        swiftModule = module.second;
        break;
      }
    }

    // The 'swift' module hasn't been loaded. There's nothing we can do.
    if (!swiftModule)
      return Type();
  }

  UnqualifiedLookup genericSliceLookup(SwiftContext.getIdentifier("CPointer"),
                                       swiftModule);
  if (TypeDecl *typeDecl = genericSliceLookup.getSingleTypeResult()) {
    if (auto nominalDecl = dyn_cast<NominalTypeDecl>(typeDecl)) {
      if (auto params = nominalDecl->getGenericParams()) {
        if (params->size() == args.size()) {
          Type implTy = BoundGenericType::get(nominalDecl, Type(), args);
          // FIXME: How do we ensure that this type gets validated?
          return implTy;
        }
      }
    }
  }

  return Type();
}

Type ClangImporter::Implementation::getNSObjectType() {
  if (NSObjectTy)
    return NSObjectTy;

  auto &sema = Instance->getSema();

  // Map the name. If we can't represent the Swift name in Clang, bail out now.
  auto clangName = &getClangASTContext().Idents.get("NSObject");

  // Perform name lookup into the global scope.
  // FIXME: Map source locations over.
  clang::LookupResult lookupResult(sema, clangName, clang::SourceLocation(),
                                   clang::Sema::LookupOrdinaryName);
  if (!sema.LookupName(lookupResult, /*Scope=*/0)) {
    return Type();
  }

  for (auto decl : lookupResult) {
    if (auto swiftDecl = importDecl(decl->getUnderlyingDecl())) {
      if (auto classDecl = dyn_cast<ClassDecl>(swiftDecl)) {
        NSObjectTy = classDecl->getDeclaredType();
        return NSObjectTy;
      }
    }
  }

  return Type();
}

