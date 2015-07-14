//: A simple memory allocator to create space for new variables at runtime.

:(scenarios run)
:(scenario new)
# call new two times with identical arguments; you should get back different results
recipe main [
  1:address:number/raw <- new number:type
  2:address:number/raw <- new number:type
  3:boolean/raw <- equal 1:address:number/raw, 2:address:number/raw
]
+mem: storing 0 in location 3

:(before "End Globals")
long long int Reserved_for_tests = 1000;
long long int Memory_allocated_until = Reserved_for_tests;
long long int Initial_memory_per_routine = 100000;
:(before "End Setup")
Memory_allocated_until = Reserved_for_tests;
Initial_memory_per_routine = 100000;
:(before "End routine Fields")
long long int alloc, alloc_max;
:(before "End routine Constructor")
alloc = Memory_allocated_until;
Memory_allocated_until += Initial_memory_per_routine;
alloc_max = Memory_allocated_until;
trace(Primitive_recipe_depth, "new") << "routine allocated memory from " << alloc << " to " << alloc_max;

//:: First handle 'type' operands.

:(before "End Mu Types Initialization")
Type_ordinal["type"] = 0;
:(after "Per-recipe Transforms")
// replace type names with type_ordinals
if (inst.operation == Recipe_ordinal["new"]) {
  // End NEW Transform Special-cases
  // first arg must be of type 'type'
  assert(SIZE(inst.ingredients) >= 1);
  if (!is_literal(inst.ingredients.at(0)))
    raise << "expected literal, got " << inst.ingredients.at(0).to_string() << '\n' << die();
  if (inst.ingredients.at(0).properties.at(0).second.at(0) != "type")
    raise << "tried to allocate non-type " << inst.ingredients.at(0).to_string() << " in recipe " << Recipe[r].name << '\n' << die();
  if (Type_ordinal.find(inst.ingredients.at(0).name) == Type_ordinal.end())
    raise << "unknown type " << inst.ingredients.at(0).name << " in recipe " << Recipe[r].name << '\n' << die();
//?   cerr << "type " << inst.ingredients.at(0).name << " => " << Type_ordinal[inst.ingredients.at(0).name] << '\n'; //? 1
  inst.ingredients.at(0).set_value(Type_ordinal[inst.ingredients.at(0).name]);
  trace(Primitive_recipe_depth, "new") << inst.ingredients.at(0).name << " -> " << inst.ingredients.at(0).name;
  end_new_transform:;
}

//:: Now implement the primitive recipe.
//: todo: build 'new' in mu itself

:(before "End Primitive Recipe Declarations")
NEW,
:(before "End Primitive Recipe Numbers")
Recipe_ordinal["new"] = NEW;
:(before "End Primitive Recipe Implementations")
case NEW: {
  // compute the space we need
  long long int size = 0;
  long long int array_length = 0;
  {
    vector<type_ordinal> type;
    assert(is_literal(current_instruction().ingredients.at(0)));
    type.push_back(current_instruction().ingredients.at(0).value);
//?     trace(Primitive_recipe_depth, "mem") << "type " << current_instruction().ingredients.at(0).to_string() << ' ' << type.size() << ' ' << type.back() << " has size " << size_of(type); //? 1
    if (SIZE(current_instruction().ingredients) > 1) {
      // array
      array_length = ingredients.at(1).at(0);
      trace(Primitive_recipe_depth, "mem") << "array size is " << array_length;
      size = array_length*size_of(type) + /*space for length*/1;
    }
    else {
      // scalar
      size = size_of(type);
    }
  }
  // compute the region of memory to return
  // really crappy at the moment
  ensure_space(size);
  const long long int result = Current_routine->alloc;
  trace(Primitive_recipe_depth, "mem") << "new alloc: " << result;
//?   trace(Primitive_recipe_depth, "mem") << "size: " << size << " locations"; //? 1
  // save result
  products.resize(1);
  products.at(0).push_back(result);
  // initialize allocated space
  for (long long int address = result; address < result+size; ++address) {
    Memory[address] = 0;
  }
  if (SIZE(current_instruction().ingredients) > 1) {
    Memory[result] = array_length;
  }
  // bump
  Current_routine->alloc += size;
  // no support for reclaiming memory
  assert(Current_routine->alloc <= Current_routine->alloc_max);
  break;
}

:(code)
void ensure_space(long long int size) {
  assert(size <= Initial_memory_per_routine);
//?   cout << Current_routine->alloc << " " << Current_routine->alloc_max << " " << size << '\n'; //? 1
  if (Current_routine->alloc + size > Current_routine->alloc_max) {
    // waste the remaining space and create a new chunk
    Current_routine->alloc = Memory_allocated_until;
    Memory_allocated_until += Initial_memory_per_routine;
    Current_routine->alloc_max = Memory_allocated_until;
    trace(Primitive_recipe_depth, "new") << "routine allocated memory from " << Current_routine->alloc << " to " << Current_routine->alloc_max;
  }
}

:(scenario new_initializes)
% Memory_allocated_until = 10;
% Memory[Memory_allocated_until] = 1;
recipe main [
  1:address:number <- new number:type
  2:number <- copy 1:address:number/deref
]
+mem: storing 0 in location 2

:(scenario new_array)
recipe main [
  1:address:array:number/raw <- new number:type, 5:literal
  2:address:number/raw <- new number:type
  3:number/raw <- subtract 2:address:number/raw, 1:address:array:number/raw
]
+run: 1:address:array:number/raw <- new number:type, 5:literal
+mem: array size is 5
# don't forget the extra location for array size
+mem: storing 6 in location 3

:(scenario new_empty_array)
recipe main [
  1:address:array:number/raw <- new number:type, 0:literal
  2:address:number/raw <- new number:type
  3:number/raw <- subtract 2:address:number/raw, 1:address:array:number/raw
]
+run: 1:address:array:number/raw <- new number:type, 0:literal
+mem: array size is 0
+mem: storing 1 in location 3

//: Make sure that each routine gets a different alloc to start.
:(scenario new_concurrent)
recipe f1 [
  start-running f2:recipe
  1:address:number/raw <- new number:type
  # wait for f2 to complete
  {
    loop-unless 4:number/raw
  }
]
recipe f2 [
  2:address:number/raw <- new number:type
  # hack: assumes scheduler implementation
  3:boolean/raw <- equal 1:address:number/raw, 2:address:number/raw
  # signal f2 complete
  4:number/raw <- copy 1:literal
]
+mem: storing 0 in location 3

//: If a routine runs out of its initial allocation, it should allocate more.
:(scenario new_overflow)
% Initial_memory_per_routine = 2;
recipe main [
  1:address:number/raw <- new number:type
  2:address:point/raw <- new point:type  # not enough room in initial page
]
+new: routine allocated memory from 1000 to 1002
+new: routine allocated memory from 1002 to 1004

//:: Next, extend 'new' to handle a unicode string literal argument.

:(scenario new_string)
recipe main [
  1:address:array:character <- new [abc def]
  2:character <- index 1:address:array:character/deref, 5:literal
]
# number code for 'e'
+mem: storing 101 in location 2

:(scenario new_string_handles_unicode)
recipe main [
  1:address:array:character <- new [a«c]
  2:number <- length 1:address:array:character/deref
  3:character <- index 1:address:array:character/deref, 1:literal
]
+mem: storing 3 in location 2
# unicode for '«'
+mem: storing 171 in location 3

:(before "End NEW Transform Special-cases")
  if (!inst.ingredients.empty()
      && !inst.ingredients.at(0).properties.empty()
      && !inst.ingredients.at(0).properties.at(0).second.empty()
      && inst.ingredients.at(0).properties.at(0).second.at(0) == "literal-string") {
    // skip transform
    inst.ingredients.at(0).initialized = true;
    goto end_new_transform;
  }

:(after "case NEW" following "Primitive Recipe Implementations")
if (is_literal(current_instruction().ingredients.at(0))
    && current_instruction().ingredients.at(0).properties.at(0).second.at(0) == "literal-string") {
  products.resize(1);
  products.at(0).push_back(new_string(current_instruction().ingredients.at(0).name));
  break;
}

:(code)
long long int new_string(const string& contents) {
  // allocate an array just large enough for it
  long long int string_length = unicode_length(contents);
//?   cout << "string_length is " << string_length << '\n'; //? 1
  ensure_space(string_length+1);  // don't forget the extra location for array size
  // initialize string
//?   cout << "new string literal: " << current_instruction().ingredients.at(0).name << '\n'; //? 1
  long long int result = Current_routine->alloc;
  Memory[Current_routine->alloc++] = string_length;
  long long int curr = 0;
  const char* raw_contents = contents.c_str();
  for (long long int i = 0; i < string_length; ++i) {
    uint32_t curr_character;
    assert(curr < SIZE(contents));
    tb_utf8_char_to_unicode(&curr_character, &raw_contents[curr]);
    Memory[Current_routine->alloc] = curr_character;
    curr += tb_utf8_char_length(raw_contents[curr]);
    ++Current_routine->alloc;
  }
  // mu strings are not null-terminated in memory
  return result;
}

//: Allocate more to routine when initializing a literal string
:(scenario new_string_overflow)
% Initial_memory_per_routine = 2;
recipe main [
  1:address:number/raw <- new number:type
  2:address:array:character/raw <- new [a]  # not enough room in initial page, if you take the array size into account
]
+new: routine allocated memory from 1000 to 1002
+new: routine allocated memory from 1002 to 1004

//: helpers
:(code)
long long int unicode_length(const string& s) {
  const char* in = s.c_str();
  long long int result = 0;
  long long int curr = 0;
  while (curr < SIZE(s)) {  // carefully bounds-check on the string
    // before accessing its raw pointer
    ++result;
    curr += tb_utf8_char_length(in[curr]);
  }
  return result;
}