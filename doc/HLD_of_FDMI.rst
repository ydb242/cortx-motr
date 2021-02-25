===========================
High-Level Design of FDMI
===========================

**************
Introduction
**************

The document is intended to specify the design for of Motr FDMI interface. FDMI is a part of Motr product. FDMI provides interface for Motr plugins and allows horizontally extending the features and capabilities of the system. The intended audience for this document are product architects, developers and QA engineers.

**************
Definitions
**************

- FDMI source

- FDMI plugin

- FDMI source dock

- FDMI plugin dock

- FDMI record

- FDMI record type

- FDMI filter

************
Overview
************

Motr is a storage core capable of deployment for a wide range of large scale storage regimes, from cloud and enterprise systems to exascale HPC installations. FDMI is a part of Motr core, providing interface for plugins implementation. FDMI is build around the core and allows for horizontally extending the features and capabilities of the system in a scalable and reliable manner.

*******************************************
fdmi position in overall motr core design
*******************************************

FDMI is an interface allowing Motr Core to scale horizontally. The scaling includes two aspects:

- Core expansion in aspect of adding core data processing abilities, including data volumes as well as transformation into alternative representation. The expansion is provided by introducing FDMI plug-ins.

- Initial design implies that FOL records are the only data plug-ins are able to process so far.

- Core expansion in aspect of adding new types of data the core is able to feed plug-ins. This sort of expansion is provided by introducing FDMI sources.

- Initial design implies that FOL record is the only source data type Motr Core provides so far.

FDMI plug-in is an application linked with Motr Core to make use of corresponding FDMI interfaces and runs as a part of Motr instance/service. This Motr instance may provide various capabilities (data storing, etc.) or may not. The purpose of introducing plug-in is getting notifications from Motr Core on events of interest for the plugin and further post-processing of the received events for producing some additional classes of service, the Core currently is not able to provide.

FDMI source is a part of Motr instance being linked with appropriate FDMI interfaces and allowing connection to additional data providers.

Considering the amount of data Motr Core operates with, it is obvious that a plug-in typically requires a sufficiently reduced bulk of data to be routed to it for post-processing. The reduction is provided by introduction of mechanism of subscription to particular data types and conditions met at runtime. The subscription mechanism is based on set of filters the plug-in registers in Motr Filter Database during its initialization.

Source in its turn refreshes its own subset of filters against the database. The subset is selected from overall filter set based on the knowledge about data types the source is able to feed FDMI with as well as operation with the data the source supports.

**************
FDMI roles
**************

FDMI consists of APIs implementing particular roles in accordance with FDMI use cases. The roles are:

- Plug-in dock, responsible for:

  - Plug-in registration in FDMI instance

  - Filter registration in Motr Filter Database

  - Listening to notifications coming over RPC

  - Payload processing

  - Self-diagnostic (TBD)

- Source dock (FDMI service), responsible for:

  - Source registration

  - Retrieving/refreshing filter set for the source

  - Input data filtration

  - Deciding on and posting notifications to filter subscribers over Mero RPC

  - Deferred input data release

  - Self-diagnostic (TBD)
  
  *********************
FDMI plug-in dock
*********************

Initialization
=================

Application starts with getting private FDMI plugin dock API allowing it start communicating with the dock.

Further initialization consists of registering a number of filters in filtered database. Every filter instance is given by plugin creator with a filter id unique across the whole system. On filter registration plugin dock checks filter semantics. If filter appears to be invalid, registration process stops.

NB
===

Although filter check is performed on registration it cannot be guaranteed that no error appears in run time during filter condition check. Criteria for filter

correctness will be defined later. If filter is treated as incorrect by FDMI source dock, corresponding ADDB record is posted and optionally HA is informed on it.

NB
====

TBD if we really need to determine the moment when all sources appear to be running filter sets consistent across the whole system. Currently we need to consider if Plug-in should be notified about this point

Data Processing
================

Remote FDMI instance running Source Dock role provides data payload via RPC channel. RPC sink calls back local FDMI instance running Plug-in Dock role. The latter resolves the filter id to plug-in callback, and calls the one passing the data to plug-in instance.

It may take some time for plug-in to do post-processing and decide if the fdmi record could be released. At the time plug-in instructs FDMI to notify corresponding source allowing particular fdmi record be released.

Filter “active” status
=======================

Filter “active” status is used for enabling/disabling this filter on the fly without removing it from filtered database. Upon filter active status change filtered notifies all the registered sources. If filter “active” status is set to false (filter is disabled), it is ignored by sources.

Application plugin may change filter “active” status by sending “enable filter” or “disable filter” command for the already registered filter:

- Filter “active“ status initial value is specified on filter registration

- To enable/disable a filter on the fly, application sends “enable filter” or “disable filter” request to filtered service. Filter ID is specified as a parameter.

De-initialization
====================

Plug-in initiates de-initialisation by calling local FDMI. The latter deregisters plug-in’s filter set with filterd service. After confirmation it deregisters the associated plug-in’s callback function.

All registered sources are notified about changes in filter set, if any occurred as the result of plug-in coming off.

*****************
fdmi source dock
*****************

Initialization
===================

FDMI Source dock does not need explicit registration in filtered. Each FDMI source dock on start requests filters list from filtered and stores it locally.

In order to notify FDMI source dock about ongoing changes in filter data set, Resource manager’s locks mechanism is used. Filters change notification: TBD. On read operation, FDMI source acquires Read lock for the filtered database. On filter metadata change, each instance holding read lock is being notified.

On receiving filter metadata change notification, FDMI source dock re-requests filter data set.

On receiving each new filter, FDMI source dock parses it, checks for its consistency and stores its in-memory representation suitable for calculations.

As an optimization, execution plan could be built for every added filter to be kept along with the one. As an option, execution plan can be built on every data filtering action to trade off memory consumption for CPU ticks.

Input Data Filtering
=======================

When input data identified by fdmi record id comes to Source, the latter calls local FDMI instance with the data. On data coming FDMI starts iterating through local filter set.

According to its in-memory representation (execution plan) each filter is traversed node by node, and for every node a predicate result is calculated by appropriate source callback.

NB
===

It is expected that source will be provided with operand definitions only. Inside the callback the source is going to extract corresponding operand according to the description passed in. And predicate result is calculated based on the extracted and transformed data.

Note how FDMI handles tree: all operations are evaluated by FDMI engine, and only getting atomic values from the fdmi record payload are delegated to Source.

When done with traversing, FDMI engine calculates final Boolean result for the filter tree, and makes a decision whether to put serialized input data onto RPC for the plug-in associated with the filter.

Deferred Input Data Release
============================

Input data may require to remain preserved in the Source until the moment when plug-in does not need it anymore. The preservation implies protection from being deleted/modified. The data processing inside plug-in is an asynchronous process in general, and plug-in is expected to notify corresponding source allowing it to release the data. The message comes from plug-in to FDMI instance hosting the corresponding source.

NB
===

TBD: We need to establish a way to resolve fdmi record identifier to FDMI instance hosting particular source. Most probably the identifier itself may contain the information, easily deduced or calculated.

FDMI service found dead
========================

When interaction between Motr services results in a timeout exceeding pre-configured value, the not responding service needs to be announced dead across the whole system. First of all confc client is notified by HA about the service not responding and announced dead. After being marked dead in confc cache, the service has to be reported by HA to filtered as well.

Interfaces
-----------

- FDMI service

- FDMI source registration

- FDMI source implementation guideline

- FDMI record

- FDMI record post

- FDMI source dock FOM

  - Normal workflow

  - FDMI source: filters set support

  - Corner cases (plugin node dead)

- FilterD

- FilterC

- FDMI plugin registration

- FDMI plugin dock FOM

- FDMI plugin implementation guideline

****************
fdmi service
****************

FDMI service runs as a part of Mero instance. FDMI service stores context data for both, FDMI source dock and FDMI plugin dock. FDMI service is initialized and started on Motr instance start up, FDMI Source dock and FDMI plugin dock are both initialised on the service start unconditionally.

*******************************
fdmi source registration
*******************************

FDMI source instance main task is to post FDMI records of a specific type to FDMI source dock for further analysis, Only 1 FDMI source instance with a specific type should be registered: FDMI record type uniquely identifies FDMI source instance. A list of FDMI record types:

- FOL record type

- ADDB record type

- TB

FDMI source instance provides the following interface functions for FDMI source dock to handle FDMI records:

- Test filter condition

- Increase/decrease record reference counter

- Xcode functions

On FDMI source registration all its internals are initialized and saved as FDMI generic source context data. Pointer to FDMI source instance is passed to FDMI source dock and saved in a list. In its turn, FDMI source dock provides back to FDMI source instance an interface function to perform FDMI record posting. FDMI generic source context stores the following:

- FDMI record type

- FDMI generic source interface

- FDMI source dock interface

****************************************
fdmi source implementation guideline
****************************************

FDMI source implementation depends on data domain. Specific FDMI source type stores

- FDMI generic source interface

- FDMI specific source context data (source private data)

For the moment FDMI FOL source is implemented as the 1st (and currently the only) FDMI source. FDMI FOL source provides ability for detailed FOL data analysis. Based on generic FOL record knowledge, “test filter condition” function implemented by FOL source checks FOP data: FOL operation code and pointer to FOL record specific data.

For FOL record specific data handling FDMI FOL record type is declared and registered for each specific FOL record type (example: write operation FOL record, set attributes FOL record, etc.)

FDMI FOL record type context stores the following:

- FOL operation code

- FOL record interface

FOL record interface functions are aware of particular FOL record structure and provide basic primitives to access data:

- Check condition

On FDMI FOL record type FDMI record registration all its internals are initialized and saved as FDMI FOL record context data. Pointer to FDMI FOL record type is stored as a list in FDMI specific source context data.

********************
fdmi record post
********************

Source starts with local locking data to be fed to FDMI interface, then it calls posting FDMI API. On FDMI side a new FDMI record (data container) is created along with new record id, and posted data gets packed into the record. The record is queued for further processing to FDMI FOM queue, and FDMI record id is returned to Source.

To be able to process further calling back from FDMI regarding particular data, i.e. original record, Source is responsible for establishing unambiguous relation between returned FDMI record ID and original record identifier, whatever it look like.

NB
===

Please note, the Source is responsible for initial record locking (incrementing ref counter), but FDMI is responsible for further record release.
