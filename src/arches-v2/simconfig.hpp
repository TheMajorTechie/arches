#pragma once
#include <string>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace Arches
{
    namespace TRaX
    {

        typedef Units::UnitDRAMRamulator UnitDRAM;
		typedef Units::UnitCache UnitL2Cache;
		typedef Units::UnitCache UnitL1Cache;
		typedef rtm::FTB PrimBlocks;
		typedef Units::TRaX::UnitRTCore<rtm::CWBVH::Node, PrimBlocks> UnitRTCore;

        struct CoreSimConfig
        {
            // compute
            double core_clock = 2235.0e6;
            double dram_clock = 5250.0e6;
            uint64_t stack_size = 512;
            uint num_tms = 128;
            uint num_tps = 128;
            uint num_threads = 12;

            // Memory
            uint64_t block_size = CACHE_BLOCK_SIZE;
            uint num_partitions = 12;
            uint partition_stride = 1 << 12;

            // DRAM
            UnitDRAM::Configuration dram_config;

            // L2$
            UnitL2Cache::Configuration l2_config;
            UnitL2Cache::PowerConfig l2_power_config;

            // Crossbar
            Units::UnitCrossbar::Configuration xbar_config;

            // L1d$
            UnitL1Cache::Configuration l1d_config;
            UnitL1Cache::PowerConfig l1d_power_config;
            UnitRTCore::Configuration rtc_config;

            // texture units because apparently that's something that is used like once
            Units::UnitTexture::Configuration tu_config;

            /** Load the configuration from a plain text file line by line */
            static CoreSimConfig load_config_from_file(
                const std::string &config_file_path,
                const std::string &project_folder_path)
            {
                CoreSimConfig config;
                
                config.core_clock = 2235.0e6;
                config.dram_clock = 5250.0e6;
                config.stack_size = 512;
                config.num_tms = 128;
                config.num_tps = 128;
                config.num_threads = 12;

                config.block_size = CACHE_BLOCK_SIZE;
                config.num_partitions = 12;
                config.partition_stride = 1 << 12;

                config.l2_config.level = 2;
                config.l2_config.block_size = CACHE_BLOCK_SIZE;
                config.l2_config.block_prefetch = true;
                config.l2_config.miss_alloc = true;
                config.l2_config.size = 6 << 20;
                config.l2_config.associativity = 16;
                config.l2_config.policy = Units::UnitCacheBase::Policy::LRU;
                config.l2_config.num_slices = 6;
                config.l2_config.num_mshr = 256;
                config.l2_config.num_subentries = 4;
                config.l2_config.latency = 187;

                config.l1d_config.level = 1;
                config.l1d_config.block_size = CACHE_BLOCK_SIZE;
                config.l1d_config.miss_alloc = true;
                config.l1d_config.size = 128 << 10;
                config.l1d_config.associativity = 32;
                config.l1d_config.policy = Units::UnitCacheBase::Policy::LRU;
                config.l1d_config.num_banks = 8;
                config.l1d_config.num_mshr = 512;
                config.l1d_config.num_subentries = 16;
                config.l1d_config.latency = 39;

                config.rtc_config.max_rays = 64;
                config.rtc_config.num_cache_ports = 4;

                config.l2_config.policy = Units::UnitCacheBase::Policy::LRU;
                config.l1d_config.policy = Units::UnitCacheBase::Policy::LRU;

                config.block_size = CACHE_BLOCK_SIZE; 
                config.l2_config.block_size = CACHE_BLOCK_SIZE;
                config.l1d_config.block_size = CACHE_BLOCK_SIZE;

                std::ifstream file(config_file_path);

                // check to see if we can open the file
                if (!file.is_open())
                {
                    std::cerr << "[CoreSimConfig] Failed to open Simulator Configuration text file at: "
                              << config_file_path << "\nDefaulting to RTX 4090 simulation.\n";

                    config.dram_config.config_path = project_folder_path + "build\\src\\arches-v2\\config-files\\gddr6x_21000_config.yaml";
                    config.dram_config.size = 1ull << 30; // 1GB per partition
                    config.dram_config.clock_ratio = config.dram_clock / config.core_clock;
                    config.dram_config.latency = 254;

                    config.l2_config.level = 2;
                    config.l2_config.block_prefetch = true;
                    config.l2_config.miss_alloc = true;
                    config.l2_config.size = 6 << 20;
                    config.l2_config.associativity = 16;
                    config.l2_config.policy = Units::UnitCacheBase::Policy::LRU;
                    config.l2_config.num_slices = 6;
                    config.l2_config.crossbar_width = config.l2_config.num_slices;
                    config.l2_config.num_mshr = 256;
                    config.l2_config.num_subentries = 4;
                    config.l2_config.latency = 187;

                    config.xbar_config.num_slices = config.l2_config.num_slices;
                    config.xbar_config.slice_stride = config.l2_config.block_size;
                    config.xbar_config.num_partitions = config.num_partitions;
                    config.xbar_config.partition_stride = config.partition_stride;

                    config.l1d_config.level = 1;
                    config.l1d_config.miss_alloc = true;
                    config.l1d_config.size = 128 << 10;
                    config.l1d_config.associativity = 32;
                    config.l1d_config.policy = Units::UnitCacheBase::Policy::LRU;
                    config.l1d_config.num_banks = 8;
                    config.l1d_config.crossbar_width = config.l1d_config.num_banks;
                    config.l1d_config.num_mshr = 512;
                    config.l1d_config.num_subentries = 16;
                    config.l1d_config.latency = 39;

                    config.rtc_config.max_rays = 64;
                    config.rtc_config.num_cache_ports = 4;

                    return config;
                }

                // start reading in the config line by line
                std::string line;
                while (std::getline(file, line))
                {
                    // some sanitization and the ability to handle comments in the config
                    line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
                    if (line.empty() || line[0] == '#')
                        continue;

                    // set up the delimiter. if there is none then just continue and skip the line
                    size_t delimiter_pos = line.find('=');
                    if (delimiter_pos == std::string::npos)
                        continue;

                    std::string key = line.substr(0, delimiter_pos);
                    std::string value = line.substr(delimiter_pos + 1);
                    std::stringstream value_stream(value); // for streaming reads and writes

                    // compute
                    if (key == "core_clock")
                        value_stream >> config.core_clock;
                    else if (key == "dram_clock")
                        value_stream >> config.dram_clock;
                    else if (key == "stack_size")
                        value_stream >> config.stack_size;
                    else if (key == "num_tms")
                        value_stream >> config.num_tms;
                    else if (key == "num_tps")
                        value_stream >> config.num_tps;
                    else if (key == "num_threads")
                        value_stream >> config.num_threads;

                    // memory
                    else if (key == "block_size")
                        value_stream >> config.block_size;
                    else if (key == "num_partitions")
                        value_stream >> config.num_partitions;
                    else if (key == "partition_stride")
                    {
                        std::string val;
                        std::getline(value_stream, val);

                        // gotta do some string processing again
                        val.erase(std::remove_if(val.begin(), val.end(), ::isspace), val.end());
                        size_t shifting_pos = val.find('<');

                        if (shifting_pos != std::string::npos)
                        {
                            std::string strlval = val.substr(0, shifting_pos);
                            std::string strrval = val.substr(shifting_pos + 2);

                            int lval = strtol(strlval.c_str(), NULL, 10);
                            int rval = strtol(strrval.c_str(), NULL, 10);

                            config.partition_stride = static_cast<uint64_t>(lval) << rval;
                        }

                        else
                            config.partition_stride = strtol(val.c_str(), NULL, 10);
                    }

                    // DRAM
                    else if (key == "dram.config_path")
                    {
                        std::string path;
                        // Extract the raw remainder of the stream string all at once
                        std::getline(value_stream, path);

                        // Clean structural artifacts out of the path safely
                        path.erase(std::remove(path.begin(), path.end(), '\"'), path.end());
                        path.erase(std::remove(path.begin(), path.end(), '\r'), path.end());
                        path.erase(std::remove(path.begin(), path.end(), '\n'), path.end());

                        config.dram_config.config_path = project_folder_path + path;

                        // Verify the memory address points to valid text strings
                        std::cout << "[Debug Check] DRAM Path resolved to: " << config.dram_config.config_path << std::endl;
                    }
                    else if (key == "dram.size")
                        value_stream >> config.dram_config.size;
                    else if (key == "dram.clock_ratio") // dummy
                        config.dram_config.clock_ratio = config.dram_clock / config.core_clock;
                    else if (key == "dram.latency")
                        value_stream >> config.dram_config.latency;

                    // L2$
                    else if (key == "l2.level")
                        value_stream >> config.l2_config.level;
                    else if (key == "l2.block_prefetch")
                    {
                        std::string val;
                        value_stream >> val;
                        config.l2_config.block_prefetch = (val == "true" || val == "1");
                    }
                    else if (key == "l2.miss_alloc")
                    {
                        std::string val;
                        value_stream >> val;
                        config.l2_config.miss_alloc = (val == "true" || val == "1");
                    }
                    else if (key == "l2.size")
                    {
                        std::string val;
                        std::getline(value_stream, val);

                        // gotta do some string processing again
                        val.erase(std::remove_if(val.begin(), val.end(), ::isspace), val.end());
                        size_t shifting_pos = val.find('<');

                        if (shifting_pos != std::string::npos)
                        {
                            std::string strlval = val.substr(0, shifting_pos);
                            std::string strrval = val.substr(shifting_pos + 2);

                            int lval = strtol(strlval.c_str(), NULL, 10);
                            int rval = strtol(strrval.c_str(), NULL, 10);

                            config.l2_config.size = static_cast<uint64_t>(lval) << rval;
                        }

                        else
                            config.l2_config.size = strtol(val.c_str(), NULL, 10);
                    }
                    else if (key == "l2.associativity")
                        value_stream >> config.l2_config.associativity;
                    else if (key == "l2.policy")
                    {
                        std::string policy_str;
                        value_stream >> policy_str;
                        if (policy_str == "LRU")
                            config.l2_config.policy = Units::UnitCacheBase::Policy::LRU;
                        else if (policy_str == "LRU_RANDOM")
                            config.l2_config.policy = Units::UnitCacheBase::Policy::LRU_RANDOM;
                        else if (policy_str == "FIFO")
                            config.l2_config.policy = Units::UnitCacheBase::Policy::FIFO;
                        else if (policy_str == "FIFO_RANDOM")
                            config.l2_config.policy = Units::UnitCacheBase::Policy::FIFO_RANDOM;
                        else
                            config.l2_config.policy = Units::UnitCacheBase::Policy::LRU;
                    }
                    else if (key == "l2.num_slices")
                        value_stream >> config.l2_config.num_slices;
                    else if (key == "l2.crossbar_width") // doesn't actually do anything
                        config.l2_config.crossbar_width = config.l2_config.num_slices;
                    else if (key == "l2.num_mshr")
                        value_stream >> config.l2_config.num_mshr;
                    else if (key == "l2.num_subentries")
                        value_stream >> config.l2_config.num_subentries;
                    else if (key == "l2.latency")
                        value_stream >> config.l2_config.latency;

                    // crossbar config stuff is all dummy
                    else if (key == "xbar.num_slices")
                        config.xbar_config.num_slices = config.l2_config.num_slices;
                    else if (key == "xbar.slice_stride")
                        config.xbar_config.slice_stride = config.l2_config.block_size;
                    else if (key == "xbar.num_partitions")
                        config.xbar_config.num_partitions = config.num_partitions;
                    else if (key == "xbar.partition_stride")
                        config.xbar_config.partition_stride = config.partition_stride;

                    // L1D$
                    else if (key == "l1.level")
                        value_stream >> config.l1d_config.level;
                    else if (key == "l1.miss_alloc")
                    {
                        std::string val;
                        value_stream >> val;
                        config.l1d_config.miss_alloc = (val == "true" || val == "1");
                    }
                    else if (key == "l1.size")
                    {
                        std::string val;
                        std::getline(value_stream, val);

                        // gotta do some string processing again
                        val.erase(std::remove_if(val.begin(), val.end(), ::isspace), val.end());
                        size_t shifting_pos = val.find('<');

                        if (shifting_pos != std::string::npos)
                        {
                            std::string strlval = val.substr(0, shifting_pos);
                            std::string strrval = val.substr(shifting_pos + 2);

                            int lval = strtol(strlval.c_str(), NULL, 10);
                            int rval = strtol(strrval.c_str(), NULL, 10);

                            config.l1d_config.size = static_cast<uint64_t>(lval) << rval;
                        }

                        else
                            config.l1d_config.size = strtol(val.c_str(), NULL, 10);
                    }
                    else if (key == "l1.associativity")
                        value_stream >> config.l1d_config.associativity;
                    else if (key == "l1.policy")
                    {
                        std::string policy_str;
                        value_stream >> policy_str;
                        if (policy_str == "LRU")
                            config.l1d_config.policy = Units::UnitCacheBase::Policy::LRU;
                        else if (policy_str == "LRU_RANDOM")
                            config.l1d_config.policy = Units::UnitCacheBase::Policy::LRU_RANDOM;
                        else if (policy_str == "FIFO")
                            config.l1d_config.policy = Units::UnitCacheBase::Policy::FIFO;
                        else if (policy_str == "FIFO_RANDOM")
                            config.l1d_config.policy = Units::UnitCacheBase::Policy::FIFO_RANDOM;
                        else
                            config.l1d_config.policy = Units::UnitCacheBase::Policy::LRU;
                    }
                    else if (key == "l1.num_banks")
                        value_stream >> config.l1d_config.num_banks;
                    else if (key == "l1.crossbar_width")
                        config.l1d_config.crossbar_width = config.l1d_config.num_banks;
                    else if (key == "l1.num_mshr")
                        value_stream >> config.l1d_config.num_mshr;
                    else if (key == "l1.num_subentries")
                        value_stream >> config.l1d_config.num_subentries;
                    else if (key == "l1.latency")
                        value_stream >> config.l1d_config.latency;

                    // rtc config
                    else if (key == "rtc.max_rays")
                        value_stream >> config.rtc_config.max_rays;
                    else if (key == "rtc.num_cache_ports")
                        value_stream >> config.rtc_config.num_cache_ports;
                    else
                    {
                        std::cout << "[Parser Warning] Key found in text file but SKIPPED by code: [" 
                                  << key << "] with value [" << value << "]" << std::endl;
                    }
                }

                // load default DRAM config
                std::cout << "DRAM config path: " << config.dram_config.config_path << std::endl;

                if (config.dram_config.config_path.empty())
                {
                    printf("No DRAM config loaded. Setting default path...\n");
                    config.dram_config.config_path = project_folder_path + "build/src/arches-v2/config-files/gddr6_14000_config.yaml";
                }
                config.dram_config.clock_ratio = config.dram_clock / config.core_clock;

                // L2 Dependent Layout
                config.l2_config.crossbar_width = config.l2_config.num_slices;

                // Crossbar Interconnect Layout Matrix
                config.xbar_config.num_slices = config.l2_config.num_slices;
                config.xbar_config.slice_stride = config.l2_config.block_size;
                config.xbar_config.num_partitions = config.num_partitions;
                config.xbar_config.partition_stride = config.partition_stride;

                // L1D Dependent Layout
                config.l1d_config.crossbar_width = config.l1d_config.num_banks;

                return config;
            }
        };
    }
}