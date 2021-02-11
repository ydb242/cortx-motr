======================================================
High level design of a parity de-clustering algorithm
======================================================

This document presents a high level design (HLD) of a parity de-clustering algorithms and its demonstration program. The main purposes of this document are: (i) to be inspected by Motr architects and peer designers to ascertain that high level design is aligned with Motr architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of Motr customers, architects, designers and developers.

***************
Introduction
***************

This specification describes layout mapping functions of Motr parity de-clustered layouts. The purpose of such a function is to prescribe how file data are redundantly stored over a set of storage objects while maintaining desirable fault-tolerance characteristics.

*************
Definitions
*************

This document includes by reference all definitions from the SNS Repair HLD [1]. Additionally,

- in the context of this document only layouts of a single storage pool, that will be called simply the pool are considered;

- number of target storage objects allocated in the pool for a layout will be denoted as P;

- number of data units in a parity group of a layout will be denoted as N;

- number of parity units in a parity group will be denoted as K (note that K is also equal to the number of spare units in a group);

- for a given 𝑖≤𝐾, union of i-th spare units from all parity groups in the pool is called an i-th spare slot (of the pool). A spare slot is used to repair a device failure;

- a failure vector is defined as an ordered collection of pool storage device identifies [D1, D2, ..., Dk], where Di is an identify of a pool storage device failed at the moment and for 𝑖≤𝐾 repair of device Di uses spare slot i. When no devices are failed, the failure vector is empty ([]). When the failure vector has more than K entries, the pool is dud (see [1]);

***************
Requirements
***************

- [r.pdec.N+K]: a parity group width and a number of parity units in it can be specified for a parity de-clustered layout, where N and K are specified as pool parameters;

- [r.pdec.P]: a parity de-clustered layout with a specified parity group parameters can be implemented on top of a given number of target objects;

- [r.pdec.valid]: produced parity de-clustered layout valid in that it provides required degree of fault tolerance by never mapping two units of the same parity group to the same device;

- [r.pdec.unit-size]: size of data and parity units used by a layout are specifiable as layout parameters;

- [r.pdec.space.uniform]: a parity de-clustered layout consumes space on pool drives uniformly as object size grows infinitely;

- [r.pdec.repair.uniform]: an amount of data that has to be read from a storage device A to repair a layout when a storage device B fails is equal for all possible (A, B) pairs assuming sufficiently high percentage of storage drives is occupied;

- [r.pdec.degraded]: given a parity de-clustered layout and a failure vector it is possible to efficiently determine how a client should execute IO requests under given failures (how-s include a possible requirement to re-fetch the layout). Additionally, SNS repair process must be able to efficiently determine repair data-flow for a given layout;

- [r.pdec.formula]: parity de-clustered layouts can be efficiently generated from a layout formula by substituting parameters.

For the sake of completeness below is a standard list of requirements for a parity de-clustered layout as identified in [0]:

1. Single failure correcting. No two stripe units in the same parity stripe may reside on the same physical disk. This is the basic characteristic of any redundancy organization that recovers the data of failed disks. In arrays in which groups of disks have a common failure mode, such as power or data cabling, this criteria should be extended to prohibit the allocation of stripe units from one parity stripe to two or more disks sharing that common failure mode.

2. Distributed reconstruction. When any disk fails, its user workload should be evenly distributed across all other disks in the array. When replaced or repaired, its reconstruction workload should also be evenly distributed.

3. Distributed parity. Parity information should be evenly distributed across the array. Every data update causes a parity update, and so an uneven parity distribution would lead to imbalanced utilization (hot spots), since the disks with more parity would experience more load.

4. Efficient mapping. The functions mapping a file system’s logical block address to physical disk addresses for the corresponding data units and parity stripes, and the appropriate inverse mappings, must be efficiently implementable; they should consume neither excessive computation nor memory resources.

5. Large write optimization. The allocation of contiguous user data to data units should correspond to the allocation of data units to parity stripes. This insures that whenever a user performs a write that is the size of the data portion of a parity stripe and starts on a parity stripe boundary, it is possible to execute the write without pre-reading the prior contents of any disk data, since the new parity unit depends only on the new data.

6. Maximal parallelism. A read of contiguous user data with size equal to a data unit times the number of disks in the array should induce a single data unit read on all disks in the array (while requiring alignment only to a data unit boundary). This insures that maximum parallelism can be obtained.

******************
Design Highlights
******************

Present parity de-clustered layout is based on combinatorial layouts (see [5]), as opposed to so called block design layouts ([2]). The key observation behind this decision is that while block design based layouts strictly conform to the uniformity requirements mentioned above under all circumstances, they are difficult to build and pose artificial constraints on admissible values of (N, K, P). Combinatorial layouts, on the other hand, are easy to build for any (N, K, P) (provided 𝑁 + 2⋅𝐾≤𝑃), at the expense of only guaranteeing uniformity asymptotically, i.e., for a sufficiently large number of units and objects.

***************************
Functional Specification
***************************

Three major external interfaces are described in this specification:

- a layout formula interface that generates a parity de-clustered layout by substituting parameters into a layout formula;

- a layout interface that plugs layout into client and server IO sub-systems, and

- a parity de-clustering demonstration program to analyze and simulate behavior of parity de-clustered layouts

A client fetches a layout from a Layout Data-Base (LDB) by sending a lookup query. LDB query typically returns a layout formula that can be instantiated into a layout by substituting parameters such as object fid and current pool failure vector to obtain a layout. Note that parameters are typically managed by the resource framework: cached on clients and revoked on resource usage conflicts. Layout formulae and layouts are resources too.

A file layout is used by a client to do IO against a file. The most basic layout IO interface is a mapping of file's logical namespace into storage objects' namespaces. A server uses layouts for SNS repair.

************************
Logical Specification
************************


************************
Layout Mapping Function
************************

The key question of a parity de-clustered layout design is a method for construction of a layout mapping function satisfying given requirements. Let's outline one such method, starting with an informal description. Precise definition of this method is given at the end of this sub-section.

First, some assumptions. A unit size is number of bytes in a data, parity or spare unit of a layout, denoted as U. For each layout the pool contains P objects (target objects). A target object is logically divided into frames of layout unit size. To specify a layout it is enough to specify how data, parity and spare units map to the target frames.

Some notation. For any natural number k, let S(k) denote the set {0, ..., k - 1}. Data units in a given parity group are indexed by S(N). Parity units and spare units are indexed by S(K). Target objects for a layout are indexed by S(P). Target frames in a target object are indexed by the set R = {0, 1, ...}. Target address-space, the set of all frames in target objects is 𝑇 = 𝑅×𝑆(𝑃).

We shall use g to denote a parity group (number), u for a unit (number) in a parity group, p for a target object (number) and r for a frame (number).

Let's fix an enumeration of units within a parity group:

To specify a layout mapping for an object one has to present a function

- 𝑓:𝐺×𝑆(𝑁+2⋅𝐾) →𝑇

mapping pairs (parity-group-number, unit-number) to pairs (target-object-frame, target-object-number) and also a function.

- 𝑚:ℕ →𝐺×𝑆(𝑁+2⋅𝐾)×𝑆(𝑈)

mapping logical byte offset in the object to the (parity-group-number, unit-number, byte-offset-in-unit) triples. In all cases considered in this document,

::

 m(offset) {

         group_size = U*(N+2*K);

         group = offset / group_size;

         unit = (offset - group*group_size) / U;

         unit_offset = offset - group*group_size - unit*U;

         return (group, unit, unit_offset);

  }

