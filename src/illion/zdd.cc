#include "illion/zdd.h"

#include <cassert>
#include <climits>

#include <unordered_map>

#include "illion/util.h"

namespace illion {

using std::pair;
using std::set;
using std::unordered_map;
using std::unordered_set;
using std::vector;

zdd_t zdd::single(elem_t e) {
  assert(0 < e && e <= BDD_MaxVar);
  if (!initialized_) {
    BDD_Init(1000000, 8000000000LL);
    initialized_ = true;
  }
  for (; num_elems_ < e; ++num_elems_)
    top().Change(BDD_NewVarOfLev(1));
  return top().Change(e);
}

zdd_t zdd::_not(zdd_t f) {
  vector<zdd_t> n(num_elems_ + 2);
  n[0] = bot(), n[1] = top();
  for (elem_t v = num_elems_; v > 0; --v) {
    elem_t i = num_elems_ - v + 2;
    n[i] = n[i - 1] + single(v) * n[i - 1];
  }
  return n[num_elems_ + 1] - f;
}

zdd_t zdd::minimal(zdd_t f) {
  if (is_term(f)) return f;
  vector<vector<zdd_t> > stacks(num_elems_ + 1);
  unordered_set<word_t> visited;
  sort_zdd(f, &stacks, &visited);
  unordered_map<word_t, zdd_t> cache
    = { {id(bot()), bot()}, {id(top()), top()} };
  for (elem_t v = num_elems_; v > 0; --v) {
    while (!stacks[v].empty()) {
      zdd_t n = stacks[v].back();
      stacks[v].pop_back();
      cache[id(n)]
        = cache.at(id(lo(n)))
        + (cache.at(id(hi(n))) - cache.at(id(lo(n)))).Change(v);
    }
  }
  return cache.at(id(f));
}

zdd_t zdd::maximal(zdd_t f) {
  if (is_term(f)) return f;
  vector<vector<zdd_t> > stacks(num_elems_ + 1);
  unordered_set<word_t> visited;
  sort_zdd(f, &stacks, &visited);
  unordered_map<word_t, zdd_t> cache
    = { {id(bot()), bot()}, {id(top()), top()} };
  for (elem_t v = num_elems_; v > 0; --v) {
    while (!stacks[v].empty()) {
      zdd_t n = stacks[v].back();
      stacks[v].pop_back();
      cache[id(n)]
        = cache.at(id(lo(n))) - cache.at(id(lo(n))).Permit(cache.at(id(hi(n))))
        + cache.at(id(hi(n))).Change(v);
    }
  }
  return cache.at(id(f));
}

zdd_t zdd::hitting(zdd_t f) {
  if (is_bot(f)) return top();
  if (is_top(f)) return bot();
  vector<vector<zdd_t> > stacks(num_elems_ + 1);
  unordered_set<word_t> visited;
  sort_zdd(f, &stacks, &visited);
  unordered_map<word_t, zdd_t> cache
    = { {id(bot()), bot()}, {id(top()), bot()} };
  for (elem_t v = num_elems_; v > 0; --v) {
    while (!stacks[v].empty()) {
      zdd_t n = stacks[v].back();
      stacks[v].pop_back();
      zdd_t l = cache.at(id(lo(n)));
      if (lo(n) != bot()) {
        elem_t j = is_top(lo(n)) ? num_elems_ : elem(lo(n)) - 1;
        for (; j > v; --j)
          l += l.Change(j);
      }
      zdd_t h = cache.at(id(hi(n)));
      if (hi(n) != bot()) {
        elem_t j = is_top(hi(n)) ? num_elems_ : elem(hi(n)) - 1;
        for (; j > v; --j)
          h += h.Change(j);
      }
      if (lo(n) == bot()) {
        zdd_t g = top();
        for (elem_t j = num_elems_; j > v; --j)
          g += g.Change(j);
        g = g.Change(elem(n));
        cache[id(n)] = h + g;
      } else {
        cache[id(n)] = (h & l) + l.Change(v);
      }
    }
  }
  zdd_t g = cache.at(id(f));
  elem_t j = is_term(f) ? num_elems_ : elem(f) - 1;
  for (; j > 0; --j)
    g += g.Change(j);
  return g;
}

struct bdd_pair_hash {
  size_t operator()(const pair<word_t, word_t>& o) const {
    return (o.first << 4*sizeof(o.first)) ^ o.second;
  }
};

struct bdd_pair_eq {
  bool operator()(const pair<word_t,word_t>& a, const pair<word_t,word_t>& b) const {
    return a.first == b.first && a.second == b.second;
  }
};

zdd_t zdd::nonsubsets(zdd_t f, zdd_t g) {
  static unordered_map<pair<word_t, word_t>, zdd_t, bdd_pair_hash, bdd_pair_eq> cache;
  if (g == bot())
    return f;
  else if (f == bot() || f == top() || f == g)
    return bot();
  pair<word_t, word_t> k = make_key(f, g);
  auto i = cache.find(k);
  if (i != cache.end())
    return i->second;
  zdd_t rl;
  zdd_t rh;
  if (elem(f) < elem(g)) {
    rl = nonsubsets(lo(f), g);
    rh = hi(f);
  }
  else {
    rl = nonsubsets(lo(f), hi(g)) & nonsubsets(lo(f), lo(g));
    rh = nonsubsets(hi(f), hi(g));
  }
  return cache[k] = zuniq(elem(f), rl, rh);
}

zdd_t zdd::nonsupersets(zdd_t f, zdd_t g) {
  static unordered_map<pair<word_t, word_t>, zdd_t, bdd_pair_hash, bdd_pair_eq> cache;
  if (g == bot())
    return f;
  else if (f == bot() || g == top() || f == g)
    return bot();
  else if (elem(f) > elem(g))
    return nonsupersets(f, lo(g));
  pair<word_t, word_t> k = make_key(f, g);
  auto i = cache.find(k);
  if (i != cache.end())
    return i->second;
  zdd_t rl;
  zdd_t rh;
  if (elem(f) < elem(g)) {
    rl = nonsupersets(lo(f), g);
    rh = nonsupersets(hi(f), g);
  }
  else {
    rl = nonsupersets(lo(f), lo(g));
    rh = nonsupersets(hi(f), hi(g)) & nonsupersets(hi(f), lo(g));
  }
  return cache[k] = zuniq(elem(f), rl, rh);
}

zdd_t zdd::choose_random(zdd_t f, vector<elem_t>* stack, int* idum) {
  assert(stack != NULL && idum != NULL);
  if (is_term(f)) {
    if (is_top(f)) {
      zdd_t g = top();
      for (int i = 0; i <= static_cast<int>(stack->size()) - 1; i++)
        g *= single((*stack)[i]);
      return g;
    }
    assert(false);
  }
#ifdef HAVE_LIBGMPXX
  double ch = algo_c(hi(f)).get_d();
  double cl = algo_c(lo(f)).get_d();
#else
  double ch = algo_c(hi(f))
  double cl = algo_c(lo(f))
#endif
  if (ran3(idum) > cl / (ch + cl)) {
    stack->push_back(elem(f));
    return choose_random(hi(f), stack, idum);
  } else {
    return choose_random(lo(f), stack, idum);
  }
}

zdd_t zdd::choose_best(zdd_t f, const vector<int>& weights, set<elem_t>* s) {
  assert(s != NULL);
  if (is_bot(f)) return bot();
  vector<bool> x;
  algo_b(f, weights, &x);
  zdd_t g = top();
  s->clear();
  for (elem_t j = 1; j <= num_elems_; j++) {
    if (x[j]) {
      g *= single(j);
      s->insert(j);
    }
  }
  return g;
}

// Algorithm B modified for ZDD, from Knuth vol. 4 fascicle 1 sec. 7.1.4.
void zdd::algo_b(zdd_t f, const vector<int>& w, vector<bool>* x) {
  assert(w.size() > static_cast<size_t>(num_elems_));
  assert(x != NULL);
  assert(!is_bot(f));
  x->clear();
  x->resize(num_elems_ + 1, false);
  if (is_top(f)) return;

  unordered_map<word_t, bool> t;
  unordered_map<word_t, int> ms = {{id(bot()), INT_MIN}, {id(top()), 0}};

  vector<vector<zdd_t> > stacks(num_elems_ + 1);
  unordered_set<word_t> visited;
  sort_zdd(f, &stacks, &visited);

  for (elem_t v = num_elems_; v > 0; --v) {
    while (!stacks[v].empty()) {
      zdd_t g = stacks[v].back();
      stacks[v].pop_back();
      word_t k = id(g);
      elem_t v = elem(g);
      word_t l = id(lo(g));
      word_t h = id(hi(g));
      if (!is_bot(lo(g)))
        ms[k] = ms.at(l);
      if (!is_bot(hi(g))) {
        int m = ms.at(h) + w[v];
        if (is_bot(lo(g)) || m > ms.at(k)) {
          ms[k] = m;
          t[k] = true;
        }
      }
    }
  }

  while (!is_term(f)) {
    word_t k = id(f);
    elem_t v = elem(f);
    if (t.find(k) == t.end())
      t[k] = false;
    (*x)[v] = t.at(k);
    f = !t.at(k) ? lo(f) : hi(f);
  }
}

// Algorithm C modified for ZDD, from Knuth vol. 4 fascicle 1 sec. 7.1.4 (p.75).
intx_t zdd::algo_c(zdd_t f) {
    static unordered_map<word_t, intx_t> counts;
    if (is_term(f))
        return is_top(f) ? 1 : 0;
    else if (counts.find(id(f)) != counts.end())
        return counts.at(id(f));
    else
        return counts[id(f)] = algo_c(hi(f)) + algo_c(lo(f));
}

// Algorithm ZUNIQ from Knuth vol. 4 fascicle 1 sec. 7.1.4.
zdd_t zdd::zuniq(elem_t v, zdd_t l, zdd_t h) {
  return l + single(v) * h;
}

// Seminumerical Algorithms from Knuth vol. 2, sec. 3.2-3.3.
#define MBIG 1000000000
#define MSEED 161803398
#define MZ 0
#define FAC (1.0/MBIG)
double zdd::ran3(int* idum) {
  static int inext, inextp;
  static long ma[56];
  static int iff = 0;
  long mj, mk;
  int i, ii, k;

  if (*idum < 0 || iff == 0) {
    iff = 1;
    mj = labs(MSEED - labs(*idum));
    mj %= MBIG;
    ma[55] = mj;
    mk = 1;
    for (i = 1; i <= 54; ++i) {
      ii = (21*i) % 55;
      ma[ii] = mk;
      mk = mj - mk;
      if (mk < MZ) mk += MBIG;
      mj = ma[ii];
    }
    for (k = 1; k <= 4; ++k)
      for (i = 1; i <= 55; ++i) {
        ma[i] -= ma[1 + (i+30) % 55];
        if (ma[i] < MZ) ma[i] += MBIG;
      }
    inext = 0;
    inextp = 31;
    *idum = 1;
  }
  if (++inext == 56) inext = 1;
  if (++inextp == 56) inextp = 1;
  mj = ma[inext] - ma[inextp];
  if (mj < MZ) mj += MBIG;
  ma[inext] = mj;
  return mj * FAC;
}

void zdd::sort_zdd(zdd_t f, vector<vector<zdd_t> >* stacks,
                   unordered_set<word_t>* visited) {
  assert(stacks != nullptr && visited != nullptr);
  if (!is_term(f) && visited->find(id(f)) == visited->end()) {
    (*stacks)[elem(f)].push_back(f);
    visited->insert(id(f));
    sort_zdd(lo(f), stacks, visited);
    sort_zdd(hi(f), stacks, visited);
  }
}

void zdd::dump(zdd_t f) {
  vector<elem_t> stack;
  printf("{");
  dump(f, &stack);
  printf("}\n");
}

void zdd::dump(zdd_t f, vector<elem_t>* stack) {
  assert(stack != nullptr);
  if (is_term(f)) {
    if (is_top(f))
      printf("{%s},", join(*stack, ",").c_str());
    return;
  }
  stack->push_back(elem(f));
  dump(hi(f), stack);
  stack->pop_back();
  dump(lo(f), stack);
}

bool zdd::initialized_ = false;
elem_t zdd::num_elems_ = 0;

}  // namespace illion
