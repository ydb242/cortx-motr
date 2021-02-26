==============================================================
High-Level Design of Function Shipping and In-Storage Compute 
==============================================================


This document presents a high-level design (HLD) of the function-shipping (in-storage compute) subsystem of Motr 

The main purposes of this document are: (i) to be inspected by Motr architects and peer designers to ascertain that high level design is aligned with Motr architecture and other designs, and contains no defects, (ii) to be a source of material for Active Reviews of Intermediate Design (ARID) and detailed level design (DLD) of the same component, (iii) to serve as a design reference document. 

The intended audience of this document consists of Motr customers, architects, designers and developers. 

*********************
Introduction
*********************

This document describes a framework for client nodes to offload computations (or “ship” them) on Motr-controlled storage nodes. In a traditional distributed file-system, the data moves to the computation. This document prescribes a method to move the computation to the data.  

This document also defines the programming model that shippable functions must use. 


*****************
Definitions
*****************

- function-shipping subsystem: the subsystem described in this document. 

- in-storage compute subsystem: another name for the function-shipping subsystem. 

- distributable function (or distributable computation): A function which conforms to a restricted model (defined below). This model allows the computation to be distributed and run (in parallel) on the storage cluster. [NOTE: this could also be called a "shippable computation". However, this term may be confusing as the actual shipping is done by the function registration mechanism. These functions are in fact not necessarily distributable.] 

- offloaded function (or offloaded computation): A distributable computation which is shipped and running on the storage cluster. In other words, an offloaded computation is a running instance of a distributable computation.  

Notations
----------

- We write [x] for a bag of x . 

- The empty bag is denoted [] .  

*******************
Requirements
*******************

**Horizontal scalability** 


A system features horizontal scalability if its performance increases linearly (or quasi-linearly) with the number of components. An ideal storage cluster should scale horizontally, that is, capacity and bandwidth should increase linearly with the number of nodes. However, the network bandwidth may not be scaled proportionally with the number of nodes. If the trends observed today continue, this situation will occur in exascale systems. In fact, the JASON group forecasts that due to this imbalance, in exascale systems it will only be possible to run applications that require a small working set size relative to computational volume [1]. 

Fortunately, many useful computations are data-local. We say that a computation is data-local when a portion of the input can be processed independently of the rest of the input1. Whenever a computation is data-local, it satisfies the criterion spelled out by the JASON group. Indeed, the working set can be made sufficiently small by processing only a sufficiently small portion of the input at a time. 

In fact, every data-local computation:

- Can be carried out locally, where the data is stored. Thus, the overhead due to data transfer can be minimized. 

- Can run in parallel, maximizing the total storage bandwidth and the usage of processing units.  


The primary function of the in-storage compute subsystem is to provide horizontal scalability by supporting distribution and execution of data-local computations in the storage cluster. Such scalability is a critical feature for a storage system aiming to reach the exascale. 

**Support for arbitrary layouts** 

The layout of data processed by offloaded functions is arbitrary. In other words, the physical location of the data (typically) does not match its logical structure. For example, even though a given data set may be divided in records, any given record may be split between two storage nodes. For some other data sets, records are big enough that it is beneficial to split them across several nodes to increase bandwidth. 

Supporting arbitrary layouts is critical for ‘fourth paradigm’ [2] scientific applications; namely the post-hoc analysis of experimental data. Indeed, by definition the analysis procedure is not known at the time of recording experimental data; thus the best layout of data on disk cannot be dictated by the needs of the analysis. 

**Support for writing outputs in place** 

In certain cases, the computation executed locally will not significantly decrease the amount of data to transfer on the network. Offloading such computations may make sense anyway if the output is to be written locally on each node (for example for processing in a subsequent phase). 

*******************************
Integration with HSM 
*******************************

Data transformation
---------------------

A specific use-case motivating the above requirement for writing outputs in place is when a function is applied to a file as it is being moved between tiers, for HSM purposes. Indeed, when the HSM subsystem triggers a copy, it may want to apply filtering of data, compression –– or any form of custom transformation. 

Informing HSM 
--------------

The data usage (or data usage patterns) made by the queries executed by the function-shipping subsystem should be made available to the HSM subsystem. To do so, the function-shipping subsystem should use the same API as other subsystem (e.g. regular I/O). [TODO: what should this API be? (It does not exist as of May 2016.)] 

Non requirement: taking advantage of SAGE layers 
-------------------------------------------------

Some functions may be more computationally intensive as others. As such, it may make sense to execute them on a 'low-level' tier (with less storage but higher computational capabilities). However, it is not the responsibility of the function-shipping subsystem to move the data. The data should be moved by other means (e.g. HSM, containers, ...) prior to running the computation on the storage cluster. 

************
Resilience
************

Components of the network storage may be brought down (and possibly brought back up) during operations, including during the execution of an offloaded function on a storage nodes. Such components include disks, nodes, network connections, etc. 

Accessing data via the function-shipping subsystem must offer the same level of resiliency as accessing it via regular I/O operations. That is: 

#. If any result is reported to the client or stored back to storage, the observed result must not be affected by failures that would not affect regular I/O. Note in particular that malfunctioning hardware may return data which is incorrect. A consequence of such a failure is that the reported data is wrong. Such hardware errors will compromise function-shipping correctness as well. [TODO: (Probably we do not want to have error correction?)] 

#. Furthermore, if a regular read operation would succeed on a given data set and a given set of failures, then a computation over the same data set and assuming the same set of failures should report a result. 

Additionally, when results are written back to storage, any change must be performed in a transaction. 

************
Security
************

Assuming that a node runs the function-shipping subsystems, it may be running application code. This application code cannot be trusted: whatever this code is, it must not be able to compromise the node in question (and a-fortiori of the rest of the storage cluster). Specifically, confidentiality, integrity and availability must not be compromised. 

****************
Confidentiality 
****************

The application code may access only the data relevant to the computation to be performed, namely the inputs provided by the application, either directly or indirectly, via the files that the offloaded function is intended to process. 

*****************
Integrity
*****************

The application code may not corrupt or otherwise modify the state of the node, except to the extent of reporting results. (Harmless side effects such as writing to debugging logs are of course allowed, to the extent that they do not otherwise compromise availability or confidentiality.) 

**************
Availability 
**************

Running the application code may only use up a reasonable amount of resources of the node, so that it continues to be available for other operations during the run of the offloaded function. Consequently, system policies may throttle the resource consumption of offloaded functions. The policies supported will be the same as those supported in general by Motr services. 

The security requirements cannot be fulfilled by solely relying on a time-consuming review process of application code by the storage cluster administration.  

The rationale is that some applications will require a short code-deploy-execute cycle. For example, when developing a new application, the programmer will typically repeat the following cycle of operations at short intervals: 

#. Write (or modify) a computation to offload 

#. Deploy the function on the storage cluster 

#. Run the application and realise it has problems which require to repeat the cycle. 

In such a scenario, the time to deploy a new function should be in the order of minutes (at most), while the code-review process may take of the order of days.

**********************
Resource release 
**********************

The run-time of the offloaded function may span indefinitely after the request is completed. That is, under normal conditions if the user cancels their request, all the currently running offloaded functions related to the request should be terminated and the corresponding resources freed. There may be an acceptable delay between the time of termination of the request and the completion of the termination. This delay corresponds to the amount of time necessary to propagate the termination message and run the necessary operations on each server. [TODO: what should be a reasonable (distribution) bound for this time?]. (This feature is particularly useful when a user mistakenly offloaded an extremely resource-hungry computation.) 


