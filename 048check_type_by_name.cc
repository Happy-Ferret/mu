//: Some simple sanity checks for types, and also attempts to guess them where
//: they aren't provided.
//:
//: You still have to provide the full type the first time you mention a
//: variable in a recipe. You have to explicitly name :offset and :variant
//: every single time. You can't use the same name with multiple types in a
//: single recipe.

:(scenario transform_fails_on_reusing_name_with_different_type)
% Hide_errors = true;
recipe main [
  x:number <- copy 1
  x:boolean <- copy 1
]
+error: main: x used with multiple types

:(after "int main")
  Transform.push_back(check_types_by_name);

:(code)
void check_types_by_name(const recipe_ordinal r) {
  map<string, type_tree*> metadata;
  for (long long int i = 0; i < SIZE(Recipe[r].steps); ++i) {
    instruction& inst = Recipe[r].steps.at(i);
    for (long long int in = 0; in < SIZE(inst.ingredients); ++in) {
      deduce_missing_type(metadata, inst.ingredients.at(in));
      check_metadata(metadata, inst.ingredients.at(in), r);
    }
    for (long long int out = 0; out < SIZE(inst.products); ++out) {
      deduce_missing_type(metadata, inst.products.at(out));
      check_metadata(metadata, inst.products.at(out), r);
    }
  }
}

void check_metadata(map<string, type_tree*>& metadata, const reagent& x, const recipe_ordinal r) {
  if (is_literal(x)) return;
  if (is_raw(x)) return;
  // if you use raw locations you're probably doing something unsafe
  if (is_integer(x.name)) return;
  if (!x.type) return;  // will throw a more precise error elsewhere
  if (metadata.find(x.name) == metadata.end())
    metadata[x.name] = x.type;
  if (!types_match(metadata[x.name], x.type))
    raise_error << maybe(Recipe[r].name) << x.name << " used with multiple types\n" << end();
}

:(scenario transform_fills_in_missing_types)
recipe main [
  x:number <- copy 1
  y:number <- add x, 1
]

:(code)
void deduce_missing_type(map<string, type_tree*>& metadata, reagent& x) {
  if (x.type) return;
  if (metadata.find(x.name) == metadata.end()) return;
  x.type = new type_tree(*metadata[x.name]);
  assert(!x.properties.at(0).second);
  x.properties.at(0).second = new string_tree("as-before");
}

:(scenario transform_fills_in_missing_types_in_product)
recipe main [
  x:number <- copy 1
  x <- copy 2
]

:(scenario transform_fills_in_missing_types_in_product_and_ingredient)
recipe main [
  x:number <- copy 1
  x <- add x, 1
]
+mem: storing 2 in location 1

:(scenario transform_fails_on_missing_types_in_first_mention)
% Hide_errors = true;
recipe main [
  x <- copy 1
  x:number <- copy 2
]
+error: main: missing type for x in 'x <- copy 1'

:(scenario typo_in_address_type_fails)
% Hide_errors = true;
recipe main [
  y:address:charcter <- new character:type
  *y <- copy 67
]
+error: main: unknown type in 'y:address:charcter <- new character:type'