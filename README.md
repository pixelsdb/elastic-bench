# Elastic-Bench: 云数据仓库测试基准

一个基于CAB（Cloud Analytics Benchmark）方法改进的云数据仓库性能测试与预测工具。

## 概述

传统的基准测试如TPC-H和TPC-DS无法捕捉云数据仓库工作负载的独特特征。实际云环境中的工作负载表现出时间变化模式、突发性和间歇性使用等特点，这些在传统基准测试中并未得到反映。

本项目在CAB基准测试（VLDB 2023）的基础上进行了扩展和改进：

1. 支持更灵活的工作负载模式生成
2. 增加基于扫描字节的资源建模能力
3. 提供用于预测任务的历史负载数据
4. 改进查询生成过程，更好地匹配目标模式
5. 允许组合工作负载模式，更好地反映实际使用情况

## 工作负载模式

Elastic-Bench支持五种基本工作负载模式，这些模式是通过分析Snowset和Redset数据集得出的：

1. **正弦噪声模式（Sinusoidal Noise Pattern）** - 创建一个基础负载，并在其上添加多个正弦波峰和随机噪声
2. **随机峰值模式（Random Spikes Pattern）** - 创建随机的较短峰值，具有高可变性
3. **突发模式（Burst Pattern）** - 在随机位置创建一个持续的高负载区域
4. **负载断路器模式（Load Breaker Pattern）** - 创建一个基础负载，在一个区域突然增加负载，同时在另一个区域抑制负载
5. **小时级峰值模式（Hourly Spikes Pattern）** - 创建均匀分布的周期性峰值（模拟24小时周期）

这些模式可以单独使用，也可以组合使用以创建更复杂、更真实的工作负载。

## 主要改进

与原始CAB基准相比：

1. **明确模式的单一查询流**：不使用多个随机模式，而是允许开发者指定确切的测试模式或组合。
2. **减少基础负载**：将模式1、4和5中的基础负载减少到原来的1/20，以允许更明显的负载变化。
3. **更好的负载拟合**：改进查询选择算法，更好地匹配目标负载模式。
4. **混合模式支持**：允许在单一查询流中组合多种简单模式，创造更真实的工作负载。
5. **字节模式**：添加基于scanned_bytes而非仅基于CPU时间的模式拟合支持。
6. **历史负载数据**：提供历史负载数据（测试期间持续时间的10倍），以支持负载预测开发。
7. **灵活参数**：通过命令行选项公开重要参数。

## 使用方法

### 构建

```bash
# 克隆仓库
git clone https://github.com/pixelsdb/elastic-bench.git
cd benchmark

# 构建代码
g++ -o benchmark *.cpp -std=c++11
```

### 运行

```bash
./benchmark -p <pattern_ids> -s <scale> -c <hours/scanedbytes> -t <duration> [-mode bytes]
```
#### 参数

- `-p`：模式ID（1-5，可以指定多个，用空格分隔）
- `-s`：数据大小的缩放因子
- `-c`：CPU小时数或扫描字节预算（取决于模式）
- `-t`：持续时间（小时）
- `-mode bytes`：可选参数，使用字节模式而非CPU时间模式

#### 示例

```bash
# 生成具有模式1、数据集大小为10GB、总cputime为24小时和8小时持续时间的查询流
./benchmark -p 1 -s 10 -c 24 -t 8

# 生成具有组合模式2和5、数据集大小为100GB、总scanned_bytes为50GB和4小时持续时间的查询流
./benchmark -p 2 5 -s 100 -c 50 -t 4 -mode bytes
```

### 输出文件

基准测试生成以下文件：

- `query_stream_p{pattern}_s{scale}_{mode}{budget}_t{duration}.json`：主查询流文件
- `query_stream_p{pattern}_s{scale}_{mode}{budget}_t{duration}_slot_info_{N}.json`：每个时间槽的历史负载分布文件（用于预测任务），其中cputime单位为10e-6s，scanned_bytes单位为字节


## 参考

- Cloud Analytics Benchmark (CAB), VLDB 2023
- [Snowset](https://github.com/resource-disaggregation/snowset) - Snowflake工作负载数据集
- [Redset](https://github.com/amazon-science/redset) - Amazon Redshift工作负载数据集
