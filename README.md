## sflex - Flexible GPU Management for SLURM Workload Manager

- **Sflex** is designed to enhance the performance of workload managers that schedule GPU-based workloads in clustered environments.
  
- The primary focus is to build scheduling mechanism for workload managers to achieve **higher throughput** and **reducing job response times** to schedule GPU workloads.

- By improving these aspects, **sflex** aims to significantly increase the efficiency and effectiveness of GPU resource management in high-performance computing setups.

## sflex features and contributions

- **Utilization aware scheduling:** Picks least loaded compute node by looking at real-time utilization stats of GPUs.
- **MPS based Overprovisioning:** Uses Nvidia's Multi-Process Service feature to multiplex GPU at process granularity. Does overprovisiong for better utilization of GPU resources.

## Prerequisites ##
- The compute nodes with GPUs should have working CUDA (11 or higher). Compute nodes use NVML library of CUDA to relay realtime GPU stats to controller.
- Required SLURM plugins: *gres/mps, select/cons_tres*. These plugins are part of source code. Successfull SLURM build should load these plugins.

## Steps to build sflex from source ##
The code changes to SLURM were made at both controller and compute node binaries. For modularity SLURM code of controller and compute node are kept in separate repositories. For ease both changes can be merged into single SLURM package(which is not done in this repo).
1. Clone the **controller_slurmcode** repo at the controller node where **slurmctld** will be launched.  
    ```sh
   git clone https://github.com/s4nd33p-p/sflex/tree/main/controller_slurmcode

2. Clone the **computenode_slurmcode** repo at the compute node where **slurmd** daemon will be launched.
    ```sh
   git clone https://github.com/s4nd33p-p/sflex/tree/main/computenode_slurmcode

The remaining steps are common for both controller and compute node build which is a standard SLURM build from source.

Following repo has the SLURM build script to build from source by installing required dependencies. Kindly follow the steps accodrdingly.
```sh
   git clone https://github.com/NISP-GmbH/SLURM
```

For reference config files used for sflex specific to SLURM have been added at **slex_sourcecode/config_files**.


