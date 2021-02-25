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
