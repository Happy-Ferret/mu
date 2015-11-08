//: Transform to maintain multiple variants of a recipe depending on the
//: number and types of the ingredients and products. Allows us to use nice
//: names like 'print' or 'length' in many mutually extensible ways.

:(scenario static_dispatch)
recipe main [
  7:number/raw <- test 3
]
recipe test a:number -> z:number [
  z <- copy 1
]
recipe test a:number, b:number -> z:number [
  z <- copy 2
]
+mem: storing 1 in location 7

//: When loading recipes, accumulate variants if headers don't collide, and
//: raise a warning if headers collide.

:(before "End Globals")
map<string, vector<recipe_ordinal> > Recipe_variants;
:(before "End One-time Setup")
put(Recipe_variants, "main", vector<recipe_ordinal>());  // since we manually added main to Recipe_ordinal
:(before "End Setup")
for (map<string, vector<recipe_ordinal> >::iterator p = Recipe_variants.begin(); p != Recipe_variants.end(); ++p) {
  for (long long int i = 0; i < SIZE(p->second); ++i) {
    if (p->second.at(i) >= Reserved_for_tests)
      p->second.at(i) = -1;  // just leave a ghost
  }
}

:(before "End Load Recipe Header(result)")
if (contains_key(Recipe_ordinal, result.name)) {
  const recipe_ordinal r = get(Recipe_ordinal, result.name);
  if ((!contains_key(Recipe, r) || get(Recipe, r).has_header)
      && !header_already_exists(result)) {
    string new_name = next_unused_recipe_name(result.name);
    put(Recipe_ordinal, new_name, Next_recipe_ordinal++);
    get(Recipe_variants, result.name).push_back(get(Recipe_ordinal, new_name));
    result.name = new_name;
  }
}
else {
  // save first variant
  put(Recipe_ordinal, result.name, Next_recipe_ordinal++);
  get_or_insert(Recipe_variants, result.name).push_back(get(Recipe_ordinal, result.name));
}

:(code)
bool header_already_exists(const recipe& rr) {
  const vector<recipe_ordinal>& variants = get(Recipe_variants, rr.name);
  for (long long int i = 0; i < SIZE(variants); ++i) {
    if (Recipe.find(variants.at(i)) != Recipe.end()
        && all_reagents_match(rr, get(Recipe, variants.at(i)))) {
      return true;
    }
  }
  return false;
}

bool all_reagents_match(const recipe& r1, const recipe& r2) {
  if (SIZE(r1.ingredients) != SIZE(r2.ingredients)) return false;
  if (SIZE(r1.products) != SIZE(r2.products)) return false;
  for (long long int i = 0; i < SIZE(r1.ingredients); ++i) {
    if (!exact_match(r1.ingredients.at(i).type, r2.ingredients.at(i).type))
      return false;
  }
  for (long long int i = 0; i < SIZE(r1.products); ++i) {
    if (!exact_match(r1.products.at(i).type, r2.products.at(i).type))
      return false;
  }
  return true;
}

bool exact_match(type_tree* a, type_tree* b) {
  if (a == b) return true;
  return a->value == b->value
      && exact_match(a->left, b->left)
      && exact_match(a->right, b->right);
}

string next_unused_recipe_name(const string& recipe_name) {
  for (long long int i = 2; ; ++i) {
    ostringstream out;
    out << recipe_name << '_' << i;
    if (Recipe_ordinal.find(out.str()) == Recipe_ordinal.end()) {
      return out.str();
    }
  }
}

//: Once all the recipes are loaded, transform their bodies to replace each
//: call with the most suitable variant.

:(scenario static_dispatch_picks_most_similar_variant)
recipe main [
  7:number/raw <- test 3, 4, 5
]
recipe test a:number -> z:number [
  z <- copy 1
]
recipe test a:number, b:number -> z:number [
  z <- copy 2
]
+mem: storing 2 in location 7

//: after insert_fragments (tangle) and before computing operation ids
:(before "Transform.push_back(deduce_types_from_header)")
Transform.push_back(resolve_ambiguous_calls);  // idempotent

:(code)
void resolve_ambiguous_calls(recipe_ordinal r) {
  if (!get(Recipe, r).has_header) return;
  trace(9991, "transform") << "--- resolve ambiguous calls for recipe " << get(Recipe, r).name << end();
//?   cerr << "--- resolve ambiguous calls for recipe " << get(Recipe, r).name << '\n';
  for (long long int index = 0; index < SIZE(get(Recipe, r).steps); ++index) {
    instruction& inst = get(Recipe, r).steps.at(index);
    if (inst.is_label) continue;
    if (!contains_key(Recipe_variants, inst.name)) continue;
    assert(!get(Recipe_variants, inst.name).empty());
    replace_best_variant(inst);
  }
}

void replace_best_variant(instruction& inst) {
  trace(9992, "transform") << "instruction " << inst.name << end();
  vector<recipe_ordinal>& variants = get(Recipe_variants, inst.name);
  long long int best_score = variant_score(inst, get(Recipe_ordinal, inst.name));
  for (long long int i = 0; i < SIZE(variants); ++i) {
    long long int current_score = variant_score(inst, variants.at(i));
    trace(9992, "transform") << "checking variant " << i << ": " << current_score << end();
    if (current_score > best_score) {
      inst.name = get(Recipe, variants.at(i)).name;
      best_score = current_score;
    }
  }
  // End Instruction Dispatch(inst, best_score)
}

long long int variant_score(const instruction& inst, recipe_ordinal variant) {
  if (variant == -1) return -1;  // ghost from a previous test
  const vector<reagent>& header_ingredients = get(Recipe, variant).ingredients;
  if (SIZE(inst.ingredients) < SIZE(header_ingredients)) {
    trace(9993, "transform") << "too few ingredients" << end();
    return -1;
  }
  for (long long int i = 0; i < SIZE(header_ingredients); ++i) {
    if (!types_match(header_ingredients.at(i), inst.ingredients.at(i))) {
      trace(9993, "transform") << "mismatch: ingredient " << i << end();
      return -1;
    }
  }
  if (SIZE(inst.products) > SIZE(get(Recipe, variant).products)) {
    trace(9993, "transform") << "too few products" << end();
    return -1;
  }
  const vector<reagent>& header_products = get(Recipe, variant).products;
  for (long long int i = 0; i < SIZE(inst.products); ++i) {
    if (!types_match(header_products.at(i), inst.products.at(i))) {
      trace(9993, "transform") << "mismatch: product " << i << end();
      return -1;
    }
  }
  // the greater the number of unused ingredients, the lower the score
  return 100 - (SIZE(get(Recipe, variant).products)-SIZE(inst.products))
             - (SIZE(inst.ingredients)-SIZE(get(Recipe, variant).ingredients));  // ok to go negative
}

:(scenario static_dispatch_disabled_on_headerless_definition)
% Hide_warnings = true;
recipe test a:number -> z:number [
  z <- copy 1
]
recipe test [
  reply 34
]
+warn: redefining recipe test

:(scenario static_dispatch_disabled_on_headerless_definition_2)
% Hide_warnings = true;
recipe test [
  reply 34
]
recipe test a:number -> z:number [
  z <- copy 1
]
+warn: redefining recipe test