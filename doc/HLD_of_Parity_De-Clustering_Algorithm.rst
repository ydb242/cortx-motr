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



