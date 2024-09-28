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
  
