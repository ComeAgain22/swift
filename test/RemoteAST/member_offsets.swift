// RUN: %target-swift-remoteast-test %s | FileCheck %s

// REQUIRES: PTRSIZE=64

@_silgen_name("printTypeMemberOffset")
func printTypeMemberOffset(_: Any.Type, _: StaticString)

@_silgen_name("printTypeMetadataMemberOffset")
func printTypeMetadataMemberOffset(_: Any.Type, _: StaticString)

printTypeMemberOffset((Int,Bool,Float,Bool,Int16).self, "0")
// CHECK: found offset: 0

printTypeMemberOffset((Int,Bool,Float,Bool,Int16).self, "1")
// CHECK: found offset: 8

printTypeMemberOffset((Int,Bool,Float,Bool,Int16).self, "2")
// CHECK: found offset: 12

printTypeMemberOffset((Int,Bool,Float,Bool,Int16).self, "3")
// CHECK: found offset: 16

printTypeMemberOffset((Int,Bool,Float,Bool,Int16).self, "4")
// CHECK: found offset: 18

printTypeMemberOffset((Int,Bool,Float,Bool,Int16).self, "5")
// CHECK: type has no member named '5'

struct A {
  var a: Int
  var b: Bool
  var c: Float
  var d: Bool
  var e: Int16
}

printTypeMemberOffset(A.self, "a")
// CHECK: found offset: 0

printTypeMemberOffset(A.self, "b")
// CHECK: found offset: 8

printTypeMemberOffset(A.self, "c")
// CHECK: found offset: 12

printTypeMemberOffset(A.self, "d")
// CHECK: found offset: 16

printTypeMemberOffset(A.self, "e")
// CHECK: found offset: 18

printTypeMemberOffset(A.self, "f")
// CHECK: type has no member named 'f'

struct B<T> {
  var a: Int
  var b: Bool
  var c: T
  var d: Bool
  var e: Int16
}

printTypeMemberOffset(B<Float>.self, "a")
// CHECK: found offset: 0

printTypeMemberOffset(B<Float>.self, "b")
// CHECK: found offset: 8

printTypeMemberOffset(B<Float>.self, "c")
// CHECK: found offset: 12

printTypeMemberOffset(B<Float>.self, "d")
// CHECK: found offset: 16

printTypeMemberOffset(B<Float>.self, "e")
// CHECK: found offset: 18

printTypeMemberOffset(B<Float>.self, "f")
// CHECK: type has no member named 'f'