# Hostlist expressions #

---

Pdsh can accept lists of hosts in the general form
> ` prefix[n-m,l-k,...]`
where `n < m` and `l < k`, etc., as an alternative to explicit
lists of hosts. This form should not be confused with regular expression
character classes (also denoted by `'[]'`). For example, `foo[19]` does
not represent an expression matching `foo1` or `foo9`, but rather
represents the single host `foo19`.

The hostlist syntax is meant only as a convenience on clusters with
`"prefixNNN"` style naming convention and spefication of host ranges
should not be considered necessary, i.e. `foo1,foo9` is equally as
acceptable as `foo[1,9]`.

Examples

> Run command on `foo01,foo02,...,foo05`:
```
  pdsh -w foo[01-05] command
```

> Run command on `foo7,foo9,foo10`
```
 pdsh -w foo[7,9-10] command
```

A simple, non-numeric suffix on the hostname is also supported:

> Run command on `foo0-eth0,foo1-eth0,foo2-eth0,foo3-eth0`
```
 pdsh -w foo[0-3]-eth0 command
```

Note also that up to two sets of brackets are also supported in **_pdsh_**.
This is because **_pdsh_** makes two passes through the list of hosts
trying to expand host ranges. For example,

> Run command on `foo1-0,foo1-1,foo1-2,foo1-3,foo2-0,foo2-1,foo2-2,foo2-3`
```
 pdsh -w foo[1-2]-[0-3] command
```

---

> > <b>Note</b>:
> > > Some shells will interpret brackets (`'['` and `']'`) for
> > > pattern matching. Depending on your shell, it may be necessary to
> > > enclose ranged lists within quotes. For example, in `tcsh`
```
pdsh -w "foo[0,4-5]" ...
```

---
