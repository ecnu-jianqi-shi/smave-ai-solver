# Supplementary Theory: Finite Verified Cascades

This supplement expands the proof sketches in Section 3 of the manuscript. It uses
the same finite action, one-action-per-expert, terminal-continuation, and
original-equation-gate model as the production optimizer.

## Independent Actions

Let action `a` have expert `e(a)`, cost `K_a >= 0`, and history-independent
acceptance probability `0 < p_a <= 1`. A feasible cascade contains at most `k`
actions and at most one action per expert. Its expected cost is

```text
sum_j [prod_{h<j}(1-p_h)] K_j + [prod_h(1-p_h)] C_t.
```

For two adjacent actions `a,b` before a common suffix, the two local costs differ by
`K_a p_b - K_b p_a`; hence `a` precedes `b` exactly when
`K_a/p_a <= K_b/p_b`. Repeated exchanges place every selected subset in this order
without increasing cost. The manuscript recurrence then exhausts the two disjoint
possibilities at each sorted index: skip the action, or take it and reach the optimal
suffix only after rejection. Backward induction proves global optimality, including
the empty cascade with value `C_t`.

For reachable states, remaining capacity is `k-|S|`; it is not an independent state
dimension. At most

```text
(n+1) * sum_{l=0}^{min(k,m)} binom(m,l)
```

scalar values and backpointers are needed. Each has constant-many transitions after
sorting, giving `O(n 2^m)` worst-case time and space.

## Adjacent Transition Costs

Let `M_{b,a} > 0` multiply the base cost of action `a` when it immediately follows
action `b`, with `M_{bottom,a}=1` for a first action. Acceptance remains
history-independent, and no cost depends on history earlier than `b`. For used
experts `S` and previous action `b`, define

```text
W(S,b) = min(C_t,
             min_{a:e(a) not in S}
               K_a M_{b,a} + (1-p_a) W(S union {e(a)},a)).
```

The inner minimum is disabled at capacity. Any feasible suffix either stops or
chooses exactly one unused-expert action next. Under the adjacent-Markov assumption,
the rejection suffix is completely characterized by the new used set and previous
action. Induction on remaining capacity therefore proves that `W(empty,bottom)` is
globally optimal over all subsets and orders.

There are at most `n+1` possible previous-action values for each feasible expert set,
so scalar storage is bounded by

```text
(n+1) * sum_{l=0}^{min(k,m)} binom(m,l).
```

Each state scans at most `n` actions, giving `O(n^2 2^m)` worst-case transitions.
If expert `e` exposes `b_e` actions, the exact number of states visited by this
unpruned recurrence is

```text
1 + sum_{S: 1 <= |S| <= min(k,m)} sum_{e in S} b_e.
```

The initial one is `(empty,bottom)`; for every nonempty used set `S`, the previous
action may be any of the `b_e` actions belonging to an expert `e` in `S`. Every such
state is reachable by ordering the experts in `S` with that action last. For uniform
`b` this becomes `1 + b * sum_l l * binom(m,l)`. The production preflight computes
the nonuniform expression in `O(mk)` time and `O(k)` scalar storage by maintaining,
for each subset size, both the subset count and the sum of previous-action choices.
Saturating arithmetic makes overflow conservative. If the exact count exceeds the
configured cap, the optimizer rejects before allocating or visiting memo states.

The C++ implementation additionally uses a 64-bit expert mask and supports at most 63
experts.

## NP-Completeness

Consider the rational decision problem asking whether a feasible cascade has expected
cost at most a supplied threshold. A cascade is a polynomial certificate, so the
problem is in NP.

Reduce directed Hamiltonian path for a graph with `n` vertices:

1. Create one action and one expert per vertex.
2. Set `K_a=1`, `p_a=1/2`, `k=n`, and `C_t=5`.
3. Set `M_{u,v}=1` if directed edge `(u,v)` exists and `M_{u,v}=2` otherwise.

At any non-full state, appending an unused action and then stopping costs at most
`2 + (1/2)5 = 4.5`, strictly below immediate terminal cost `5`. Thus every optimum
uses all vertices. A full sequence using only graph edges has cost

```text
T_n = sum_{j=0}^{n-1} 2^{-j} + 5 * 2^{-n}.
```

Every nonedge at position `j` changes its multiplier from one to two and adds the
strictly positive discounted amount `2^{-(j-1)}`. Therefore the optimum is at most
`T_n` if and only if the graph has a directed Hamiltonian path. The decision problem
is NP-complete even under the uniform parameters and binary multipliers above.

## Executable Checks

`tests/joint_route_budget_evidence.cpp` provides three independent checks:

- 256 independent six-action cases against all feasible subsets and permutations;
- 256 adjacent-interaction six-action cases, each with 24 transition multipliers,
  against the same exhaustive oracle;
- all 4,096 directed simple graphs on four vertices against the Hamiltonian threshold
  equivalence above.

The generated evidence records zero maximum oracle gap, 126 interaction-induced plan
changes, expert exclusivity, full-length hardness constructions, and exact reduction
agreement.

`tests/joint_route_scaling_evidence.cpp` independently evaluates the closed-form state
and transition counts, compares them with production diagnostics on eight uniform
profiles and the deployed 12-action shape, and checks exact cap acceptance plus
zero-visit preflight rejection. It executes no numerical solver and makes no timing or
memory claim.
