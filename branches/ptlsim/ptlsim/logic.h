// -*- c++ -*-
//
// Sequential Logic Primitives for C++
//
// Copyright 2000-2004 Matt T. Yourst <yourst@yourst.com>
//

#ifndef _LOGIC_H_
#define _LOGIC_H_

#include <globals.h>
#include <superstl.h>

template <typename T> 
struct latch {
  T data;
  T newdata;

  latch() {
    reset();
  }

  void reset(const T& d = T()) {
    data = d;
    newdata = d;
  }

  latch(const T& t) { newdata = t; }

  operator T() const { return data; }

  T& operator =(const T& t) {
    newdata = t; return data;
  }

  void clock(bool clkenable = true) {
    if (clkenable)
      data = newdata;
  }
};

typedef W64 regdata_t;

template <int b>
class reg {
public:

  reg() { }

  reg(int r) { value = r; }

  // operator , // (concat bits, verilog style)

  reg(const reg<b>& r) { value = r.value; }

  operator regdata_t() const {
    return value;
  }
  /*
  template <int high, int low>
  reg<(high - low) + 1> bits() const {
    //return bits(value, low, (high - low)+1);
    return (value >> low);
  }
  */
  regdata_t operator ()(int high, int low) const {
    return (value >> low) & ((1 << ((high-low)+1)) - 1);
  }

  bool operator [](int i) const {
    return (value >> i) & 1;
  }

protected:
  regdata_t value:b;
};

template <int leftbits, int rightbits>
reg<leftbits + rightbits> operator ,(const reg<leftbits>& r1, const reg<rightbits>& r2) {
  return reg<leftbits + rightbits>(((regdata_t)r1 << rightbits) | (regdata_t)r2);
}

typedef reg<1> bit;

template <typename T, int size>
struct SynchronousRegisterFile {
  SynchronousRegisterFile() { 
    reset();
  }

  void reset() {
    for (int i = 0; i < size; i++) {
      data[i].data = 0;
      data[i].newdata = 0;
    }
  }

  latch<T> data[size];

  latch<T>& operator [](int i) { 
    return data[i]; 
  }

  void clock(bool clkenable = true) {
    if (!clkenable)
      return;

    for (int i = 0; i < size; i++) {
      data[i].clock();
    }
  }
};

//
// Queue
//

// Iterate forward through queue from head to tail 
#define foreach_forward(Q, i) for (int i = Q.head; i != Q.tail; i = add_index_modulo(i, +1, Q.size))

// Iterate forward through queue from the specified entry until the tail
#define foreach_forward_from(Q, E, i) for (int i = E->index(); i != Q.tail; i = add_index_modulo(i, +1, Q.size))

// Iterate forward through queue from the entry after the specified entry until the tail
#define foreach_forward_after(Q, E, i) for (int i = add_index_modulo(E->index(), +1, Q.size); i != Q.tail; i = add_index_modulo(i, +1, Q.size))

// Iterate backward through queue from tail to head
#define foreach_backward(Q, i) for (int i = add_index_modulo(Q.tail, -1, Q.size); i != add_index_modulo(Q.head, -1, Q.size); i = add_index_modulo(i, -1, Q.size))

// Iterate backward through queue from the specified entry until the tail
#define foreach_backward_from(Q, i) for (int i = E->index(); i != add_index_modulo(Q.head, -1, Q.size); i = add_index_modulo(i, -1, Q.size))

// Iterate backward through queue from the entry before the specified entry until the head
#define foreach_backward_before(Q, E, i) for (int i = add_index_modulo(E->index(), -1, Q.size); ((i != add_index_modulo(Q.head, -1, Q.size)) && (E->index() != Q.head)); i = add_index_modulo(i, -1, Q.size))

template <class T, int SIZE>
struct Queue: public array<T, SIZE> {
  int head; // used for allocation
  int tail; // used for deallocation
  int count; // count of entries

  static const int size = SIZE;

  Queue() {
    reset();
  }

  void reset() {
    head = tail = count = 0;
    foreach (i, SIZE) {
      (*this)[i].init();
    }
  }

  int remaining() const {
    return max((SIZE - count) - 1, 0);
  }

  bool empty() const {
    return (!count);
  }

  bool full() const {
    return (!remaining());
  }

  T* alloc() {
    if (!remaining())
      return null;

    T* entry = &(*this)[tail];
    entry->validate();

    tail = add_index_modulo(tail, +1, SIZE);
    count++;

    return entry;
  }

  T* push() {
    return alloc();
  }

  T* push(const T& data) {
    T* slot = push();
    if (!slot) return null;
    *slot = data;
    return slot;
  }

  void prepfree(T& entry);

  void commit(T& entry) {
    assert(entry.index() == head);
    prepfree(entry);
    count--;
    head = add_index_modulo(head, +1, SIZE);
  }

  void annul(T& entry) {
    assert(entry.index() == add_index_modulo(tail, -1, SIZE));
    prepfree(entry);
    count--;
    tail = add_index_modulo(tail, -1, SIZE);
  }

  T* pop() {
    if (empty()) return null;
    tail = add_index_modulo(tail, -1, SIZE);
    count--;
    return &(*this)[tail];
  }

  T* peek() {
    if (empty())
      return null;
    return &(*this)[head];
  }

  T* dequeue() {
    if (empty())
      return null;
    count--;
    T* entry = &(*this)[head];
    head = add_index_modulo(head, +1, SIZE);
    return entry;
  }

  void prepfree(T* entry) { prepfree(*entry); }
  void commit(T* entry) { commit(*entry); }
  void annul(T* entry) { annul(*entry); }

  void print(ostream& os) {
    int i;

    foreach_forward(*this, i) {
      (*this)[i].print(os);
    }
  }
};

template <class T, int size>
ostream& operator <<(ostream& os, Queue<T, size>& queue) {
  os << "Queue<", size, "]: head ", queue.head, " to tail ", queue.tail, " (", queue.count, " entries):", endl;
  foreach_forward(queue, i) {
    const T& entry = queue[i];
    os << "  ", entry, endl;
  }

  return os;
}

template <typename T, int size>
struct HistoryBuffer: public array<T, size> {
  int current;
  T prevoldest;

  void reset() {
    current = size-1;
    setzero(this->data);
  }

  HistoryBuffer() {
    reset();
  }

  //
  // Enqueue t at the tail of the queue, making the results
  // visible for possible dequeueing by an earlier pipeline
  // stage within the same cycle (i.e., forwarding is used).
  // If this is not desirable, use enqueuesync() instead.
  //
  void add(const T& t) {
    current = add_index_modulo(current, +1, size);
    prevoldest = this->data[current];
    this->data[current] = t;
  }

  /*
   * Undo last addition
   */ 
  void undo() {
    this->data[current] = prevoldest;
    current = add_index_modulo(current, -1, size);
  }

  /*
   * Index backwards in time: 0 = most recent addition
   */
  T& operator [](int index) {
    assert(index < size);
    return this->data[add_index_modulo(current, -index, size)];
  }
};

template <class T, int size>
ostream& operator <<(ostream& os, HistoryBuffer<T, size>& history) {
  os << "HistoryBuffer[", size, "]: current = ", history.current, ", prevoldest = ", history.prevoldest, endl;
  for (int i = 0; i < size; i++) {
    os << "  ", history[i], endl;
  }
  return os;
}

//
// Fully Associative Arrays
//

template <typename T> struct InvalidTag { static const T INVALID; };
template <> struct InvalidTag<W64> { static const W64 INVALID = 0xffffffffffffffffULL; };
template <> struct InvalidTag<W32> { static const W32 INVALID = 0xffffffff; };
template <> struct InvalidTag<W16> { static const W16 INVALID = 0xffff; };
template <> struct InvalidTag<W8> { static const W8 INVALID = 0xff; };

//
// The replacement policy is pseudo-LRU using a most recently used
// bit vector (mLRU), as described in the paper "Performance Evaluation
// of Cache Replacement Policies for the SPEC CPU2000 Benchmark Suite"
// by Al-Zoubi et al. Essentially we maintain one MRU bit per way and
// set the bit for the way when that way is accessed. The way to evict
// is the first way without its MRU bit set. If all MRU bits become
// set, they are all reset and we start over. Surprisingly, this
// simple method performs as good as, if not better than, true LRU
// or tree-based hot sector LRU.
//

template <typename T, int ways>
struct FullyAssociativeTags {
  bitvec<ways> evictmap;
  T tags[ways];

  static const T INVALID = InvalidTag<T>::INVALID;

  FullyAssociativeTags() {
    reset();
  }

  void reset() {
    evictmap = 0;
    foreach (i, ways) {
      tags[i] = INVALID;
    }
  }

  void use(int way) {
    evictmap[way] = 1;
    // Performance is somewhat better with this off with higher associativity caches:
    // if (evictmap.allset()) evictmap = 0;
  }

  //
  // This is a clever way of doing branch-free matching
  // with conditional moves and addition. It relies on
  // having at most one matching entry in the array;
  // otherwise the algorithm breaks:
  //
  int match(T target) {
    int way = 0;
    foreach (i, ways) {
      way += (tags[i] == target) ? (i + 1) : 0;
    }

    return way - 1;
  }

  int probe(T target) {
    int way = match(target);
    if (way < 0) return -1;

    use(way);
    return way;
  }

  int lru() const {
    return (evictmap.allset()) ? 0 : (~evictmap).lsb();
  }

  int select(T target, T& oldtag) {
    int way = probe(target);
    if (way < 0) {
      way = lru();
      if (evictmap.allset()) evictmap = 0;
      oldtag = tags[way];
      tags[way] = target;
    }
    use(way);
    return way;
  }

  int select(T target) {
    T dummy;
    return select(target, dummy);
  }

  void invalidate_way(int way) {
    tags[way] = INVALID;
    evictmap[way] = 0;
  }

  int invalidate(T target) {
    int way = probe(target);
    if (way < 0) return -1;
    invalidate_way(way);
    return way;
  }

  const T& operator [](int index) const { return tags[index]; }

  T& operator [](int index) { return tags[index]; }
  int operator ()(T target) { return probe(target); }

  stringbuf& printway(stringbuf& os, int i) const {
    os << "  way ", intstring(i, -2), ": ";
    if (tags[i] != INVALID) {
      os << "tag 0x", hexstring(tags[i], sizeof(T)*8);
      if (evictmap[i]) os << " (MRU)";
    } else {
      os << "<invalid>";
    }
    return os;
  }

  stringbuf& print(stringbuf& os) const {
    foreach (i, ways) {
      printway(os, i);
      os << endl;
    }
    return os;
  }

  ostream& print(ostream& os) const {
    stringbuf sb;
    print(sb);
    os << sb;
    return os;
  }
};

template <typename T, int ways>
ostream& operator <<(ostream& os, const FullyAssociativeTags<T, ways>& tags) {
  return tags.print(os);
}

template <typename T, int ways>
stringbuf& operator <<(stringbuf& sb, const FullyAssociativeTags<T, ways>& tags) {
  return tags.print(sb);
}

template <typename T, typename V, int ways>
struct FullyAssociativeArray {
  FullyAssociativeTags<T, ways> tags;
  V data[ways];

  FullyAssociativeArray() {
    reset();
  }

  void reset() {
    tags.reset();
    foreach (i, ways) { data[i].reset(); }
  }

  V* probe(T tag) {
    int way = tags.probe(tag);
    return (way < 0) ? null : &data[way];
  }

  V* select(T tag, T& oldtag) {
    int way = tags.select(tag, oldtag);
    return (way < 0) ? null : &data[way];
  }

  V* select(T tag) {
    T dummy;
    return select(tag, dummy);
  }

  void invalidate(T tag) { tags.invalidate(tag); }

  int wayof(const V* line) const {
    int way = (line - (const V*)&data);
#if 0
    assert(inrange(way, 0, ways-1));
#endif
    return way;
  }

  T tagof(V* line) {
    int way = wayof(line);
    return tags.tags[way];
  }

  void invalidate_line(V* line) {
    int way = wayof(line);
    tags.invalidate_way(way);
    data[way].reset();
  }
  
  V& operator [](int way) { return data[way]; }

  V* operator ()(T tag) { return select(tag); }

  ostream& print(ostream& os) const {
    foreach (i, ways) {
      stringbuf sb;
      tags.printway(sb, i);
      os << padstring(sb, -40), " -> ";
      data[i].print(os, tags.tags[i]);
      os << endl;
    }
    return os;
  }
};

template <typename T, typename V, int ways>
ostream& operator <<(ostream& os, const FullyAssociativeArray<T, V, ways>& assoc) {
  return assoc.print(os);
}

template <typename T, typename V, int setcount, int waycount, int linesize>
struct AssociativeArray {
  typedef FullyAssociativeArray<T, V, waycount> Set;
  Set sets[setcount];

  AssociativeArray() {
    reset();
  }

  void reset() {
    foreach (set, setcount) {
      sets[set].reset();
    }
  }

  static int setof(T addr) {
    return bits(addr, log2(linesize), log2(setcount));
  }

  static T tagof(T addr) {
    return floor(addr, linesize);
  }

  V* probe(T addr) {
    return sets[setof(addr)].probe(tagof(addr));
  }

  V* select(T addr, T& oldaddr) {
    return sets[setof(addr)].select(tagof(addr), oldaddr);
  }

  V* select(T addr) {
    T dummy;
    return sets[setof(addr)].select(tagof(addr), dummy);
  }

  void invalidate(T addr) {
    sets[setof(addr)].invalidate(tagof(addr));
  }

  ostream& print(ostream& os) const {
    os << "AssociativeArray<", setcount, " sets, ", waycount, " ways, ", linesize, "-byte lines>:", endl;
    foreach (set, setcount) {
      os << "  Set ", set, ":", endl;
      os << sets[set];
    }
    return os;
  }
};

template <typename T, typename V, int size, int ways, int linesize>
ostream& operator <<(ostream& os, const AssociativeArray<T, V, size, ways, linesize>& aa) {
  return aa.print(os);
}

//
// Lockable version of associative arrays:
//

template <typename T, int ways>
struct LockableFullyAssociativeTags {
  bitvec<ways> evictmap;
  bitvec<ways> unlockedmap;
  T tags[ways];

  static const T INVALID = InvalidTag<T>::INVALID;

  LockableFullyAssociativeTags() {
    reset();
  }

  void reset() {
    evictmap = 0;
    unlockedmap.setall();
    foreach (i, ways) {
      tags[i] = INVALID;
    }
  }

  void use(int way) {
    evictmap[way] = 1;
    // Performance is somewhat better with this off with higher associativity caches:
    // if (evictmap.allset()) evictmap = 0;
  }

  //
  // This is a clever way of doing branch-free matching
  // with conditional moves and addition. It relies on
  // having at most one matching entry in the array;
  // otherwise the algorithm breaks:
  //
  int match(T target) {
    int way = 0;
    foreach (i, ways) {
      way += (tags[i] == target) ? (i + 1) : 0;
    }

    return way - 1;
  }

  int probe(T target) {
    int way = match(target);
    if (way < 0) return -1;

    use(way);
    return way;
  }

  int lru() const {
    if (!unlockedmap) return -1;
    bitvec<ways> w = (~evictmap) & unlockedmap;
    return (*w) ? w.lsb() : 0;
  }

  int select(T target, T& oldtag) {
    int way = probe(target);
    if (way < 0) {
      way = lru();
      if (way < 0) return -1;
      if (evictmap.allset()) evictmap = 0;
      oldtag = tags[way];
      tags[way] = target;
    }
    use(way);
    return way;
  }

  int select(T target) {
    T dummy;
    return select(target, dummy);
  }

  int select_and_lock(T tag, bool& firstlock, T& oldtag) {
    int way = select(tag, oldtag);
    if (way < 0) return way;
    firstlock = unlockedmap[way];
    lock(way);
    return way;
  }

  int select_and_lock(T tag, bool& firstlock) {
    T dummy;
    return select_and_lock(tag, firstlock, dummy);
  }

  int select_and_lock(T target) { bool dummy; return select_and_lock(target, dummy); }

  void invalidate_way(int way) {
    tags[way] = INVALID;
    evictmap[way] = 0;
    unlockedmap[way] = 1;
  }

  int invalidate(T target) {
    int way = probe(target);
    if (way < 0) return -1;
    invalidate_way(way);
  }

  void lock(int way) { unlockedmap[way] = 0; }
  void unlock(int way) { unlockedmap[way] = 1; }

  const T& operator [](int index) const { return tags[index]; }

  T& operator [](int index) { return tags[index]; }
  int operator ()(T target) { return probe(target); }

  stringbuf& printway(stringbuf& os, int i) const {
    os << "  way ", intstring(i, -2), ": ";
    if (tags[i] != INVALID) {
      os << "tag 0x", hexstring(tags[i], sizeof(T)*8);
      if (evictmap[i]) os << " (MRU)";
      if (!unlockedmap[i]) os << " (locked)";
    } else {
      os << "<invalid>";
    }
    return os;
  }

  stringbuf& print(stringbuf& os) const {
    foreach (i, ways) {
      printway(os, i);
      os << endl;
    }
    return os;
  }

  ostream& print(ostream& os) const {
    stringbuf sb;
    print(sb);
    os << sb;
    return os;
  }
};

template <typename T, int ways>
ostream& operator <<(ostream& os, const LockableFullyAssociativeTags<T, ways>& tags) {
  return tags.print(os);
}

template <typename T, int ways>
stringbuf& operator <<(stringbuf& sb, const LockableFullyAssociativeTags<T, ways>& tags) {
  return tags.print(sb);
}

template <typename T, typename V, int ways>
struct LockableFullyAssociativeArray {
  LockableFullyAssociativeTags<T, ways> tags;
  V data[ways];

  LockableFullyAssociativeArray() {
    reset();
  }

  void reset() {
    tags.reset();
    foreach (i, ways) { data[i].reset(); }
  }

  V* probe(T tag) {
    int way = tags.probe(tag);
    return (way < 0) ? null : &data[way];
  }

  V* select(T tag, T& oldtag) {
    int way = tags.select(tag, oldtag);
    return (way < 0) ? null : &data[way];
  }

  V* select(T tag) {
    T dummy;
    return select(tag, dummy);
  }

  V* select_and_lock(T tag, bool& firstlock, T& oldtag) {
    int way = tags.select_and_lock(tag, firstlock, oldtag);
    if (way < 0) return null;
    return &data[way];
  }

  V* select_and_lock(T tag, bool& firstlock) {
    T dummy;
    return select_and_lock(tag, firstlock, dummy);
  }

  V* select_and_lock(T tag) { bool dummy; return select_and_lock(tag, dummy); }

  void invalidate(T tag) { tags.invalidate(tag); }

  int wayof(const V* line) const {
    int way = (line - (const V*)&data);
#if 0
    assert(inrange(way, 0, ways-1));
#endif
    return way;
  }

  T tagof(V* line) {
    int way = wayof(line);
    return tags.tags[way];
  }

  void invalidate_line(V* line) {
    int way = wayof(line);
    tags.invalidate_way(way);
    data[way].reset();
  }
  
  void unlock(T tag) {
    int way = probe(tag);
    tags.unlock(way);
  }

  void unlock_line(V* line) {
    int way = wayof(line);
    tags.unlock(way);
  }

  V& operator [](int way) { return data[way]; }

  V* operator ()(T tag) { return select(tag); }

  ostream& print(ostream& os) const {
    foreach (i, ways) {
      stringbuf sb;
      tags.printway(sb, i);
      os << padstring(sb, -40), " -> ";
      data[i].print(os, tags.tags[i]);
      os << endl;
    }
    return os;
  }
};

template <typename T, typename V, int ways>
ostream& operator <<(ostream& os, const LockableFullyAssociativeArray<T, V, ways>& assoc) {
  return assoc.print(os);
}

template <typename T, typename V, int setcount, int waycount, int linesize>
struct LockableAssociativeArray {
  typedef LockableFullyAssociativeArray<T, V, waycount> Set;
  Set sets[setcount];

  struct ClearList {
    W16 set;
    W16 way;
  };

  ClearList clearlist[setcount * waycount];
  ClearList* cleartail;

  LockableAssociativeArray() {
    reset();
  }

  void reset() {
    foreach (set, setcount) {
      sets[set].reset();
    }
    cleartail = clearlist;
  }

  static int setof(T addr) {
    return bits(addr, log2(linesize), log2(setcount));
  }

  static T tagof(T addr) {
    return floor(addr, linesize);
  }

  V* probe(T addr) {
    return sets[setof(addr)].probe(tagof(addr));
  }

  V* select(T addr, T& oldaddr) {
    return sets[setof(addr)].select(tagof(addr), oldaddr);
  }

  V* select(T addr) {
    T dummy;
    return select(addr, dummy);
  }

  void invalidate(T addr) {
    sets[setof(addr)].invalidate(tagof(addr));
  }

  V* select_and_lock(T addr, bool& firstlock) {
    V* line = sets[setof(addr)].select_and_lock(tagof(addr), firstlock);
    if (!line) return null;
    if (firstlock) {
      int set = setof(addr);
      int way = sets[set].wayof(line);
      cleartail->set = set;
      cleartail->way = way;
      cleartail++;
    }
    return line;
  }

  V* select_and_lock(T addr) { bool dummy; return select_and_lock(addr, dummy); }

  void unlock_all_and_invalidate() {
    ClearList* p = clearlist;
    while (p < cleartail) {
#if 0
      assert(p->set < setcount);
      assert(p->way < waycount);
#endif
      Set& set = sets[p->set];
      V& line = set[p->way];
      set.invalidate_line(&line);
      p++;
    }
    cleartail = clearlist;
#if 0
    foreach (s, setcount) {
      Set& set = sets[s];
      foreach (way, waycount) {
        V& line = set[way];
        T tag = set.tagof(&line);
        if ((tag != set.tags.INVALID)) {
          assert(false);
        }
      }
    }
#endif
  }

  void unlock_all() {
    ClearList* p = clearlist;
    while (p < cleartail) {
#if 0
      assert(p->set < setcount);
      assert(p->way < waycount);
#endif
      Set& set = sets[p->set];
      V& line = set[p->way];
      set.unlock_line(&line);
      p++;
    }
    cleartail = clearlist;
  }

  ostream& print(ostream& os) const {
    os << "LockableAssociativeArray<", setcount, " sets, ", waycount, " ways, ", linesize, "-byte lines>:", endl;
    foreach (set, setcount) {
      os << "  Set ", set, ":", endl;
      os << sets[set];
    }
    return os;
  }
};

template <typename T, typename V, int size, int ways, int linesize>
ostream& operator <<(ostream& os, const LockableAssociativeArray<T, V, size, ways, linesize>& aa) {
  return aa.print(os);
}

//
// Lockable cache arrays supporting commit/rollback
//
// This structure implements the dirty-and-locked scheme to prevent speculative
// data from propagating to lower levels of the cache hierarchy until it can be
// committed.
//
// Any stores into the cache (signalled by select_and_lock()) back up the old
// cache line and add this to an array for later rollback purposes.
//
// At commit(), all locked lines are unlocked and the backed up cache lines are
// simply discarded, leaving them free to be replaced or written back.
//
// At rollback() all locked lines are invalidated in both this cache and any
// higher levels (via the invalidate_upwards() callback), thereby forcing
// clean copies to be refetched as needed after the rollback.
//

template <typename T, typename V, int setcount, int waycount, int linesize, int maxdirty>
struct CommitRollbackCache: public LockableAssociativeArray<T, V, setcount, waycount, linesize> {
  typedef LockableAssociativeArray<T, V, setcount, waycount, linesize> array_t;
  
  struct BackupCacheLine {
    W64* addr;
    W64 data[linesize / sizeof(W64)];
  };

  BackupCacheLine stores[maxdirty];
  BackupCacheLine* storetail;
  
  CommitRollbackCache() {
    reset();
  }

  void reset() {
    array_t::reset();
    storetail = stores;
  }

  //
  // Invalidate lines in higher level caches if needed
  //
  void invalidate_upwards(T addr);
  
  void invalidate(T addr) {
    array_t::invalidate(addr);
    invalidate_upwards(addr);
  }

  V* select_and_lock(T addr) {
    addr = floor(addr, linesize);

    bool firstlock;
    V* line = array_t::select_and_lock(addr, firstlock);
    if (!line) return null;

    if (firstlock) {
      W64* linedata = (W64*)addr;
      storetail->addr = linedata;
      foreach (i, lengthof(storetail->data)) storetail->data[i] = linedata[i];
      storetail++;
    }

    return line;
  }

  void commit() {
    array_t::unlock_all();
    storetail = stores;
  }

  void rollback() {
    array_t::unlock_all_and_invalidate();

    BackupCacheLine* cl = stores;
    while (cl < storetail) {
      W64* linedata = cl->addr;
      foreach (i, lengthof(storetail->data)) linedata[i] = cl->data[i];
      invalidate_upwards((W64)cl->addr);
      cl++;
    }
    storetail = stores;
  }
  
  void complete() { }
};

template <int size, int padsize = 0>
struct FullyAssociativeTags8bit {
  typedef vec16b vec_t;
  typedef byte base_t;

  static const int chunkcount = (size+15) / 16;
  static const int padchunkcount = (padsize+15) / 16;

  vec_t tags[chunkcount + padchunkcount] alignto(16);
  bitvec<size> valid;

  W64 getvalid() { return valid.integer(); }

  FullyAssociativeTags8bit() {
    reset();
  }

  base_t operator [](int i) const {
    return ((base_t*)&tags)[i];
  }

  base_t& operator [](int i) {
    return ((base_t*)&tags)[i];
  }

  bool isvalid(int index) {
    return valid[index];
  }

  void reset() {
    valid = 0;
    W64* p = (W64*)&tags;
    foreach (i, ((chunkcount + padchunkcount)*16)/8) p[i] = 0xffffffffffffffffLL;
  }

  static const vec_t prep(base_t tag) {
    return x86_sse_dupb(tag);
  }

  int insertslot(int idx, base_t tag) {
    valid[idx] = 1;
    (*this)[idx] = tag;
    return idx;
  }

  int insert(base_t tag) {
    if (valid.allset()) return -1;
    int idx = (~valid).lsb();
    return insertslot(idx, tag);
  }

  bitvec<size> match(const vec16b target) const {
    bitvec<size> m = 0;

    foreach (i, chunkcount) {
      m = m.accum(i*16, 16, x86_sse_pmovmskb(x86_sse_pcmpeqb(target, tags[i])));
    }

    return m & valid;
  }

  int search(const vec16b target) const {
    bitvec<size> bitmap = match(target);
    int idx = bitmap.lsb();
    if (!bitmap) idx = -1;
    return idx;
  }

  int extract(const vec16b target) {
    int idx = search(target);
    if (idx >= 0) valid[idx] = 0;
    return idx;
  }

  int search(base_t tag) const {
    return search(prep(tag));
  }

  bitvec<size> extract(base_t tag) {
    return extract(prep(tag));
  }

  void invalidateslot(int index) {
    valid[index] = 0;
  }

  const bitvec<size>& invalidatemask(const bitvec<size>& mask) {
    valid &= ~mask;
    return mask;
  }

  bitvec<size> invalidate(const vec16b target) {
    return invalidatemask(match(target));
  }

  bitvec<size> invalidate(base_t target) {
    return invalidate(prep(target));
  }

  void collapse(int index) {
    byte* tagbase = (byte*)&tags;
    byte* base = tagbase + index;
    vec_t* dp = (vec_t*)base;
    vec_t* sp = (vec_t*)(base + sizeof(base_t));

    foreach (i, (ceil(size, 16) / 16)) {
      x86_sse_stvbu(dp++, x86_sse_ldvbu(sp++));
    }

    valid = valid.remove(index);
  }

  void decrement(base_t amount = 1) {
    foreach (i, chunkcount) { tags[i] = x86_sse_psubusb(tags[i], prep(amount)); }
  }

  void increment(base_t amount = 1) {
    foreach (i, chunkcount) { tags[i] = x86_sse_paddusb(tags[i], prep(amount)); }
  }

  ostream& printid(ostream& os, int slot) const {
    int tag = (*this)[slot];
    if (valid[slot])
      os << intstring(tag, 3);
    else os << "???";
    return os;
  }
};

template <int size, int padsize = 0>
struct FullyAssociativeTags16bit {
  typedef vec8w vec_t;
  typedef W16 base_t;

  static const int chunkcount = ((size*2)+15) / 16;
  static const int padchunkcount = ((padsize*2)+15) / 16;

  vec_t tags[chunkcount + padchunkcount] alignto(16);
  bitvec<size> valid;

  W64 getvalid() { return valid.integer(); }

  FullyAssociativeTags16bit() {
    reset();
  }

  base_t operator [](int i) const {
    return ((base_t*)&tags)[i];
  }

  base_t& operator [](int i) {
    return ((base_t*)&tags)[i];
  }

  bool isvalid(int index) {
    return valid[index];
  }

  void reset() {
    valid = 0;
    W64* p = (W64*)&tags;
    foreach (i, ((chunkcount + padchunkcount)*16)/8) p[i] = 0xffffffffffffffffLL;
  }

  static const vec_t prep(base_t tag) {
    return x86_sse_dupw(tag);
  }

  int insertslot(int idx, base_t tag) {
    valid[idx] = 1;
    (*this)[idx] = tag;
    return idx;
  }

  int insert(base_t tag) {
    if (valid.allset()) return -1;
    int idx = (~valid).lsb();
    return insertslot(idx, tag);
  }

  bitvec<size> match(const vec8w target) const {
    bitvec<size> m = 0;

    foreach (i, chunkcount) {
      m.accum(i*16, 16, x86_sse_pmovmskw(x86_sse_pcmpeqw(target, tags[i])));
    }

    return m & valid;
  }

  int search(const vec8w target) const {
    bitvec<size> bitmap = match(target);
    int idx = bitmap.lsb();
    if (!bitmap) idx = -1;
    return idx;
  }

  int extract(const vec8w target) {
    int idx = search(target);
    if (idx >= 0) valid[idx] = 0;
    return idx;
  }

  int search(base_t tag) const {
    return search(prep(tag));
  }

  bitvec<size> extract(base_t tag) {
    return extract(prep(tag));
  }

  void invalidateslot(int index) {
    valid[index] = 0;
  }

  const bitvec<size>& invalidatemask(const bitvec<size>& mask) {
    valid &= ~mask;
    return mask;
  }

  bitvec<size> invalidate(const vec_t target) {
    return invalidatemask(match(target));
  }

  bitvec<size> invalidate(base_t target) {
    return invalidate(prep(target));
  }

  void collapse(int index) {
    byte* tagbase = (byte*)&tags;
    byte* base = tagbase + index;
    vec_t* dp = (vec_t*)base;
    vec_t* sp = (vec_t*)(base + sizeof(base_t));

    foreach (i, (ceil(size, 16) / 16)) {
      x86_sse_stvwu(dp++, x86_sse_ldvwu(sp++));
    }

    valid = valid.remove(index);
  }

  void decrement(base_t amount = 1) {
    foreach (i, chunkcount) { tags[i] = x86_sse_psubusw(tags[i], prep(amount)); }
  }
      
  void increment(base_t amount = 1) {
    foreach (i, chunkcount) { tags[i] = x86_sse_paddusw(tags[i], prep(amount)); }
  }

  ostream& printid(ostream& os, int slot) const {
    int tag = (*this)[slot];
    if (valid[slot])
      os << intstring(tag, 3);
    else os << "???";
    return os;
  }
};

#endif // _LOGIC_H_