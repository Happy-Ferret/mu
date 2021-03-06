//: Reclaiming memory when it's no longer used.

:(scenario new_reclaim)
def main [
  1:address:num <- new number:type
  2:num <- copy 1:address:num  # because 1 will get reset during abandon below
  abandon 1:address:num
  3:address:num <- new number:type  # must be same size as abandoned memory to reuse
  4:num <- copy 3:address:num
  5:bool <- equal 2:num, 4:num
]
# both allocations should have returned the same address
+mem: storing 1 in location 5

//: When abandoning addresses we'll save them to a 'free list', segregated by size.

:(before "End routine Fields")
map<int, int> free_list;

:(before "End Primitive Recipe Declarations")
ABANDON,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "abandon", ABANDON);
:(before "End Primitive Recipe Checks")
case ABANDON: {
  if (!inst.products.empty()) {
    raise << maybe(get(Recipe, r).name) << "'abandon' shouldn't write to any products in '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  for (int i = 0;  i < SIZE(inst.ingredients);  ++i) {
    if (!is_mu_address(inst.ingredients.at(i)))
      raise << maybe(get(Recipe, r).name) << "ingredients of 'abandon' should be addresses, but ingredient " << i << " is '" << to_string(inst.ingredients.at(i)) << '\n' << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case ABANDON: {
  for (int i = 0;  i < SIZE(current_instruction().ingredients);  ++i) {
    reagent/*copy*/ ingredient = current_instruction().ingredients.at(i);
    canonize(ingredient);
    abandon(get_or_insert(Memory, ingredient.value), payload_size(ingredient));
  }
  break;
}

:(code)
void abandon(int address, int payload_size) {
  // clear memory
  for (int curr = address;  curr < address+payload_size;  ++curr)
    put(Memory, curr, 0);
  // append existing free list to address
  trace("abandon") << "saving " << address << " in free-list of size " << payload_size << end();
  put(Memory, address, get_or_insert(Current_routine->free_list, payload_size));
  put(Current_routine->free_list, payload_size, address);
}

:(after "Allocate Special-cases")
if (get_or_insert(Current_routine->free_list, size)) {
  trace("abandon") << "picking up space from free-list of size " << size << end();
  int result = get_or_insert(Current_routine->free_list, size);
  trace("mem") << "new alloc from free list: " << result << end();
  put(Current_routine->free_list, size, get_or_insert(Memory, result));
  put(Memory, result, 0);
  for (int curr = result;  curr < result+size;  ++curr) {
    if (get_or_insert(Memory, curr) != 0) {
      raise << maybe(current_recipe_name()) << "memory in free list was not zeroed out: " << curr << '/' << result << "; somebody wrote to us after free!!!\n" << end();
      break;  // always fatal
    }
  }
  return result;
}

:(scenario new_differing_size_no_reclaim)
def main [
  1:address:num <- new number:type
  2:num <- copy 1:address:num
  abandon 1:address:num
  3:address:array:num <- new number:type, 2  # different size
  4:num <- copy 3:address:array:num
  5:bool <- equal 2:num, 4:num
]
# no reuse
+mem: storing 0 in location 5

:(scenario new_reclaim_array)
def main [
  1:address:array:num <- new number:type, 2
  2:num <- copy 1:address:array:num
  abandon 1:address:array:num
  3:address:array:num <- new number:type, 2  # same size
  4:num <- copy 3:address:array:num
  5:bool <- equal 2:num, 4:num
]
# both calls to new returned identical addresses
+mem: storing 1 in location 5
