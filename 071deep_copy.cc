// To recursively copy containers and any addresses they contain, use
// 'deep-copy'.
//
// Invariant: After a deep-copy its ingredient and result will point to no
// common addresses.
//
// We do handle cycles in the ingredient, however. All cycles are translated
// to new cycles in the product.

:(scenario deep_copy_number)
def main [
  local-scope
  x:num <- copy 34
  y:num <- deep-copy x
  10:bool/raw <- equal x, y
]
# non-address primitives are identical
+mem: storing 1 in location 10

:(scenario deep_copy_null_pointer)
def main [
  local-scope
  x:&:num <- copy 0
  y:&:num <- deep-copy x
  10:bool/raw <- equal x, y
]
# null addresses are identical
+mem: storing 1 in location 10

:(scenario deep_copy_container_without_address)
container foo [
  x:num
  y:num
]
def main [
  local-scope
  a:foo <- merge 34, 35
  b:foo <- deep-copy a
  10:bool/raw <- equal a, b
]
# containers are identical as long as they don't contain addresses
+mem: storing 1 in location 10

:(scenario deep_copy_address)
% Memory_allocated_until = 200;
def main [
  # avoid all memory allocations except the implicit ones inside deep-copy, so
  # that the result is deterministic
  1:&:num <- copy 100/unsafe  # pretend allocation
  *1:&:num <- copy 34
  2:&:num <- deep-copy 1:&:num
  10:bool <- equal 1:&:num, 2:&:num
  11:bool <- equal *1:&:num, *2:&:num
  2:&:num <- copy 0
]
# the result of deep-copy is a new address
+mem: storing 0 in location 10
# however, the contents are identical
+mem: storing 1 in location 11

:(scenario deep_copy_address_to_container)
% Memory_allocated_until = 200;
def main [
  # avoid all memory allocations except the implicit ones inside deep-copy, so
  # that the result is deterministic
  1:&:point <- copy 100/unsafe  # pretend allocation
  *1:&:point <- merge 34, 35
  2:&:point <- deep-copy 1:&:point
  10:bool <- equal 1:&:point, 2:&:point
  11:bool <- equal *1:&:point, *2:&:point
]
# the result of deep-copy is a new address
+mem: storing 0 in location 10
# however, the contents are identical
+mem: storing 1 in location 11

:(scenario deep_copy_address_to_address)
% Memory_allocated_until = 200;
def main [
  # avoid all memory allocations except the implicit ones inside deep-copy, so
  # that the result is deterministic
  1:&:&:num <- copy 100/unsafe  # pretend allocation
  *1:&:&:num <- copy 150/unsafe
  **1:&:&:num <- copy 34
  2:&:&:num <- deep-copy 1:&:&:num
  10:bool <- equal 1:&:&:num, 2:&:&:num
  11:bool <- equal *1:&:&:num, *2:&:&:num
  12:bool <- equal **1:&:&:num, **2:&:&:num
]
# the result of deep-copy is a new address
+mem: storing 0 in location 10
# any addresses in it or pointed to it are also new
+mem: storing 0 in location 11
# however, the non-address contents are identical
+mem: storing 1 in location 12

:(scenario deep_copy_array)
% Memory_allocated_until = 200;
def main [
  # avoid all memory allocations except the implicit ones inside deep-copy, so
  # that the result is deterministic
  100:num <- copy 3  # pretend array length
  1:&:@:num <- copy 100/unsafe  # pretend allocation
  put-index *1:&:@:num, 0, 34
  put-index *1:&:@:num, 1, 35
  put-index *1:&:@:num, 2, 36
  stash [old:], *1:&:@:num
  2:&:@:num <- deep-copy 1:&:@:num
  stash 2:&:@:num
  stash [new:], *2:&:@:num
  10:bool <- equal 1:&:@:num, 2:&:@:num
  11:bool <- equal *1:&:@:num, *2:&:@:num
]
+app: old: 3 34 35 36
+app: new: 3 34 35 36
# the result of deep-copy is a new address
+mem: storing 0 in location 10
# however, the contents are identical
+mem: storing 1 in location 11

:(scenario deep_copy_container_with_address)
container foo [
  x:num
  y:&:num
]
def main [
  local-scope
  y0:&:num <- new number:type
  *y0 <- copy 35
  a:foo <- merge 34, y0
  b:foo <- deep-copy a
  10:bool/raw <- equal a, b
  y1:&:num <- get b, y:offset
  11:bool/raw <- equal y0, y1
  12:num/raw <- copy *y1
]
# containers containing addresses are not identical to their deep copies
+mem: storing 0 in location 10
# the addresses they contain are not identical either
+mem: storing 0 in location 11
+mem: storing 35 in location 12

:(scenario deep_copy_exclusive_container_with_address)
exclusive-container foo [
  x:num
  y:&:num
]
def main [
  local-scope
  y0:&:num <- new number:type
  *y0 <- copy 34
  a:foo <- merge 1/y, y0
  b:foo <- deep-copy a
  10:bool/raw <- equal a, b
  y1:&:num, z:bool <- maybe-convert b, y:variant
  11:bool/raw <- equal y0, y1
  12:num/raw <- copy *y1
]
# exclusive containers containing addresses are not identical to their deep copies
+mem: storing 0 in location 10
# the addresses they contain are not identical either
+mem: storing 0 in location 11
+mem: storing 34 in location 12

:(scenario deep_copy_exclusive_container_with_container_with_address)
exclusive-container foo [
  x:num
  y:bar  # inline
]
container bar [
  x:&:num
]
def main [
  local-scope
  y0:&:num <- new number:type
  *y0 <- copy 34
  a:bar <- merge y0
  b:foo <- merge 1/y, a
  c:foo <- deep-copy b
  10:bool/raw <- equal b, c
  d:bar, z:bool <- maybe-convert c, y:variant
  y1:&:num <- get d, x:offset
  11:bool/raw <- equal y0, y1
  12:num/raw <- copy *y1
]
# exclusive containers containing addresses are not identical to their deep copies
+mem: storing 0 in location 10
# sub-containers containing addresses are not identical either
+mem: storing 0 in location 11
+mem: storing 34 in location 12

:(before "End Primitive Recipe Declarations")
DEEP_COPY,
:(before "End Primitive Recipe Numbers")
put(Recipe_ordinal, "deep-copy", DEEP_COPY);
:(before "End Primitive Recipe Checks")
case DEEP_COPY: {
  if (SIZE(inst.ingredients) != 1) {
    raise << maybe(get(Recipe, r).name) << "'deep-copy' takes exactly one ingredient rather than '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (SIZE(inst.products) != 1) {
    raise << maybe(get(Recipe, r).name) << "'deep-copy' takes exactly one ingredient rather than '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  if (!types_strictly_match(inst.ingredients.at(0), inst.products.at(0))) {
    raise << maybe(get(Recipe, r).name) << "'deep-copy' requires its ingredient and product to be the same type, but got '" << to_original_string(inst) << "'\n" << end();
    break;
  }
  break;
}
:(before "End Primitive Recipe Implementations")
case DEEP_COPY: {
  products.push_back(deep_copy(current_instruction().ingredients.at(0)));
  break;
}

:(code)
vector<double> deep_copy(const reagent& in) {
  // allocate a tiny bit of temporary space for deep_copy()
  trace(9991, "run") << "deep-copy: allocating space for temporary" << end();
  reagent tmp("tmp:address:number");
  tmp.set_value(allocate(1));
  map<int, int> addresses_copied;
  vector<double> result = deep_copy(in, addresses_copied, tmp);
  // reclaim Mu memory allocated for tmp
  trace(9991, "run") << "deep-copy: reclaiming temporary" << end();
  abandon(tmp.value, payload_size(tmp));
  // reclaim host memory allocated for tmp.type when tmp goes out of scope
  return result;
}

vector<double> deep_copy(reagent/*copy*/ in, map<int, int>& addresses_copied, const reagent& tmp) {
  canonize(in);
  if (is_mu_address(in)) {
    // hack: skip primitive containers that do their own locking; they're designed to be shared between routines
    type_tree* payload = in.type->right;
    if (payload->left->name == "channel" || payload->left->name == "screen" || payload->left->name == "console" || payload->left->name == "resources")
      return read_memory(in);
  }
  vector<double> result;
  if (is_mu_address(in))
    result.push_back(deep_copy_address(in, addresses_copied, tmp));
  else
    deep_copy(in, addresses_copied, tmp, result);
  return result;
}

// deep-copy an address and return a new address
int deep_copy_address(const reagent& canonized_in, map<int, int>& addresses_copied, const reagent& tmp) {
  if (address_value(canonized_in) == 0) return 0;
  int in_address = payload_address(canonized_in);
  trace(9991, "run") << "deep-copy: copying address " << in_address << end();
  if (contains_key(addresses_copied, in_address)) {
    int out = get(addresses_copied, in_address);
    trace(9991, "run") << "deep-copy: copy already exists: " << out << end();
    return out;
  }
  int out = allocate(payload_size(canonized_in));
  trace(9991, "run") << "deep-copy: new address is " << out << end();
  put(addresses_copied, in_address, out);
  reagent/*copy*/ payload = canonized_in;
  payload.properties.push_back(pair<string, string_tree*>("lookup", NULL));
  trace(9991, "run") << "recursing on payload " << payload.value << ' ' << to_string(payload) << end();
  vector<double> data = deep_copy(payload, addresses_copied, tmp);
  trace(9991, "run") << "deep-copy: writing result " << out << ": " << to_string(data) << end();
  // HACK: write_memory interface isn't ideal for this situation; we need
  // a temporary location to help copy the payload.
  trace(9991, "run") << "deep-copy: writing temporary " << tmp.value << ": " << out << end();
  put(Memory, tmp.value, out);
  payload.set_value(tmp.value);  // now modified for output
  vector<double> old_data = read_memory(payload);
  trace(9991, "run") << "deep-copy: really writing to " << payload.value << ' ' << to_string(payload) << " (old value " << to_string(old_data) << " new value " << to_string(data) << ")" << end();
  write_memory(payload, data);
  trace(9991, "run") << "deep-copy: output is " << to_string(data) << end();
  return out;
}

// deep-copy a non-address and return a vector of locations
void deep_copy(const reagent& canonized_in, map<int, int>& addresses_copied, const reagent& tmp, vector<double>& out) {
  assert(!is_mu_address(canonized_in));
  vector<double> data = read_memory(canonized_in);
  out.insert(out.end(), data.begin(), data.end());
  if (!contains_key(Container_metadata, canonized_in.type)) return;
  trace(9991, "run") << "deep-copy: scanning for addresses in " << to_string(data) << end();
  const container_metadata& metadata = get(Container_metadata, canonized_in.type);
  for (map<set<tag_condition_info>, set<address_element_info> >::const_iterator p = metadata.address.begin();  p != metadata.address.end();  ++p) {
    if (!all_match(data, p->first)) continue;
    for (set<address_element_info>::const_iterator info = p->second.begin();  info != p->second.end();  ++info) {
      // hack: skip primitive containers that do their own locking; they're designed to be shared between routines
      if (!info->payload_type->atom && info->payload_type->left->name == "channel")
        continue;
      if (info->payload_type->name == "screen" || info->payload_type->name == "console" || info->payload_type->name == "resources")
        continue;
      // construct a fake reagent that reads directly from the appropriate
      // field of the container
      reagent curr;
      if (info->payload_type->atom)
        curr.type = new type_tree(new type_tree("address"), new type_tree(new type_tree(info->payload_type->name), NULL));
      else
        curr.type = new type_tree(new type_tree("address"), new type_tree(*info->payload_type));
      curr.set_value(canonized_in.value + info->offset);
      curr.properties.push_back(pair<string, string_tree*>("raw", NULL));
      trace(9991, "run") << "deep-copy: copying address " << curr.value << end();
      out.at(info->offset) = deep_copy_address(curr, addresses_copied, tmp);
    }
  }
}

int payload_address(reagent/*copy*/ x) {
  x.properties.push_back(pair<string, string_tree*>("lookup", NULL));
  canonize(x);
  return x.value;
}

int address_value(const reagent& x) {
  assert(is_mu_address(x));
  vector<double> result = read_memory(x);
  assert(SIZE(result) == 1);
  return static_cast<int>(result.at(0));
}

:(scenario deep_copy_stress_test_1)
container foo1 [
  p:&:num
]
container foo2 [
  p:&:foo1
]
exclusive-container foo3 [
  p:&:foo1
  q:&:foo2
]
def main [
  local-scope
  x:&:num <- new number:type
  *x <- copy 34
  a:&:foo1 <- new foo1:type
  *a <- merge x
  b:&:foo2 <- new foo2:type
  *b <- merge a
  c:foo3 <- merge 1/q, b
  d:foo3 <- deep-copy c
  e:&:foo2, z:bool <- maybe-convert d, q:variant
  f:&:foo1 <- get *e, p:offset
  g:&:num <- get *f, p:offset
  1:num/raw <- copy *g
]
+mem: storing 34 in location 1

:(scenario deep_copy_stress_test_2)
container foo1 [
  p:&:num
]
container foo2 [
  p:&:foo1
]
exclusive-container foo3 [
  p:&:foo1
  q:&:foo2
]
container foo4 [
  p:num
  q:&:foo3
]
def main [
  local-scope
  x:&:num <- new number:type
  *x <- copy 34
  a:&:foo1 <- new foo1:type
  *a <- merge x
  b:&:foo2 <- new foo2:type
  *b <- merge a
  c:&:foo3 <- new foo3:type
  *c <- merge 1/q, b
  d:foo4 <- merge 35, c
  e:foo4 <- deep-copy d
  f:&:foo3 <- get e, q:offset
  g:&:foo2, z:bool <- maybe-convert *f, q:variant
  h:&:foo1 <- get *g, p:offset
  y:&:num <- get *h, p:offset
  1:num/raw <- copy *y
]
+mem: storing 34 in location 1

:(scenario deep_copy_cycles)
container foo [
  p:num
  q:&:foo
]
def main [
  local-scope
  x:&:foo <- new foo:type
  *x <- put *x, p:offset, 34
  *x <- put *x, q:offset, x  # create a cycle
  y:&:foo <- deep-copy x
  1:num/raw <- get *y, p:offset
  y2:&:foo <- get *y, q:offset
  2:bool/raw <- equal y, y2  # is it still a cycle?
  3:bool/raw <- equal x, y  # is it the same cycle?
  # not bothering cleaning up; both cycles leak memory
]
+mem: storing 34 in location 1
# deep copy also contains a cycle
+mem: storing 1 in location 2
# but it's a completely different (disjoint) cycle
+mem: storing 0 in location 3

//: todo: version of deep_copy_cycles that breaks cycles at end and verifies no memory leaks
//: needs approximate matching over scenario traces

:(scenario deep_copy_skips_channel)
# ugly! dummy 'channel' type if we happen to be building without that layer
% if (!contains_key(Type_ordinal, "channel")) get_or_insert(Type, put(Type_ordinal, "channel", Next_type_ordinal++)).name = "channel";
def main [
  local-scope
  x:&:channel:num <- new {(channel num): type}
  y:&:channel:num <- deep-copy x
  10:bool/raw <- equal x, y
]
# channels are never deep-copied
+mem: storing 1 in location 10

:(scenario deep_copy_skips_nested_channel)
# ugly! dummy 'channel' type if we happen to be building without that layer
% if (!contains_key(Type_ordinal, "channel")) get_or_insert(Type, put(Type_ordinal, "channel", Next_type_ordinal++)).name = "channel";
container foo [
  c:&:channel:num
]
def main [
  local-scope
  x:&:foo <- new foo:type
  y:&:foo <- deep-copy x
  xc:&:channel:num <- get *x, c:offset
  yc:&:channel:num <- get *y, c:offset
  10:bool/raw <- equal xc, yc
]
# channels are never deep-copied
+mem: storing 1 in location 10

:(scenario deep_copy_skips_resources)
# ugly! dummy 'resources' type if we happen to be building without that layer
% if (!contains_key(Type_ordinal, "resources")) get_or_insert(Type, put(Type_ordinal, "resources", Next_type_ordinal++)).name = "resources";
def main [
  local-scope
  x:&:resources <- new resources:type
  y:&:resources <- deep-copy x
  10:bool/raw <- equal x, y
]
# resources are never deep-copied
+mem: storing 1 in location 10

:(scenario deep_copy_skips_nested_resources)
# ugly! dummy 'resources' type if we happen to be building without that layer
% if (!contains_key(Type_ordinal, "resources")) get_or_insert(Type, put(Type_ordinal, "resources", Next_type_ordinal++)).name = "resources";
container foo [
  c:&:resources
]
def main [
  local-scope
  x:&:foo <- new foo:type
  y:&:foo <- deep-copy x
  xc:&:resources <- get *x, c:offset
  yc:&:resources <- get *y, c:offset
  10:bool/raw <- equal xc, yc
]
# resources are never deep-copied
+mem: storing 1 in location 10
