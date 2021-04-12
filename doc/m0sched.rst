This document contains information about m0scheduler python3 script. 

**m0scheduler** responsible for,

- getting **m0sched** parameters from Hare
- runnning **m0sched** and attaches to it's stdin and stdout
- getting **m0sched** stdout and parses JSONs produced by **m0sched**
- caching all kv pairs received from **m0sched** in memory and deduplication newly received kv pairs
- sending signals SIGINT and SIGTERM to **m0sched** and waits for its termination.

***************
Build
***************

- Setup **rpm** based motr/hare cluster with help of Hare User Guide. Ref - https://github.com/Seagate/cortx-hare/blob/c0ff88671c16c49de1cca81f6d06af180113e72b/README.md

- Once you setup **rpm** based motr/hare cluster, m0scheduler script will be available for use. **m0scheduler** source location: fdmi/plugins/m0scheduler. 

***************
Execution
***************

- **m0scheduler** executes **m0sched** internally by passing required params as local ep, ha ep, profile fid and process fid, script will figure out required m0sched input params based on cluster setup.  (**Note:** script always pass first m0d instance ep and fid to m0sched as a local ep and process fid) 

::

    m0sched -i
    m0sched -?



- execute **m0scheduler**, it will wait for fdmi source data to receive.

::

    m0scheduler


- execute **m0crate** with dix put and get workload config file

    ::

    mcrate -S /tmp/m0crate-index.yaml

