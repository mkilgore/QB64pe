# Expanded `_Mem` and `_TypeInfo`/reflection API

The APIs here have a few goals they are trying to accomplish:

1. Attach layout information to each `_Mem` that tracks what it holds inside.
2. Allow `_Mem`'s to be "bound" to a variable, such that interacting with the variable accesses the backing `_Mem` directly.
3. Allow `_Mem`s to hold and use `String` and `_Dynamic` TYPE members (which you cannot interact with using `_MemGet` and `_MemPut`).
4. Track the full memory block a `_Mem` comes from, such that you can take a `_Mem` to an inner UDT member and get a `_Mem` to its containing UDT.
5. Use the layout information to allow `_Mem` with any type while enforcing `_MemGet` and `_MemPut` restrictions.
6. Allow the layout information to be queried at runtime, such that code can be written to interact with `_Mem`s without knowing what they are.

The expanded system results in a number of new `_Mem*` commands along with a
new `_TypeInfo` built-in type and `_Type*` commands.

## `struct mem_layout` internal C++ layout description

The core of the system is a new `struct mem_layout` structure in libqb, this
holds information on a QB64 type. There is one `mem_layout` per type and they
are (with a few exceptions) statically allocated at compile time, so pointer
comparison of `mem_layout`s can be used to determine if two things have the
same layout.

`struct mem_layout` holds information on the 'kind' of layout (primitive type,
UDT, array, String, etc.), its size, and then either its primitive type (Ex.
`Long`, `Integer`, etc.) or information on its contained type(s) (Ex. The members
of a UDT, the type of elements of an array, etc.). Thus given a `mem_layout` of
a given `_Mem`, you can understand exactly what is located at every byte.

Each layout also marks whether a layout is "copyable", which allows it to be
used with `_MemGet` and `_MemPut`. This thus maintains the runtime safety of
those actions even on TYPEs containing `String`s and `_Dynamic`, via checking
the region the action is done across and only allowing it if the entire region
is marked as "copyable". (Note: "copyable" just means the type does not require
special considerations when copying it to another variable, it does not mean it's
safe to modify it directly. Ex `_Mem` is "copyable")

## `_Mem` expansion

`_Mem` itself gains a few new internal fields: `$_LAYOUT`, `$_LAYOUT_OFFSET`, and `$_VALUE`

These fields track the layout information for the block as a whole (for
`$_LAYOUT`) and for the specific entry within the block that the `_Mem` is
identifying (`$_VALUE`). `$_LAYOUT_OFFSET` tracks how far into the `$_LAYOUT`
 `$_VALUE` is. `$_LAYOUT` and `$_VALUE` are pointers to `mem_layout`s.

```vb
TYPE foo
    l1 As Long
    i1 As Integer
END TYPE

TYPE bar
    foo1 As foo
    foo2 As foo
END TYPE

Dim b As bar
Dim f As foo
Dim m As _MEM

m = _MEM(b.foo1.l1) ' $_LAYOUT is 'bar', $_VALUE is 'Long', $_LAYOUT_OFFSET is zero
m = _MEM(b.foo2) ' $_LAYOUT is 'bar', $_VALUE is 'foo', $_LAYOUT_OFFSET is 6 (length of 'foo1')
m = _MEM(f.i1) ' $_LAYOUT is 'foo', $_VALUE is 'Integer', $_LAYOUT_OFFSET is 4 (length of 'l1')
```

Along with this, `_MemNew` gains two new forms: `_MemNew(As type)` and `_MemNew(typeInfo)`.

| call | gives | meaning |
|---|---|---|
| `_MemNew(AS type)` | `_Mem` | Creates a `_MemNew` block the correct size for type, and initializes it properly |
| `_MemNew(typeInfo)` | `_TypeInfo` | The same as `As type`, but creates it using the given `_TypeInfo` (explained later in this document) |

Using this new syntax allows creating `_MemNew` blocks with a `mem_layout`
attached to them based on the type they were made from. It also correctly
initializes the contents of the mem block for that type, so Ex. `String`s get a
proper `qbs_new()`, `_Dynamic` gets properly set up, etc.

## `_MemBound`, `_MemBind`, `_MemUnbind`

This was the original goal, it allow `_Mem`s to be accessed directly
as though it's a regular variable. This avoids having to copy in/out of the
`_MEM` or continually use `_MemGet`/`_MemPut` which are very unergonomic and
error prone.

`_MemBound` is declared on the `Dim` statement to represent that this variable
is to be 'bound' to a `_Mem`. The `_Mem` can either be specified directly to
`_MemBound()`, or later via `_MemBind` (then removed with `_MemUnbind`). When a
variable is 'bound' any access of that variable actually accesses the memory in
the bound `_Mem` block. The layout information on the `_Mem` prevents binding a
block to variables of the wrong type (`$CHECKING:OFF` bypasses this). For example:

```vb
Dim b As bar

Dim m1 As _Mem: m1 = _Mem(b.foo1)
Dim m2 As _Mem: m2 = _Mem(b.foo1.l1)

Dim f2 As _MemBound(m1) foo
Print f2.l1 ' Access the l1 member of foo, which in turn access the memory in the bound `m1` and ends up reading `b.foo1.l1`

Dim f3 As _MemBound(m1) Long
Print f3 ' This is allowed, you can bind to the `$_VALUE` layout or any deeper layouts at
         ' the same offset. In this case, `Long` is at the same offset because it is the
         ' first member of `foo`.

Increment f3 ' Also valid, the bound variable can be passed by-ref to a
             ' SUB/FUNCTION and that SUB/FUNCTION does not need to be aware of the binding.

Dim f4 As _MemBound(m2) foo ' Runtime error - '$_VALUE' is a `Long`, so there is no valid way to bind a 'foo'

Dim f5 As _MemBoundContainer(m2) foo
Print f5.i1 ' Valid, 'Container' starts at `$_LAYOUT` and then looks 'inwards' towards `$_VALUE` for the requested bind type

Dim f6 As _MemBound foo
_MemBind f6, m1
Print f6.i1
_MemUnbind f6 ' After being unbound it can be bound to another _Mem

SUB increment(l As Long)
    l = l + 1
END SUB
```

### Arrays

The `mem_layout` system makes a distinction between a `_Mem` holding a run of
elements, and a `_Mem` holding an actual array descriptor.

The existing command `_Mem(a())` gives a layout for a "run of elements" - the
`_Mem` points directly at the backing array memory, rather than the array
descriptor. Thus a "run of elements" has no array bounds and cannot be bound
to an array.

The new `_MemArray(a())` gives a `_Mem` pointing at the array descriptor. This
version thus retains the bounds and other information from the original array
and can be `_MemBound` to an array, redimmed, etc.

Static arrays in `TYPE` are a special case - They do not have array descriptors
at runtime (they are created ad-hoc when needed). Their dimension information is
thus stored in the `mem_layout` and when bound to an array the descriptor is created
and filled in with this information.

### `_MemBound` safety tracking

The safety of this system ties into the existing `mem_lock` system which tracks
the validity of `_Mem` blocks. Additionally we do not check the validity of the
`_Mem` when accessed through a bound variable, only at bind time. When a `_Mem`
is bound, an entry on the `mem_lock` is incremented to track this. Invalidating
a `_Mem` which is currently bound is a fatal runtime error, so Ex. Taking a
`_MemElement` to an element inside a dynamic array, binding it to a variable,
and then REDIMing the array is a fatal error (REDIMming invalidates all the
`_MEM`s to elements inside of the array).

This safety approach means that once the variable is bound it is treated like
a normal variable for all other purposes. Critically, it can be passed by-ref
to SUB/FUNCTIONs and those SUB/FUNCTIONs do not need to know it is a bound
variable.

## New `_TypeInfo` and reflection API

The `mem_layout` information created at compile time can subsequently be
accessed at runtime via the new `_TypeInfo` built-in UDT and `_Type*` APIs:

```vb
CONST _TYPE_NONE = 0        ' names no type at all
CONST _TYPE_PRIMITIVE = 1   ' LONG, SINGLE, _OFFSET, etc.
CONST _TYPE_STRING = 2      ' a variable-length STRING
CONST _TYPE_FIXEDSTRING = 3 ' STRING * n
CONST _TYPE_UDT = 4         ' a TYPE
CONST _TYPE_ARRAY = 5       ' a whole array, as _MemArray names one
CONST _TYPE_ELEMENTS = 6    ' a run of one element type, repeating

TYPE _TypeInfo
    SIZE AS _OFFSET   ' the number of bytes this type occupies
    KIND AS LONG      ' one of the _TYPE_* constants
    ' $_LAYOUT - Internal, the mem_layout this _TypeInfo describes
END TYPE
```

To acquire a `_TypeInfo` you use `_TypeOf` either with a `As typename` directly, or with a variable:

| call | gives | meaning |
|---|---|---|
| `_TypeOf(AS type)` | `_TypeInfo` | Get a `_TypeInfo` for the given type name - a `Long`, `STRING`, a `TYPE`, etc. |
| `_TypeOf(variable)` | `_TypeInfo` | The `_TypeInfo` for a variable, array, element, UDT member, etc. |

There are then APIs for getting generic information about and using the `_TypeInfo`s:

| call | gives | meaning |
|---|---|---|
| `_TypeName$(typeInfo)` | `STRING` | The type as written in source |
| `_TypeAreEqual(t1, t2)` | `LONG` | `_TRUE` if these track the same type |
| `_TypeIsEmpty(typeInfo)` | `LONG` | `_TRUE` when it identifies no type, the default when declared |
| `_TypeIsCopyable(typeInfo)` | `LONG` | Indicates support with `_MemGet`, and `_MemPut` |
| `_TypeClear typeInfoVariable` | | After calling this `_TypeIsEmpty` returns `_TRUE` again |

Some of the possible `KIND` types have their own sets of APIs for getting their information:

### UDTs

Note: `member` is the index of the member in the UDT, starting at 1

| call | gives |
|---|---|
| `_TypeUdtMemberCount(typeInfo)` | `LONG`, total number of members |
| `_TypeUdtMemberName$(typeInfo, member)` | `STRING` name in source of the given member |
| `_TypeUdtMemberType(typeInfo, member)` | `_TypeInfo` of the given member |
| `_TypeUdtMemberOffset(typeInfo, member)` | `_OFFSET` of the member within the UDT |
| `_TypeUdtMemberIndex(typeInfo, memberName$)` | `LONG` identifying which member has this name, or 0 (case insensitive) |

### Arrays and runs of Elements

| call | gives |
|---|---|
| `_TypeArrayElementType(typeInfo)` | `_TypeInfo` of the elements - takes an array or an element run |
| `_TypeArrayDimensions(typeInfo)` | `LONG` giving the number of dimensions for this array; 0 for an element run |
| `_TypeArrayHasBounds(typeInfo)` | `LONG` indicating if bounds are part of the type information (static arrays in UDTs) |
| `_TypeArrayLBound(typeInfo, dimension)` | `_OFFSET` - only valid when 'HasBounds' is `_TRUE` |
| `_TypeArrayUBound(typeInfo, dimension)` | `_OFFSET` - only valid when 'HasBounds' is `_TRUE` |

`HasBounds` is only `_TRUE` for a static array declared inside a `TYPE`, which is
the one case where the bounds really are type information - there is no array
descriptor. Every other array keeps its bounds in a descriptor at runtime, so they
are not type information.

## Generic `_Mem` access

You can query the layout information attached to a `_Mem` via a few commands:

| call | gives | meaning |
|---|---|---|
| `_MemTypeOf(mem)` | `_TypeInfo` | Gives the `_TypeInfo` associated with the given `_Mem` (`$_VALUE`) |
| `_MemRegionTypeOf(mem)` | `_TypeInfo` | Gives the `_TypeInfo` associated with block the `_Mem` is from (`$_LAYOUT`) |
| `_MemIsType(mem, type)` | `Long` | Gives `_TRUE` if the `$_VALUE` or inner type at offset 0 matches the specified type |
| `_MemIsInContainer(mem, type)` | `Long` | Gives `_TRUE` if the given `_Mem` could be container-bound to `type`. |

### UDTs

UDTs have one special command, `_MemUdtMember(m, index)`. This command returns
a `_Mem` which refers to the member of the given UDT at index `index`. The
`$_VALUE` is the layout of that UDT member.

### Arrays

You cannot bind to an array without specifying the type of element you're binding,
so these APIs exist to allow you to interact with an array without knowing its
element type.

| call | gives | meaning |
|---|---|---|
| `_MemArrayDimensions(memBlock)` | `LONG` | The total number of dimensions |
| `_MemArrayIsDynamic(memBlock)` | `LONG` | Whether this array can be resized, so whether `_MemArrayRedim` and `REDIM` would be allowed |
| `_MemArrayCount(memBlock)` | `_INTEGER64` | Total number of elements, counting all dimensions |
| `_MemArrayLBound(memBlock, dimension)` | `_OFFSET` | LBound of a given dimension |
| `_MemArrayUBound(memBlock, dimension)` | `_OFFSET` | UBound of a given dimension |
| `_MemArrayElement(memBlock, index)` | `_MEM` | A `_Mem` to a single element in the array, indexed from 0 in flat-storage order, ignoring bounds and dimensions |
| `_MemArrayElements(memBlock)` | `_MEM` | A `_Mem` to the whole run of elements backing this array |
| `_MemArrayRedim memBlock, lower(), upper()` | | Resizes the array to the bounds in lower() and upper() (arrays of `Long`). The size of lower() and upper() should match the number of dimensions |
| `_MemArraySwap memBlock, index1, index2` | | Exchanges two elements, indexed as `_MemArrayElement` indexes them. This is always allowed, even for non-copyable members |

## Example

Declaring some nested TYPEs, creating _MEMs and accessing data through them via `_MemBound()`:

```vb
TYPE Point
    x AS LONG
    y AS LONG
END TYPE
TYPE Segment
    a AS Point
    b AS Point
END TYPE

DIM s AS Segment
s.a.x = 1: s.a.y = 2: s.b.x = 30: s.b.y = 40

DIM second AS _MEM: second = _MEM(s.b)   ' $_LAYOUT 'Segment', $_VALUE 'Point', offset 8

DIM p AS _MemBound(second) Point
PRINT p.x; p.y                           ' 30 40
p.x = 33
PRINT s.b.x                              ' 33

DIM firstX AS _MemBound(second) LONG     ' Binds to 'LONG' rather than 'Point'.
                                         ' 'x' is one layer down from 'Point' at the
                                         ' same offset, so binding to it directly is allowed

Bump firstX                              ' Passed by-ref
PRINT s.b.x                              ' 34

' 'Segment' is outwards from `$_VALUE`, so hence using _MemBoundContainer is necessary
PRINT _MemIsType(second, Segment); _MemIsInContainer(second, Segment)   ' 0 -1
DIM back AS _MemBoundContainer(second) Segment
PRINT back.a.x; back.b.x                 ' 1 34

SUB Bump (n AS LONG)
    n = n + 1
END SUB
```

A generic Sort that takes a `_MemArray` and sorts generically based on the first
member of each element (You could also do something like sort UDTs on a member
named "key", pass a UDT member name to sort on, pass a member offset, etc.):

```vb
TYPE Player
    nam As String
    val As Long
END TYPE

Dim players(10) As Player
Dim scores(10) As Long

' Fill up arrays with data

SortByFirstMember _MemArray(players()) ' Sorts by 'nam' member
SortByFirstMember _MemArray(scores()) ' Stores by Long value


SUB SortByFirstMember (arr AS _MEM)
    DIM et AS _TypeInfo, kt AS _TypeInfo
    DIM n AS LONG, i AS LONG, j AS LONG
    DIM lo AS _MEM, hi AS _MEM

    et = _TypeArrayElementType(_MemTypeOf(arr))
    IF _TypeUdtMemberCount(et) < 1 THEN ERROR 5
    kt = _TypeUdtMemberType(et, 1)

    n = _MemArrayCount(arr)
    FOR i = 0 TO n - 2
        FOR j = 0 TO n - 2 - i
            lo = _MemArrayElement(arr, j)
            hi = _MemArrayElement(arr, j + 1)

            IF KeyAfter%(lo, hi, kt) THEN _MemArraySwap arr, j, j + 1
        NEXT
    NEXT
END SUB

FUNCTION KeyAfter% (x AS _MEM, y AS _MEM, kt AS _TypeInfo)
    DIM kx AS _MEM, ky AS _MEM
    DIM nx AS LONG, ny AS LONG
    DIM sx AS STRING, sy AS STRING
    kx = _MemUdtMember(x, 1)
    ky = _MemUdtMember(y, 1)
    SELECT CASE kt.KIND
        CASE _TYPE_PRIMITIVE
            IF kt.SIZE <> 4 THEN ERROR 5
            _MEMGET kx, kx.OFFSET, nx
            _MEMGET ky, ky.OFFSET, ny
            KeyAfter% = (nx > ny)
        CASE _TYPE_FIXEDSTRING
            sx = SPACE$(kt.SIZE): _MEMGET kx, kx.OFFSET, sx
            sy = SPACE$(kt.SIZE): _MEMGET ky, ky.OFFSET, sy
            KeyAfter% = (sx > sy)
        CASE ELSE
            ERROR 5
    END SELECT
END FUNCTION
```

A simple INI serializer via two-level nested TYPEs (supporting String's, Long's and arrays):

```vb
$Unstable:TypeFields
TYPE Server
    host AS STRING
    port AS LONG
    tags(0 TO 0) _Dynamic AS STRING
END TYPE
TYPE Config
    primary AS Server
    backup AS Server
END TYPE

Dim C As Config

ReDim C.primary.tags(0 TO 1) As String

C.primary.host = "example.com": C.primary.port = 80
C.Primary.tags(0) = "up": C.primary.tags(1) = "tag2"

C.backup.host = "second.example.com": C.backup.port = 8080
C.backup.tags(0) = "down"

Print IniOf$(_Mem(C))

' Produces this:

' [primary]
' host=example.com
' port=80
' tags=up,tag2
' [backup]
' host=second.example.com
' port=8080
' tags=down

' Adding extra members to the TYPEs automatically produces them in the INI string

' Each member of the outer TYPE is a section, each member of a section a key.
FUNCTION IniOf$ (m AS _MEM)
    DIM t AS _TypeInfo
    DIM i AS LONG
    DIM buf AS STRING
    t = _MemTypeOf(m)
    FOR i = 1 TO _TypeUdtMemberCount(t)
        buf = buf + "[" + _TypeUdtMemberName$(t, i) + "]" + CHR$(10)
        buf = buf + SectionOf$(_MemUdtMember(m, i))
    NEXT
    IniOf$ = buf
END FUNCTION

FUNCTION SectionOf$ (sec AS _MEM)
    DIM t AS _TypeInfo
    DIM i AS LONG
    DIM buf AS STRING
    t = _MemTypeOf(sec)
    FOR i = 1 TO _TypeUdtMemberCount(t)
        buf = buf + _TypeUdtMemberName$(t, i) + "=" + _
              ValueOf$(_MemUdtMember(sec, i), _TypeUdtMemberType(t, i)) + CHR$(10)
    NEXT
    SectionOf$ = buf
END FUNCTION

FUNCTION ValueOf$ (v AS _MEM, t AS _TypeInfo)
    DIM sv AS _MemBound STRING
    DIM n AS LONG, k AS LONG
    DIM buf AS STRING
    DIM et AS _TypeInfo
    SELECT CASE t.KIND
        CASE _TYPE_STRING
            _MemBind sv, v
            ValueOf$ = sv
            _MemUnbind sv
        CASE _TYPE_PRIMITIVE
            IF kt.SIZE <> 4 THEN ERROR 5
            _MEMGET v, v.OFFSET, n ' This assumes 'Long', but it could handle other integer types if wanted
            ValueOf$ = LTRIM$(STR$(n))
        CASE _TYPE_ARRAY
            et = _TypeArrayElementType(t)
            FOR k = 0 TO _MemArrayCount(v) - 1
                IF k > 0 THEN buf = buf + ","
                buf = buf + ValueOf$(_MemArrayElement(v, k), et)
            NEXT
            ValueOf$ = buf
    END SELECT
END FUNCTION
```

A simple list data structure with add/next, using `_MemContainer` to get
`_Mem`'s to the ListNode back to the TYPEs holding it:

```vb
TYPE ListNode
    nxt AS _MEM
END TYPE
TYPE Item
    name AS STRING
    link AS ListNode
END TYPE

DIM items(1 TO 3) AS Item
items(1).name = "alice"
items(2).name = "bob"
items(3).name = "carol"

DIM head AS _MEM, cur AS _MEM
DIM i AS LONG
DIM it AS _MemBound Item

FOR i = 1 TO 3
    ListAdd head, _MEM(items(i).link)      ' the list holds nodes, not Items
NEXT

cur = head
DO WHILE _MemExists(cur)
    _MemBind it, _MemContainer(cur, Item)  ' from the node back out to the Item
    PRINT it.name                          ' carol, bob, alice - adds push to the front
    _MemUnbind it
    ListNext cur, cur
LOOP

' These only know about ListNode, they rely on callers using _MemContainer to
' turn the ListNode back into the TYPE they are stored in.
SUB ListAdd (head AS _MEM, node AS _MEM)
    DIM n AS _MemBound(node) ListNode
    n.nxt = head
    head = node
END SUB

SUB ListNext (node AS _MEM, nxt AS _MEM)
    DIM n AS _MemBound(node) ListNode
    nxt = n.nxt
END SUB
```
