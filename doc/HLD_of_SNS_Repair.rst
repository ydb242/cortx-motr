==================================
High Level Design of SNS Repair
==================================

This document presents a high level design (HLD) of SNS repair of Motr M0. The main purposes of this document are: (i) to be inspected by M0 architects and peer designers to ascertain that high level design is aligned with M0 architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document.

The intended audience of this document consists of M0 customers, architects, designers and developers.

*************
Introduction
*************

See HLD template document for important note on terminology used by the high level design specifications.

Redundant striping is a proved technology to achieve higher throughput and data availability. SNS (a.k.a Server Network Striping) applies this technology to networked devices, and achieves the similar goals as local RAID.

In the presence of storage and/or server failure, SNS repair will reconstruct lost data from surviving data reliably and quickly, without major impact to the product systems. This document will present the HLD of SNS repair.

*************
Definitions
*************

Repair is a scalable mechanism to reconstruct data or meta-data in a redundant striping pool. Redundant striping stores a cluster-wide object in a collection of components, according to a particular striping pattern.

The following terms are used to discuss and describe Repair:

- storage devices are attached to data servers;

- storage objects provide access to storage device contents by means of a linear name-space associated with an object;

- some of the objects are containers, capable of storing other objects. Containers are organized into a hierarchy;

- a cluster-wide object is an array of bytes (the object's linear name-space, called cluster-wide object data) and accompanying meta-data, stored in containers and accessed through at least read and write operations (and potentially other operations of POSIX or similar interface). Index in this array is called an offset; A cluster-wide object can appear as a file if it is visible in file system namespace.

- a cluster-wide object is uniquely identified by an identifier, called a fid; 

- a cluster-wide object layout [4], roughly speaking, is a map, determining where on the storage a particular element of its byte array is located. For the purposes of the present discussion only striping layouts, that will be called simply layouts, are needed. There are other layouts for encryption, deduplication, etc. that look quite differently. In addition to the cluster-wide object data, a cluster-wide object layout specifies where and how redundancy information is stored;

- a cluster-wide object layout specifies a location of its data or redundancy information as a pair (component-id, component-offset), where component-id is a fid of a component that is stored in a certain container (Layouts introduce constraints on fids of components of a given cluster-wide object, but these are not important for the present specification.);

- a component is an object, and, like a cluster-wide object, an array of bytes (component data) identified by a (component) offset plus some meta-data;

- a component has allocation data—a meta-data specifying where in the container component data are stored;

- cluster-wide object layouts, considered in this document, are piecewise linear mappings in the sense that for every layout there exists a positive integer S, called a (layout) striping unit size, such that the layout maps any extent [p*S, (p + 1)*S - 1] onto some [q*S, (q + 1)*S - 1] extent of target, which is some component. Such an extent in a target is called a striping unit of the layout. The layout mapping within a striping unit is increasing (this characterizes it uniquely);

- in addition to placing object data, striping layouts define the contents and placement of redundancy information, used to recreate lost data. Simplest example of redundancy information is given by RAID5 parity. In general, redundancy information is some form of check-sum over data;

- a striping unit to which cluster-wide object or component data are mapped is called a data unit;

- a striping unit to which redundancy information is mapped is called a parity unit (this standard term will be used even though redundancy information might be something different than parity);

- A striping layout belongs to a striping pattern (N+K)/G if it stores K parity units with redundancy information for every N data units and units are stored in G containers. Typically G is equal to the number of storage devices in the pool. Where G is not important or clear from the context, one talks about N+K striping pattern (which coincides with the standard RAID terminology);

- a parity group is a collection of data units and their parity units. We only consider layouts where data units of a parity group are contiguous in the source. We do consider layouts where units of a parity group are not contiguous in the target (parity declustering). Layouts of N+K pattern allow data in a parity group to be reconstructed when no more than K units of the parity group are missing;

- for completeness, it should be mentioned that meta-data objects have associated layouts, not considered by this specification. Meta-data object layouts are described in terms of meta-data keys (rather than byte offsets) and also based on redundancy, typically in the form of mirroring. Replicas in such mirrors are not necessary (and usually aren't) byte-wise copies of each other, but they are key-wise copies;

- components of a given cluster-wide object are normally located on the servers of the same pool. Sometimes, e.g., during migration, a cluster-wide object can have a more complex layout with components scattered across multiple pools;

- a layout is said to intersect (at the moment) with a storage device or a data server if it maps any data to the device or to any device currently attached to the server, respectively;

- a pool is a collection of storage, communication and computational resources (server nodes, storage devices and network interconnects) configured to provide IO services with certain fault-tolerance characteristics. Specifically, cluster-wide objects are stored in the pool with striping layouts with such striping patterns that guarantee that data are accessible after a certain number of server and storage device failures. Additionally, pools guarantee (by means of SNS repair described in this specification) that a failure is repaired in a certain time.

Examples of striping patterns:

- K = 0. RAID0, striping without redundancy;

- N = K = 1. RAID1, mirroring;

- K = 1 < N. RAID5;

- K = 2 < N. RAID6.

A cluster-wide object layout owns its target components. That is, no two cluster-wide objects store data or redundancy information in the same component object.

**************
Requirements
**************

These requirements are already listed in the SNS repair requirement analysis document [1]:

- [r.sns.repair.triggers] Repair can be triggered by a storage, network or node failure.

- [r.sns.repair.code-reuse] Repair of a network array and of a local array is done by the same code.

- [r.sns.repair.layout-update] Repair changes layouts as data are reconstructed.

- [r.sns.repair.client-io] Client IO continues as repair is going on.

- [r.sns.repair.io-copy-reuse] The following points of variability are implemented as policy applied to the same underlying IO copying and re-structuring mechanism:

  - do client writes target the same data that are being repaired or client writes are directed elsewhere?

  - does repair reconstruct only a sub-set of data (e.g., data missing due to a failure) or all data in the array?

That is, the following use cases are covered by the same IO restructuring mechanism:

+-------------------------------+---------------------------------------+--------------------------------------+
|                               |same layout                            |separate layouts                      |
+-------------------------------+---------------------------------------+--------------------------------------+
|missing data                   |in-place repair                        |NBA                                   |
+-------------------------------+---------------------------------------+--------------------------------------+
|all data                       |migration, replication                 |snapshot taking                       |
+-------------------------------+---------------------------------------+--------------------------------------+


Here "same layout" means that client IO continues to the source layouts while data restructuring is in progress and "separate layout" means that client IO is re-directed to a new layout at the moment when data restructuring starts.

"Missing" data means that only a portion of source data is copied into a target and "all data" means that all data in the source layouts are copied.

While data restructuring is in progress affected objects have composite layouts showing what parts of object linear name-space have already been re-structured. Due to the possibly on-going client IO against an object, such a composite layout can have a structure more complex than "old layout up to a certain point, new layout after".

- [r.sns.repair.priority] Containers can be assigned a repair priority specifying in what order they are to be repaired. This allows to restore critical cluster-wide objects (meta-data indices, cluster configuration data-base, etc.) quickly, decreasing the damage of a potential double failure.

- [r.sns.repair.degraded] Pool state machine is in degraded mode during repair. Individual layouts are moved out of degraded mode as they are reconstructed.

- [r.sns.repair.c4] Repair is controllable by an advanced C4 settings: can be paused, aborted, its IO priority can be changed. Repair reports its progress to C4.

- [r.sns.repair.addb] Repair should produce ADDB records of its actions.

- [r.sns.repair.device-oriented] Repair uses device-oriented repair algorithm, as described in [3].

- [r.sns.repair.failure.transient] Repair survives transient node and network failures.

- [r.sns.repair.failure.permanent] Repair handles permanent failures gracefully.

- [r.sns.repair.used-only] Repair should not reconstruct unused (free) parts of failed storage.

There are also some overall requirements from the Summary Requirement Table [2], but none of them are relevant to SNS repair.

Concurrency & priority
=======================

- To guarantee that sufficient fraction of system resource are used, we (i) guarantee that only a single repair can go on a given server pool and (ii) different pools do not compete for resources.

- Every container has a repair priority. A repair for failed container has the priority derived from the container.

Client I/O during SNS repair
=============================

- From client's point of view, the client I/O will be served while SNS repair is going on. Some performance degradation may be experienced, but this should not lead to starvation or indefinite delays.

- Client I/O to surviving containers or servers will be handled normally. But SNS repair agent will also read from or write to the containers while SNS repair is going on.

- Client I/O to failed container (or failed server) will be directed to proper container according to the new layout, or data will be served by retrieving from other containers and computing from parity/data unit. This depends on implementation options. we will discuss this later.

- When repair is completed, client I/O will restore to its normal performance.

Repair throttling
===================

SNS manager can throttle the repair according to system bandwidth, user control. This is done by dynamically changing the fraction of resource usage of individual repair or overall.

Repair logging
================

SNS repair will produce ADDB records about its operations and progress. These records include, but not limited to, {start, pause, resume, complete} of individual repair, failure of individual repair, progress of individual repair, throughput of individual repair, etc.

Device-oriented repair
============================

Agent iterates components over the affected container, or all containers which have surviving data/parity unit in the need-to-reconstruct parity group. These data/parity unit will be read and sent to proper agent where spare space lives, and used to re-compute the lost data.

SNS repair and layout
==========================

SNS manager gets an input set configuration and output set configuration as the repair initiated. These input/output set can be described by some form of layout. SNS repair will read data/parity from the devices described with the input set and reconstruct missing data. In the process of reconstruction object layouts affected by the data reconstruction (i.e., layouts with data located on the lost storage device or node) are transactionally updated to reflect changed data placement. Additionally, while the reconstruction is in progress, all affected layouts are switched into a degraded mode so that clients can continue to access and modify data.

Note that the standard mode of operation is a so called "non-blocking availability" (NBA) where after a failure a client can immediately continue writing new data without any IO degradation. To this end a client is handed out a new layout where it can write to. A cluster-wide object has, after this point, a composite layout: some parts of object linear name-space are laid accordingly to the old layout and other parts (ones where clients write to after a failure)—a new one. In this configuration, clients never write to the old layout, while its content is being reconstructed.

The situation where there is a client-originated IO against layouts being reconstructed is possible because of

- reads have to access old data even under NBA policy and

- non-repair reconstructions like migration or replication.


