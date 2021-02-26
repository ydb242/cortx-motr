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

It is assumed that all state machines are endowed with a common set of features that won't be specifically mentioned:

- integration with ADDB 1[u.machine.addb]. State transitions and distribution of times spent in particular states are logged to the ADDB;

- persistency 2[u.machine.persistency]. State machines use services of local and distributed transaction managers to recover from node failures. After a restart, persistent state machine receives a restart event, that it can use to recover its lost volatile state;

- discoverability 3[u.machine.discoverability]. State machines can be discovered by the management tool;

- queuing 4[u.machine.queuing]. A state machine has a queue of incoming requests.

Copy machine initialization
===============================

A copy machine parameters are:

- an input set description. An input set consists of data and meta-data that the copy machine has to re-structure. Examples of input set description are:

  - data in a given container;

  - data in all cluster-wide objects, having a component in a given container;

  - data on a given storage device;

  - data in all cluster-wide objects, having a component in a container on a given storage device;

  - data on a given data server;

  - data in all cluster-wide objects, having a component in a container residing on a given data server;

  - data in a client or proxy volatile or persistent cache;

  - data from the operation records in a given segment of FOL;

  - data from all files in a given file-set.

Examples above are for data input sets specifying sources of data reconstruction process. Similarly, meta-data reconstruction uses copy machine operating on a meta-data input set. Meta-data input set describes a collection of meta-data in one or more meta-data containers:

- a collection of meta-data records in a certain meta-data table;

- a collection of meta-data records stored on a certain storage device;

- a collection of meta-data records pertaining to operations on objects in a given file-set;

- a collection of meta-data records in a certain segment of FOL, etc.

Present specification deals mostly with data reconstruction.

It is possible, for a given input set description, to efficiently estimate at what storage devices and at what servers the data from the set are located at the moment. A copy machine uses this information to start agents. The estimation must be conservative: it must include all the servers and devices where data from the set are, but may also include some other servers and devices. The assumption here is that estimation can be made simpler and cheaper at the expense of creating extra agents that will do nothing. The "at the moment" qualification of estimation is for the possibility of container migration. There are multiple possible strategies to deal with the latter:

- "lock" the container as part of estimation, so that migration is not possible while a copy machine uses the container;

- implement container migration as a 2-phase protocol 5 [u.container.migration.vote] where container users (in this case a copy machine) are first asked a permission to move a container;

- implement agents as part of container, migrated together with the later. When migration of a container completes, all its agents get migration event 6 [u.container.migration.call-back]. This is basically the same solution as the previous one, but implemented as part of a container framework.

In the case where a container is migrated by physically moving a storage device around, migration cannot be prevented and any solution of migration problem must deal with this (possibly by failing the copy machine). It is easy to see that an input set description is, essentially, a layout [4] of some kind. Its difference from a more typical file layout is that a file layout domain is a linear file offset name-space, whereas input set layout domain can be more complex: it addresses contents of multiple objects and also their redundancy information.

Input set specifies how data can be found in a certain container or a set of containers. In a typical case of SNS repair, input set specifies how data can be found in the device container (i.e., on a storage device). Other possibilities include input set referring to data containers or objects. These cases capture the cases where input data are compressed, encrypted or de-duplicated;

- priority assignment. A priority is assigned to every container in the input set, determining the order of restructuring;

- an output set description. Similarly to the input set description, an output set description indicates where re-structured data will be stored at. Examples of output set descriptions are:

  - a container;

  - a distributed spare space of a pool;

  - a file or file-set;

All the notes about input set descriptions apply to the output set descriptions.

- an aggregation function. An aggregation function maps input set description layout onto output set description layout domain. Examples of possible aggregation functions are:

  - identity function (for replication, migration, backup);

  - map striping units of a striping group in the input set layout to a striping unit in the output set layout (for repair);

  - map a byte extent of a file in a client data cache onto an equally sized extent in one of the file's component objects;

  - function mapping a block of input set data to its hash (de-duplication).
  
  Aggregation function specifies which parts of input set have to be accumulated before output data can be produced. Hence, aggregation function defines the restructuring data-flow and graph of agent-to-agent connections. A set of input parts that aggregation function maps to the same place in the output set is called an aggregation group. For example, striping units of the same parity group form aggregation group for repair, all blocks sharing the same de-duplication hash value form aggregation group for de-duplication;

- a transformation function. A transformation function changes aggregated data or meta-data before they are placed into output set. Examples of possible transformation functions are:

  - identity function (for replication, migration, backup);

  - XOR or higher Reed-Solomon codes for N+K striping layout repair;

  - compression or encryption algorithm.

Transformation function produces output data from a collection of all input data that aggregate function accumulates at a given point of output set layout domain. All transformations that we consider can be calculated by applying some function to each new portion of input set as it arrives, that is, it is never necessary to buffer all aggregated input data before calculating output.

An important optimisation is an opportunistic aggregation and transformation of data. For example, when a parity group having multiple units on the same server is reconstructed, XOR of these units can be calculated by the collecting agent on the server, without sending individual units over the network.

- resource consumption thresholds:

  - memory;

  - processor cycles;

  - storage bandwidth;

  - network bandwidth;

Resource thresholds can be adjusted dynamically.

- a set of call-backs that copy machine calls at certain conditions:

  - progress notifications:

  - enter: called when the copy machine starts re-structuring a part of its input set;

  - leave: called when the copy machine has completed re-structuring a part of its input set.

   A copy machine invokes notification call-backs at different granularities: when it starts or stops processing

  - the whole input set;

  - on a server;

  - on a storage device;

  - in a container;

  - a layout;

  - an aggregation group.

Copy machine operation
=======================

To describe the operation of a copy machine, the following definitions are convenient:

- copy packet: a chunk of input data traversing through the copy machine. A copy packet is created by a storage-in agent or a collecting agent and encapsulates information necessary to route the data through the machine's agents;

- next-agent(packet, agent): a function returning for a copy packet identity of the agent that has to process this packet next after the given agent. next-agent function determines the routing of copy packets through the copy machine. next-agent function is determined by the aggregation function; 

- aggregation-group(P) is an aggregation group to which packet P belongs;

- queue(P, A) is a function adding copy packet P to the incoming queue of next-agent(P, A), assuming that this queue is local, i.e., that A and next-agent(P, A) are located on the same node.

Persistent state
--------------------

A copy machine persistent state records processed parts of input set. In a typical case where input set description is defined as a meta-data iterator 7 [u.md.iterator] of some kind (like it is in all the examples given above), persistent copy machine state consists of extents in the iterator position space 8 [u.md.iterator.position]. Multiple disjoint processed extents can appear in input set because of the out-of-order processing of user IO requests. A natural persistent state structure is a B-tree of such extents 9 [u.TREE.SCHEMA.ADD] ST, keyed by starting position of an extent, with merge on insertion and split on deletion done as part of transaction undo. Note: or maybe without merging to avoid the complications of undo action failing. End note.

Each storage device, participating in data restructuring, holds its own copy of copy machine persistent state. This provides a natural level of copy machine fault tolerance, assuring that restructuring can proceed as long as data to restructure are available.

Resource management
----------------------

A copy machine deals with distributed resource consumption problem. For example, limitations on a fraction of storage device throughput that the copy machine can consume constrain the rate at which copy packets can be sent to the node. Such constraints are addressed by the generic M0 resource management infrastructure 10 [u.resource.generic]. Server nodes declare 11 [u.resource.declare] resources (memory, disk bandwidth, processor bandwidth) allocated for the particular copy machine instance. Other nodes grab 12 [u.resource.grab] portions of declared resources and cache 13 [u.RESOURCE.CACHEABLE] ST resources to submit copy packets.

Transactions
----------------

Restructuring of an aggregation group must be a distributed transaction. To achieve this, a copy packet P is tagged with a distributed transaction identifier which is a function of aggregation-group(P), so that all packets of the same group have the same transaction identifiers 14 [u.dtm.tid.generate]. Every agent processing the packet, places transaction record in FOL 15 [u.fol.record.custom] as usual part of transaction processing. In a typical scenario, where packet pipe-line contains only a single storage-out agent, redo-only transactions can be used.

FOL records of packet processing are pruned from the FOL as usual. On the undo phase of recovery, FOL is replayed and each node undoes local part of copy packet processing. On the redo recovery, an agent redoes packet processing, including forwarding the packet to other nodes, if versions match.

Copy machine call-backs are executed as part of the same transaction as the rest of packet processing.

- "component level locking" is achieved by taking lock on an extent of object data on the same server where these data are located; 

- time-stamp based optimistic concurrency control 

Independently of whether a cluster-wide object level locking model 16 [u.dlm.logical-locking], where data are protected by locks taken on cluster-wide object (these can be either extent locks taken in cluster-wide object byte offset name-space 17 [u.IO.EXTENT-LOCKING] ST or "whole-file" locks 18 [u.IO.MD-LOCKING] ST, or component level locking model, or time-stamping model is used, locks or time-stamps are served by a potentially replicated locking service running on a set of lock servers (a set that might be equal to the set of servers in the pool). The standard locking protocol as used by the file system clients would imply that all locks or time-stamps necessary for an aggregation group processing must be acquired before any processing can be done. This implies a high degree of synchronization between agents processing copy packets from the same aggregation group.

Fortunately, this ordering requirement can be weakened by making every agent to take (the same) required lock and assuming that lock manager recognizes, by comparing transaction identifiers, that lock requests from different agents are part of the same transaction and, hence, are not in conflict 19 [u.dlm.transaction-based].

Pool Machine
--------------

Pool machine is a replicated state machine [5], having replicas on all pool nodes. Each replica maintains the following state:

node : array of struct { id : node identity,

state : enum state };

device : array of struct { id : device identity,

state : enum state };

read-version : integer;

write-version : integer;

where state is enum { ONLINE, FAILED, OFFLINE, RECOVERING }. It is assumed that there is a function device-node() mapping device identity to the index in node[] corresponding to the node the device is currently attached to. The elements of the device[] array corresponding to devices attached to non-ONLINE nodes are effectively undefined (state transition function does not depend on them). To avoid mentioning this condition in the following, it is assumed that

device-node(device[i].id).state == ONLINE,

for any index i in device[] array, that is, devices attached to non-ONLINE nodes are excised from the state.

State transitions of a pool machine happen when the state is changed on a quorum 37 [u.quorum.consensus] of replicas. To describe state transitions the following derived state (that is not necessary actually stored on replicas) is introduced:

nr-nodes : number of elements in node[] array

nr-devices : number of elements in device[] array

nodes-in-state[S] : number of elements in node[] array with the state field equal to S

devices-in-state[S] : number of elements in device[] array with the state field equal to S

nodes-missing = nr-nodes - nodes-in-state[ONLINE]

devices-missing = nr-devices - devices-in-state[ONLINE]

In addition to the state described above, a pool is equipped with a "constant" (in the sense that its modifications are beyond the scope of the present design specification) configuration state including:

- max-node-failures : integer, a number of node failures that the pool tolerates;

- max-device-failures : integer, a number of storage device failures that the pool tolerates.

A pool is said to be a dud (Data possibly Unavailable or Damaged) when more device and node failed in it than the pool is configured to tolerate.

A pool state with nodes-missing = n and devices-missing = k is said to belong to a state class S(n, k), for example, any normal state belongs to the class S(0,0).

As part of changing its state, a pool machine interacts with external entities such as layout manager or client caches. During this interaction multiple failures, delays and concurrent pool machine state transitions might happen. In general it is impossible to guarantee that all external state will be updated by the time the pool machine reaches its target state. To deal with this, pool state contains a version vector, some components of which are increased on any state transition. All external requests to the pool (specifically, IO requests) are tagged with the version vector of the pool state the request issuer knows about. The pool rejects requests with incompatibly stale versions, forcing issuer to re-new its knowledge of the pool state. Separate read and write versions are used to avoid unnecessary rejections. E.g., read requests are not invalidated by adding a new device or a new server to the pool.

Server state machine
=======================

Persistent server state consists of its copy of the pool state.

On boot a server contacts a quorum of pool servers (counting itself) and updates its copy of the pool state. If recovery is necessary (unclean shutdown, server state as returned by the quorum is not OFFLINE), the server changes the pool state (through the quorum) to register that it is recovering. After the recovery of distributed transactions completes, the server changes the pool state to indicate that the server is now in ONLINE state (which must have been the server's pre-recovery state). See details in the State section.

Scalable IO

As described in the Logical specification, at the heart of SNS repair design is a copy machine: a mechanism for scalable data re-structuring. The same copy machine code can be used for variety of purposes, some described in certain detail in this document. One particularly important use of copy machine outside of SNS repair proper is a "normal" client IO.

In the simplest case, a client performs a non-cached write operation on a file. A very simple copy machine consisting of a single user-in agent reading data from the user space, a network-out agent sending data to the servers and a network-in and storage-out agents on the servers can be used. The input set description in this case consists of a single buffer in the user application address space or iovec vector describing a collection of such buffers. Output set description is similarly given by a extent or a vector of extents in the file layout. An obvious generalisation of this is a copy machine with multiple user-in agents copying segments of an input buffer in parallel. This design can utilise throughput of multiple processor cores at the expense of weakening POSIX failure semantics. The same applies for file read, with user-out agent(s) copying data to the user space.

Similar copy machines can be employed for cached IO, except that input set description is done in terms of local file layout (based on page cache indexing).

An alternative construction of a copy machine for cached IO is one with an input set description made directly in terms of cached data pages. This copy machine would be able to form copy packets containing data from multiple objects and multiple files. To unify this model with earlier described layout based input set descriptions, a client allocates a number of local containers and populates them with cached data. A copy machine sends out parts of these containers. With this approach, multi-object cached IO and container migration become instances of the same generic data re-structuring process, sharing the copy machine infrastructure.

An important advantage of copy machine based IO is a flexibility in data routing. The description of SNS repair above mentioned the possibility of partial aggregation of data on intermediate servers. In the case of client IO, the same mechanism makes it possible to route data through proxy nodes, including proxy servers and peer-to-peer clients. For example, an owning client can be selected for every parity group in a file with IO requests to this group being routed through this client. In addition to utilising client to client network bandwidth, this allows makes client to server operations more efficient because they would more likely be full-stripe ones.

**************
Conformance
**************

- [i.sns.repair.triggers] A pool machine registers with health layer its interest in hearing about device, node and network failures. When health layer notifies the pool machine about a failure, state transition happens44 and repair, if necessary, is triggered.

- [i.sns.repair.code-reuse] Local RAID repair is a special case of general repair. When a storage device fails in a way that requires only local repair, the pool machine records this failure as in general case and creates a copy engine to handle the repair. All agents of this machine are operating on the same node.

- [i.sns.repair.layout-update] When a pool state machine enters a non-normal state, it changes its version numbers. Clients attempting to do IO on layouts tagged with old version numbers, would have to re-fetch the pool state. Optionally, requests layout manager to proactively revoke all layouts intersecting with the failed device or node. Optionally, use copy machine "enter layout" progress call-back to revoke a particular layout. As part of re-fetching layouts, clients learn updated list of alive nodes and devices. This list is a parameter to layout. The layout IO engine uses this parameter to do IO in degraded mode47.

- [i.sns.repair.client-io] Client IO continues as repair is going on. This is achieved by re-directing clients to degraded layouts, so that clients collaborate with the copy machine in repair. After copy machine notifies pool machine of processing progress (through the "leave" progress call-back), repaired parts of layout48 are upgraded; 

- [i.sns.repair.priority] Containers can be assigned a repair priority specifying in what order they are to be repaired. Prioritization is part of the storage-in agent logic.

- [i.sns.repair.degraded] Pool state machine is in degraded mode during repair: described in the pool machine logical specification. Individual layouts are moved out of degraded mode as they are reconstructed: when copy machine is done with all components of a layout, it signals layout manager that layout can be upgraded (either lazily or by revoking all degraded layouts).

- [i.sns.repair.c4]

   - Repair is controllable by advanced C4 settings: can be paused, its IO priority can be changed. This is guaranteed by dynamically adjustable copy machine resource consumption thresholds. 

   - Repair reports its progress to C4. This is guaranteed by the standard state machine functionality. 

- [i.sns.repair.addb] Repair should produce ADDB records of its actions: this is a part of standard state machine functionality.

- [i.sns.repair.device-oriented] Repair uses device-oriented repair algorithm, as described in On-line Data reconstruction in Redundant Disk Arrays dissertation: this follows from the storage-in agent processing logic.

- [i.sns.repair.failure.transient] Repair survives transient node and network failures. After failed node restarts or network partitions heals, distributed transactions, including repair transactions created by copy machine are redone or undone to restore consistency. Due to the construction of repair transactions, recovery also restores repair to a consistent state, from which it can resume.

- [i.sns.repair.failure.permanent] Repair handles permanent failures gracefully. Repair updates file layouts with at the transaction boundary. Together with copy machine state replication, described in the Persistent state sub-section, this guarantees that repair can continue in the face of multiple failures.

- [i.sns.repair.used-only] Repair should not reconstruct unused (free) parts of failed storage: this is a property of a container based repair design.

********************
Dependencies
********************

- layouts

  - [r.layout.intersects]: it must be possible to efficiently find all layouts intersecting with a given server or a given storage device;

  - [r.layout.parameter.dead]: a list of failed servers and devices is a parameter to a layout formula;

  - [r.layout.degraded-mode]: layout IO engine does degraded mode IO if directed to do so by the layout parameters;

  - [r.layout.lazy-invalidation]: layout can be invalidated lazily, on a next IO request;

- DTM

  - [r.fol.record.custom] : custom FOL record type, with user defined redo and undo actions can be defined;

  - [r.dtm.intercept]: it is possible to execute additional actions in the context of a user-level transaction;

  - [r.dtm.tid.generate]: transaction identifiers can be assigned by DTM users;

- management tool

- RPC

  - [r.rpc.maximal.bulk-size]

  - [r.network.utilization]: an interface to estimate network utilization;

  - [r.rpc.pluggable]: it is possible to register a call-back to be called by the RPC layer to process a particular RPC type;

  - health and liveness layer:

  - [r.health.interest], [r.health.node], [r.health.device], [r.health.network] it is possible to register interest in certain failure event types (network, node, storage device) for certain system components (e.g., all nodes in a pool);

  - [r.health.call-back] liveness layer invokes a call-back when an event on interest happens;

  - [r.health.fault-tolerance] liveness layer is fault-tolerant. Call-back invocation is carried through the node and network failures;

  - [r.rpc.streaming.bandwidth]: optimally streamed RPCs can utilize at least 95% of raw network bandwidth;

  - [r.rpc.async]: there is an asynchronous RPC sending interface;

- DLM

  - [r.dlm.enqueue.async]: a lock can be enqueued asynchronously;

  - [r.dlm.logical-locking]: locks are taken on cluster-wide objects;

  - [r.dlm.transaction-based]: lock requests issued on behalf of transactions. Lock requests made on behalf of the same transaction are never in conflict;

- meta-data:

  - [u.md.iterator]: generic meta-data iterators, suitable for input set description;

  - [u.md.iterator.position]: meta-data iterators come with a totally ordered space of possible iteration positions;

- state machines:

  - [r.machine.addb]: state machines report statistics about their state transitions to ADDB;

  - [r.machine.persistence]: state machine can be made persistent and recoverable. Local transaction manager invokes restart event on persistent state machines after node reboots;

  - [r.machine.discoverability]: state machines can be discovered by c4;

  - [r.machine.queuing]: a state machine has a queue of incoming requests;

- containers:

  - [r.container.enumerate]: it is possible to efficiently iterate through the containers stored (at the moment) on a given storage device;

  - [r.container.migration.call-back]: a container notifies interested parties in its migration events;

  - [r.container.migration.vote]: container migration, if possible, includes a voting phase, giving interested parties an opportunity to prepare for the future migration;

  - [r.container.offset-order]: container offset order matches underlying storage device block ordering enough to make container offset ordered transfers optimal;

  - [r.container.read-ahead]: container do read-ahead;

  - [r.container.streaming.bandwidth]: large-chunk streaming container IO can utilize at least 95% of raw storage device throughput;

  - [r.container.async]: there is an asynchronous container IO interface;

- storage:

  - [r.storage.utilization]: an interface to measure a utilization a given device for a certain time period;

  - [r.storage.transfer-size]: an interface to determine maximal efficient request size of a given storage device;

  - [r.storage.intercept]: it should be possible to intercept IO requests targeting a given storage device;

- SNS:

  - [r.sns.trusted-client] (constraint): only trusted clients can operate on SNS objects;

- miscellaneous:

  - [r.processor.utilization]: an interface to measure processor utilization for a certain time period;

- resource management:

  - [r.resource.generic]: resource management infrastructure is generic enough to implement copy machine resource limits;

- quorum:

  - [r.quorum.consensus]: quorum based consensus mechanism is needed;

  - [r.quorum.read]: read access to quorum decisions is needed;
  
 ***************
Security Model
***************

Network
=======

It is assumed that messages exchanged over the network are signed so that a message sender can be established reliably. Under this condition, nodes cannot impersonate each other.

Servers
========

Present design provides very little protection against a compromised server. While compromised storage-in or network agents can be detected by using striping redundancy information, there is no way to independently validate the output of a collecting agent or check that storage-out agent in fact wrote the right data to the storage. This issue is, in general, unavoidable, as long as output set can be non-redundant.

Deeper problem is due to servers collaborating in distributed transactions. Given that transaction identifiers used by the copy machine are generated by a known method, a server can check that server-to-server requests it receives are from well-formed transactions and a malicious server cannot wreak chaos by initiating malformed transactions. What is harder to counter is a server not sending requests that it must send according to the copying algorithm. We assume that the worst thing that can happen when a server delays or omits certain messages is that corresponding transaction will eventually be aborted and undone. Unresponsive server is evicted from the cluster and pool handles this as a server failure. This still doesn't guarantee progress, because the server might immediately re-join the cluster only to sabotage more transactions.

Clients
=========

The fundamental difference between a server and a client is that the latter cannot, in general, be replicated, because it runs arbitrary code outside of M0 control. While well-formedness of client supplied transactions and client liveness can be checked with some effort, there is no obvious way to verify that a client calculates redundancy information correctly, without sacrificing system performance to a considerable degree. It is, hence, posited that SNS operations, including client interaction with repair machinery, can originate only from the trusted clients.

Others
=======

SNS repair interacts with and depends on variety of core distributed M0 services including liveness layer, lock servers, distributed transaction manager and management tool. Security concerns for such services should be addressed generically and are beyond the scope of the present design.

Issues
=======
It is in no way clear that the analysis above is any close to exhaustive. A formal security model is required.

************
Refinement
************

- copy machine

  - input set

    - estimate servers

    - estimate devices

    - efficiently enumerate all containers an input set has on a given device

    - input set vs. container migration

    - efficient check of whether given container contains data from an input set

    - efficiently enumerate all data extents common between an input set and a container

    - processed extents of an input set can be efficiently and compactly coded by an agent

  - agents creation is inexpensive

  - resource thresholds can be adjusted dynamically, agents should be notified about new thresholds





