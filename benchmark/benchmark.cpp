#include "database.h"

#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <cassert>
#include <sys/stat.h>
#include <cstring>
#include <tuple>
#include <ctime>
#include <iomanip>  
#include <vector>
using namespace std;
unsigned myseed = time(nullptr); 

struct Vector {
    static void AddSinusHead(vector<double> &scales, double intensity, double start_ratio, double width) {
       double a = 3.1415 / (scales.size() * width);
       double b = 3.1415 * ((start_ratio + (0.5 * (1 - width))) / width);
 
       for (uint32_t i = 0; i < scales.size(); i++) {
          double x = i * a - b;
          if (0 < x && x < 3.1415) {
             double val = sin(x) * intensity;
             if (val > 0) {
                scales[i] += val;
             }
          }
       }
    }
    
    static void AddRandomNoise(vector<double> &scales, mt19937 &gen, double intensity, double likelihood) {
       uniform_real_distribution dist(0.0, 1.0);
       for (double &scale : scales) {
          if (dist(gen) <= likelihood) {
             auto fac = dist(gen);
             scale += intensity * fac;
             if (scale < 0) {
                scale = 0;
             }
          }
       }
    }
    
    static void AddSequence(vector<double> &scales, double intensity, double start_ratio, double length_ratio) {
       uint32_t start = start_ratio * scales.size();
       uint32_t length = length_ratio * scales.size();
       for (uint32_t pos = start; pos < start + length; pos++) {
          double &scale = scales[pos % scales.size()];
          scale += intensity;
          if (scale < 0) {
             scale = 0;
          }
       }
    }
 
    static void OnOffPattern(vector<double> &scales, mt19937 &gen, double intensity, uint32_t spike_count, double length) {
       double spike_width = double(scales.size()) / spike_count;
       for (uint32_t spike_idx = 0; spike_idx < spike_count; spike_idx++) {
          uint32_t spike_start = spike_idx * spike_width;
          uint32_t spike_end = spike_start + spike_width * length;
          for (uint32_t pos = spike_start; pos < spike_end; pos++) {
             double &scale = scales[pos % scales.size()];
             scale += intensity;
             if (scale < 0) {
                scale = 0;
             }
          }
       }
    }
    
    static void OnOffPatternNoise(vector<double> &scales, mt19937 &gen, double intensity, uint32_t spike_count, double length) {
       uniform_real_distribution dist(0.0, 1.0);
       double spike_width = double(scales.size()) / spike_count;
       for (uint32_t spike_idx = 0; spike_idx < spike_count; spike_idx++) {
          uint32_t spike_start = spike_idx * spike_width;
          uint32_t spike_end = spike_start + spike_width * length;
          double spike_height = dist(gen);
          for (uint32_t pos = spike_start; pos < spike_end; pos++) {
             double &scale = scales[pos % scales.size()];
             scale += intensity * spike_height;
             if (scale < 0) {
                scale = 0;
             }
          }
       }
    }
 
    static void AddSequenceRandomNoise(vector<double> &scales, mt19937 &gen, double intensity, double start_ratio, double length_ratio) {
       uniform_real_distribution dist(0.0, 1.0);
       uint32_t start = start_ratio * scales.size();
       uint32_t length = length_ratio * scales.size();
       for (uint32_t pos = start; pos < start + length; pos++) {
          double &scale = scales[pos % scales.size()];
          scale += intensity * dist(gen);
          if (scale < 0) {
             scale = 0;
          }
       }
    }
 
    static void AddRandomWalk(vector<double> &scales, mt19937 &gen, double intensity, double start_ratio, double length_ratio) {
       uniform_real_distribution dist(0.0, 1.0);
       uint32_t start = start_ratio * scales.size();
       uint32_t length = length_ratio * scales.size();
       double diff = 0.1 * intensity;
       double state = dist(gen) * intensity;
       for (uint32_t pos = start; pos < start + length; pos++) {
          if (dist(gen) >= 0.5) {
             state += diff;
          } else {
             state -= diff;
          }
          double &scale = scales[pos % scales.size()];
          scale += state;
          if (scale < 0) {
             scale = 0;
          }
          if (scale > intensity) {
             scale = intensity;
          }
       }
    }
    
    static vector<uint64_t> ToCpuTime(vector<double> &scales, uint64_t total_cpu_time) {
       double sum = Sum(scales);
       assert(sum > 0);
 
       vector<uint64_t> cpu_time_in_slot(scales.size());
       for (uint32_t idx = 0; idx < scales.size(); idx++) {
          cpu_time_in_slot[idx] = total_cpu_time * scales[idx] / sum;
       }
       return cpu_time_in_slot;
    }
    
    static double Sum(vector<double> &scales) {
       double sum = 0.0;
       for (double scale : scales) {
          assert(scale >= 0);
          sum += scale;
       }
       return sum;
    }
 };
 

 struct TpchQueries {
    struct Arguments {
       inline constexpr static std::array<const char *, 5> regions = {"AFRICA", "AMERICA", "ASIA", "EUROPE", "MIDDLE EAST"};
       inline constexpr static std::array<int32_t, 25> nation_to_region = {0, 1, 1, 1, 4, 0, 3, 3, 2, 2, 4, 4, 2, 4, 0, 0, 0, 1, 2, 3, 4, 2, 3, 3, 1};
       inline constexpr static std::array<const char *, 25> nations = {"ALGERIA", "ARGENTINA", "BRAZIL", "CANADA", "EGYPT", "ETHIOPIA", "FRANCE", "GERMANY", "INDIA", "INDONESIA", "IRAN", "IRAQ", "JAPAN", "JORDAN", "KENYA", "MOROCCO", "MOZAMBIQUE", "PERU", "CHINA", "ROMANIA", "SAUDI ARABIA", "VIETNAM", "RUSSIA", "UNITED KINGDOM", "UNITED STATES"};
       
       struct Type {
          inline constexpr static std::array<const char *, 6> syllable1 = {"STANDARD", "SMALL", "MEDIUM", "LARGE", "ECONOMY", "PROMO"};
          inline constexpr static std::array<const char *, 5> syllable2 = {"ANODIZED", "BURNISHED", "PLATED", "POLISHED", "BRUSHED"};
          inline constexpr static std::array<const char *, 5> syllable3 = {"TIN", "NICKEL", "BRASS", "STEEL", "COPPER"};
       };
       
       struct Container {
          inline constexpr static std::array<const char *, 5> syllable1 = {"SM", "LG", "MED", "JUMBO", "WRAP"};
          inline constexpr static std::array<const char *, 8> syllable2 = {"CASE", "BOX", "BAG", "JAR", "PKG", "PACK", "CAN", "DRUM"};
       };
       
       inline constexpr static std::array<const char *, 5> segments = {"AUTOMOBILE", "BUILDING", "FURNITURE", "MACHINERY", "HOUSEHOLD"};
       inline constexpr static std::array<const char *, 92> colors = {"almond", "antique", "aquamarine", "azure", "beige", "bisque", "black", "blanched", "blue", "blush", "brown", "burlywood", "burnished", "chartreuse", "chiffon", "chocolate", "coral", "cornflower", "cornsilk", "cream", "cyan", "dark", "deep", "dim", "dodger", "drab", "firebrick", "floral", "forest", "frosted", "gainsboro", "ghost", "goldenrod", "green", "grey", "honeydew", "hot", "indian", "ivory", "khaki", "lace", "lavender", "lawn", "lemon", "light", "lime", "linen", "magenta", "maroon", "medium", "metallic", "midnight", "mint", "misty", "moccasin", "navajo", "navy", "olive", "orange", "orchid", "pale", "papaya", "peach", "peru", "pink", "plum", "powder", "puff", "purple", "red", "rose", "rosy", "royal", "saddle", "salmon", "sandy", "seashell", "sienna", "sky", "slate", "smoke", "snow", "spring", "steel", "tan", "thistle", "tomato", "turquoise", "violet", "wheat", "white", "yellow"};
       inline constexpr static std::array<const char *, 7> mode = {"REG AIR", "AIR", "RAIL", "SHIP", "TRUCK", "MAIL", "FOB"};
    };
    
    struct UpdateState {
       uint32_t update_count;
       uint32_t rotation;
       uint32_t update;
 
       explicit UpdateState(uint32_t update_count)
               : update_count(update_count)
                 , rotation(0)
                 , update(update_count) {}
       
       std::vector<std::string> GenerateNext(uint64_t scale_factor) {
          const uint64_t order_count = scale_factor * 1500000;
          const uint64_t max_order_key = 1 + order_count * 4;
          const uint64_t group_count = 1 + max_order_key / 32;
          const uint64_t first_group = (group_count * (update - 1)) / update_count;
          const uint64_t last_group = (group_count * update) / update_count;
          assert(group_count >= update_count);
 
          std::vector<std::string> result;
          result.push_back(to_string(first_group * 32));
          result.push_back(to_string(last_group * 32));
          result.push_back(to_string(rotation * 8));
          result.push_back(to_string((rotation + 1) * 8 - 1));
 
          update--;
          if (update == 0) {
             update = update_count;
             rotation = (rotation + 1) % 4;
          }
          return result;
       }
    };
 
    static string TwoDigitNumber(uint32_t num) {
       return (num < 10 ? "0" : "") + to_string(num);
    }
 
    static string QStr(const string &str) {
       return "\"" + str + "\"";
    }
 
    static vector<string> GenerateQueryArguments(int query_id, uint64_t scale_factor, mt19937 &gen, UpdateState &update_state) {
       switch (query_id) {
          case 1: {
             uniform_int_distribution<int32_t> dist1(60, 120);
             return vector<string>{to_string(dist1(gen))};
          }
          case 2: {
             uniform_int_distribution<int32_t> dist1(1, 50);
             uniform_int_distribution<int32_t> dist2(0, Arguments::Type::syllable3.size() - 1);
             uniform_int_distribution<int32_t> dist3(0, Arguments::regions.size() - 1);
             return vector<string>{to_string(dist1(gen)), QStr(Arguments::Type::syllable3[dist2(gen)]), QStr(Arguments::regions[dist3(gen)])};
          }
          case 3: {
             uniform_int_distribution<int32_t> segment(0, Arguments::segments.size() - 1);
             uniform_int_distribution<int32_t> day(1, 31);
             return vector<string>{QStr(Arguments::segments[segment(gen)]), QStr("1995-03-" + TwoDigitNumber(day(gen)))};
          }
          case 4: {
             uniform_int_distribution<int32_t> dist1(0, 57);
             uint32_t month_offset = dist1(gen);
             uint32_t year = 3 + month_offset / 12;
             uint32_t month = 1 + month_offset % 12;
             return vector<string>{QStr("199" + to_string(year) + "-" + TwoDigitNumber(month) + "-01")};
          }
          case 5: {
             uniform_int_distribution<int32_t> region(0, Arguments::regions.size() - 1);
             uniform_int_distribution<int32_t> year(1993, 1997);
             return vector<string>{QStr(Arguments::regions[region(gen)]), QStr(to_string(year(gen)) + "-01-01")};
          }
          case 6: {
             uniform_int_distribution<int32_t> date(1993, 1997);
             uniform_int_distribution<int32_t> discount(2, 9);
             uniform_int_distribution<int32_t> quantity(24, 25);
             return vector<string>{QStr(to_string(date(gen)) + "-01-01"), to_string(discount(gen)), to_string(quantity(gen))};
          }
          case 7: {
             uniform_int_distribution<int32_t> nation(0, Arguments::nations.size() - 1);
             return vector<string>{QStr(Arguments::nations[nation(gen)]), QStr(Arguments::nations[nation(gen)])};
          }
          case 8: {
             uniform_int_distribution<int32_t> nation(0, Arguments::nations.size() - 1);
             int32_t selected_nation_idx = nation(gen);
             uniform_int_distribution<int32_t> type1(0, Arguments::Type::syllable1.size() - 1);
             uniform_int_distribution<int32_t> type2(0, Arguments::Type::syllable2.size() - 1);
             uniform_int_distribution<int32_t> type3(0, Arguments::Type::syllable3.size() - 1);
             return vector<string>{QStr(Arguments::nations[selected_nation_idx]), QStr(Arguments::regions[Arguments::nation_to_region[selected_nation_idx]]),
                     QStr(string(Arguments::Type::syllable1[type1(gen)]) + " " + Arguments::Type::syllable2[type2(gen)] + " " + Arguments::Type::syllable3[type3(gen)])};
          }
          case 9: {
             uniform_int_distribution<int32_t> color(0, Arguments::colors.size() - 1);
             return vector<string>{QStr(Arguments::colors[color(gen)])};
          }
          case 10: {
             uniform_int_distribution<int32_t> dist1(1, 24);
             uint32_t month_offset = dist1(gen);
             uint32_t year = 3 + month_offset / 12;
             uint32_t month = 1 + month_offset % 12;
             return vector<string>{QStr("199" + to_string(year) + "-" + TwoDigitNumber(month) + "-01")};
          }
          case 11: {
             uniform_int_distribution<int32_t> nation(0, Arguments::nations.size() - 1);
             return vector<string>{QStr(Arguments::nations[nation(gen)]), to_string(scale_factor)};
          }
          case 12: {
             uint32_t mode_1 = 0;
             uint32_t mode_2 = 0;
             uniform_int_distribution<int32_t> mode(0, Arguments::mode.size() - 1);
             while (mode_1 == mode_2) {
                mode_1 = mode(gen);
                mode_2 = mode(gen);
             }
             uniform_int_distribution<int32_t> year(1993, 1997);
             return vector<string>{QStr(Arguments::mode[mode_1]), QStr(Arguments::mode[mode_2]), QStr(to_string(year(gen)) + "-01-01")};
          }
          case 13: {
             constexpr static std::array<const char *, 4> word_1 = {"special", "pending", "unusual", "express"};
             constexpr static std::array<const char *, 4> word_2 = {"packages", "requests", "accounts", "deposits"};
             uniform_int_distribution<int32_t> word(0, 3);
             return vector<string>{QStr(word_1[word(gen)]), QStr(word_2[word(gen)])};
          }
          case 14: {
             uniform_int_distribution<int32_t> dist1(0, 59);
             uint32_t month_offset = dist1(gen);
             uint32_t year = 3 + month_offset / 12;
             uint32_t month = 1 + month_offset % 12;
             return vector<string>{QStr("199" + to_string(year) + "-" + TwoDigitNumber(month) + "-01")};
          }
          case 15: {
             uniform_int_distribution<int32_t> dist1(0, 57);
             uint32_t month_offset = dist1(gen);
             uint32_t year = 3 + month_offset / 12;
             uint32_t month = 1 + month_offset % 12;
             return vector<string>{QStr("199" + to_string(year) + "-" + TwoDigitNumber(month) + "-01")};
          }
          case 16: {
             vector<string> result;
             uniform_int_distribution<int32_t> brand(1, 5);
             result.push_back(QStr("Brand#" + to_string(brand(gen)) + to_string(brand(gen))));
             uniform_int_distribution<int32_t> type1(0, Arguments::Type::syllable1.size() - 1);
             uniform_int_distribution<int32_t> type2(0, Arguments::Type::syllable2.size() - 1);
             result.push_back(QStr(Arguments::Type::syllable1[type1(gen)] + string(" ") + Arguments::Type::syllable2[type2(gen)]));
             unordered_set<int32_t> generated_sizes;
             uniform_int_distribution<int32_t> size_dist(1, 50);
             while (generated_sizes.size() != 8) {
                int32_t size = size_dist(gen);
                if (generated_sizes.count(size) == 0) {
                   generated_sizes.insert(size);
                   result.push_back(to_string(size));
                }
             }
             return result;
          }
          case 17: {
             uniform_int_distribution<int32_t> brand(1, 5);
             uniform_int_distribution<int32_t> container1(0, Arguments::Container::syllable1.size() - 1);
             uniform_int_distribution<int32_t> container2(0, Arguments::Container::syllable2.size() - 1);
             return vector<string>{QStr("Brand#" + to_string(brand(gen)) + to_string(brand(gen))),
                     QStr(Arguments::Container::syllable1[container1(gen)] + string(" ") + Arguments::Container::syllable2[container2(gen)])};
          }
          case 18: {
             uniform_int_distribution<int32_t> quantity(312, 315);
             return vector<string>{to_string(quantity(gen))};
          }
          case 19: {
             vector<string> result;
             uniform_int_distribution<int32_t> brand(1, 5);
             result.push_back(QStr("Brand#" + to_string(brand(gen)) + to_string(brand(gen))));
             result.push_back(QStr("Brand#" + to_string(brand(gen)) + to_string(brand(gen))));
             result.push_back(QStr("Brand#" + to_string(brand(gen)) + to_string(brand(gen))));
             uniform_int_distribution<int32_t> quantity1(1, 10);
             uniform_int_distribution<int32_t> quantity2(10, 20);
             uniform_int_distribution<int32_t> quantity3(20, 30);
             result.push_back(to_string(quantity1(gen)));
             result.push_back(to_string(quantity2(gen)));
             result.push_back(to_string(quantity3(gen)));
             return result;
          }
          case 20: {
             uniform_int_distribution<int32_t> color(0, Arguments::colors.size() - 1);
             uniform_int_distribution<int32_t> year(1993, 1997);
             uniform_int_distribution<int32_t> nation(0, Arguments::nations.size() - 1);
             return vector<string>{QStr(Arguments::colors[color(gen)]), QStr(to_string(year(gen)) + "-01-01"), QStr(Arguments::nations[nation(gen)])};
          }
          case 21: {
             uniform_int_distribution<int32_t> nation(0, Arguments::nations.size() - 1);
             return vector<string>{QStr(Arguments::nations[nation(gen)])};
          }
          case 22: {
             vector<string> result;
             unordered_set<int32_t> generated_country_codes;
             uniform_int_distribution<int32_t> nation_dist(0, Arguments::nations.size() - 1);
             while (generated_country_codes.size() != 7) {
                int32_t country_code = nation_dist(gen) + 10;
                if (generated_country_codes.count(country_code) == 0) {
                   generated_country_codes.insert(country_code);
                   result.push_back(to_string(country_code));
                }
             }
             return result;
          }
          case 23: {
             return update_state.GenerateNext(scale_factor);
          }
          default: {
             throw runtime_error("Invalid query ID");
          }
       }
    }
 };

struct Generator {
    static uint64_t GetSeedForPatterns() { return myseed; }
    static uint64_t GetSeedForQueries() { return myseed; }
    static uint64_t GetSeedForQueryArguments() { return myseed; }
    bool use_bytes_mode = false; // 默认使用 cputime 模式
 
    vector<Database> databases;

    void GenerateFixedDatabases(uint64_t database_count, uint64_t scale_factor, uint64_t budget, bool use_bytes) {
        this->use_bytes_mode = use_bytes;
        
        this->databases.resize(database_count);
        for (uint64_t idx = 0; idx < database_count; idx++) {
           this->databases[idx].database_id = idx;
           this->databases[idx].scale_factor = scale_factor;
           
           if (use_bytes_mode) {
              // 例如 budget 以 GB 为单位
              this->databases[idx].scanned_bytes = budget * 1024 * 1024 * 1024;
              this->databases[idx].cpu_time = 0; // 或保持为零
           } else {
              // 原来的逻辑，budget 以小时为单位
              this->databases[idx].cpu_time = budget * 3600e6;
              this->databases[idx].scanned_bytes = 0; // 或保持为零
           }
        }
     }

    void GenerateQueryArrivalDistribution(const vector<int>& pattern_ids, double intensity) {
        mt19937 gen(GetSeedForPatterns());
        uniform_real_distribution<double> rdist(0.0, 1.0);
    
        // 定义所有基础模式
        const vector<tuple<int, string, function<void(vector<double>&, mt19937&, uniform_real_distribution<double>&)>>> patterns = {
            {1, "Sinusoidal Noise", [](auto& scales, auto& gen, auto& rdist) {
                Vector::AddSequence(scales, 0.01, 0, 1.0);
                for (uint32_t i = 0; i < 8; i++) {
                    Vector::AddSinusHead(scales, 0.5, 
                        0.6 * rdist(gen) - 0.3, 
                        0.05 + rdist(gen) * 0.05);
                }
                Vector::AddRandomNoise(scales, gen, 1.0, 0.1 * rdist(gen));
            }},
            {2, "Random Spikes", [](auto& scales, auto& gen, auto& rdist) {
                exponential_distribution<double> exp_dist(0.5);
                uint32_t iterations = round(exp_dist(gen) * 2 + 1);
                for (uint32_t i = 0; i < iterations; i++) {
                    Vector::AddRandomWalk(scales, gen, 1.0, 
                        0.1 + rdist(gen) * 0.8, 
                        0.1 * rdist(gen));
                }
            }},
            {3, "Burst Pattern", [](auto& scales, auto& gen, auto& rdist) {
                Vector::AddRandomWalk(scales, gen, 1.0, 
                    0.1 + rdist(gen) * 0.8, 
                    0.15 + 0.1 * rdist(gen));
            }},
            {4, "Load Breaker", [](auto& scales, auto& gen, auto& rdist) {
                Vector::AddSequence(scales, 0.2, 0.0, 1.0);
                Vector::AddSequence(scales, 6.0, rdist(gen), 
                    0.05 + 0.15 * rdist(gen));
                Vector::AddSequence(scales, -100, rdist(gen), 
                    0.05 + 0.15 * rdist(gen));
            }},
            {5, "Hourly Spikes", [](auto& scales, auto& gen, auto& rdist) {
                Vector::AddSequence(scales, 0.1, 0.0, 1.0);
                Vector::OnOffPatternNoise(scales, gen, 
                    2.0 + 5.0 * rdist(gen), 24, 
                    0.4 + rdist(gen) * 0.2);
            }}
        };
    
        auto& database = databases[0];
        database.query_count_slots.assign(100, 0.0);
        string pattern_desc;
    
        // 按顺序应用所有选择的模式
        for (auto pid : pattern_ids) {
            auto it = find_if(patterns.begin(), patterns.end(),
                [pid](const auto& p) { return get<0>(p) == pid; });
            
            if (it == patterns.end()) {
                throw runtime_error("Invalid pattern ID: " + to_string(pid));
            }
    
            // 应用当前模式的生成函数
            get<2>(*it)(database.query_count_slots, gen, rdist);
            
            // 构建模式描述
            if (!pattern_desc.empty()) pattern_desc += "+";
            pattern_desc += get<1>(*it);
        }
    
        // 应用全局强度缩放
        for (auto& s : database.query_count_slots) {
            s *= intensity;
            s = max(s, 0.0);
        }
    
        if (Vector::Sum(database.query_count_slots) <= 1e-9) {
            throw runtime_error("Combined pattern has zero sum");
        }
    
        database.pattern_ids = pattern_ids;
        database.pattern_description = pattern_desc;
    }
    static uint64_t EstimateTimeForQuery(uint32_t query_id, uint64_t scale_factor) {
        // 固定的查询时间估计
        const uint64_t query_times[24] = {
           0,
           scale_factor * 149560 * 8, // 1
           scale_factor * 55150 * 8,  // 2
           scale_factor * 112050 * 8, // 3
           scale_factor * 58190 * 8,  // 4
           scale_factor * 66190 * 8,  // 5
           scale_factor * 17490 * 8,  // 6
           scale_factor * 61090 * 8,  // 7
           scale_factor * 67480 * 8,  // 8
           scale_factor * 127850 * 8, // 9
           scale_factor * 95110 * 8,  // 10
           scale_factor * 15720 * 8,  // 11
           scale_factor * 64200 * 8,  // 12
           scale_factor * 146020 * 8, // 13
           scale_factor * 29000 * 8,  // 14
           scale_factor * 46390 * 8,  // 15
           scale_factor * 32790 * 8,  // 16
           scale_factor * 42430 * 8,  // 17
           scale_factor * 205680 * 8, // 18
           scale_factor * 51580 * 8,  // 19
           scale_factor * 46310 * 8,  // 20
           scale_factor * 113140 * 8, // 21
           scale_factor * 19820 * 8,  // 22
           scale_factor * (72530 + 97780) * 8 // 23
        };
 
        return query_times[query_id];
     }
     static uint64_t EstimateBytesForQuery(uint32_t query_id, uint64_t scale_factor) {
        
        const uint64_t query_bytes[24] = {
         0,            
         scale_factor * 158000000,   
         scale_factor * 55000000,    
         scale_factor * 110000000,   
         scale_factor * 60000000,    
         scale_factor * 65000000,    
         scale_factor * 20000000,    
         scale_factor * 60000000,    
         scale_factor * 70000000,    
         scale_factor * 125000000,   
         scale_factor * 90000000,    
         scale_factor * 16000000,    
         scale_factor * 65000000,    
         scale_factor * 140000000,   
         scale_factor * 30000000,    
         scale_factor * 45000000,    
         scale_factor * 33000000,    
         scale_factor * 42000000,    
         scale_factor * 200000000,   
         scale_factor * 50000000,    
         scale_factor * 45000000,    
         scale_factor * 110000000,   
         scale_factor * 20000000,    // 22,todo不是全都用snowflake测的值

     };
        return query_bytes[query_id];
    }
     static uint64_t AverageQueryTime(uint64_t scale_factor) {
        uint64_t sum = 0;
        for (uint32_t query_id = 1; query_id <= 23; query_id++) {
           sum += EstimateTimeForQuery(query_id, scale_factor);
        }
        return sum / 23;
     }
  
     static uint32_t GetRandomQuery(mt19937 &gen, Database &database) {
        if (database.is_read_only) {
           uniform_int_distribution<uint32_t> query_dist(1, 22);
           return query_dist(gen);
        } else {
           uniform_int_distribution<uint32_t> query_dist(1, 23);
           return query_dist(gen);
        }
     }

     void GenerateQueryArrivalTimes(uint64_t total_duration_in_hours) {
        mt19937 gen(GetSeedForQueries());
    
        for (auto &database: databases) {
            uint64_t total_budget;
            vector<uint64_t> budget_slots;
            
            if (use_bytes_mode) {
                total_budget = database.scanned_bytes;
                // 直接使用 query_count_slots 作为权重
                budget_slots.resize(database.query_count_slots.size());
                double sum = Vector::Sum(database.query_count_slots);
                
                for (size_t i = 0; i < budget_slots.size(); i++) {
                    budget_slots[i] = static_cast<uint64_t>(
                        (database.query_count_slots[i] / sum) * total_budget
                    );
                }
            } else {
                total_budget = database.cpu_time;
                budget_slots = Vector::ToCpuTime(database.query_count_slots, total_budget);
            }
            
            const uint32_t ms_per_slot = (total_duration_in_hours * 3600 * 1000) / budget_slots.size();
            database.queries.clear();
    
            for (uint64_t slot_idx = 0; slot_idx < budget_slots.size(); slot_idx++) {
                const uint64_t slot_budget = budget_slots[slot_idx];
                if (slot_budget == 0) continue;
    
                // 时间槽边界
                const uint32_t slot_start = ms_per_slot * slot_idx;
                const uint32_t slot_end = slot_start + ms_per_slot;
                
                // 均匀时间分布生成器
                uniform_int_distribution<uint32_t> time_dist(slot_start, slot_end - 1);
                
                // 当前槽剩余预算（CPU时间或扫描字节）
                uint64_t remaining_budget = slot_budget;
                
                // 生成查询直到用完预算
                while (remaining_budget > 0) {
                    // 随机选择查询
                    const uint32_t query_id = GetRandomQuery(gen, database);
                    uint64_t query_cost;
                    
                    if (use_bytes_mode) {
                        query_cost = EstimateBytesForQuery(query_id, database.scale_factor);
                    } else {
                        query_cost = EstimateTimeForQuery(query_id, database.scale_factor);
                    }
                    
                    // 确保至少生成一个查询
                    if (query_cost == 0) continue;
                    if (query_cost > remaining_budget && database.queries.size() > 0) break;
    
                    // 记录查询
                    database.queries.push_back({
                        time_dist(gen), // 在时间槽内均匀分布时间戳
                        query_id
                    });
    
                    // 更新剩余预算
                    remaining_budget -= min(query_cost, remaining_budget);
                }
            }
    
            // 按时间排序
            sort(database.queries.begin(), database.queries.end(),
                [](const Database::Query &a, const Database::Query &b) {
                    return a.start < b.start;
                });
            
            // 后处理：验证预算使用情况
            uint64_t total_used = 0;
            for (const auto &q : database.queries) {
                if (use_bytes_mode) {
                    total_used += EstimateBytesForQuery(q.query_id, database.scale_factor);
                } else {
                    total_used += EstimateTimeForQuery(q.query_id, database.scale_factor);
                }
            }
            
        }
    }
  
     void GenerateQueryArguments() {
        mt19937 gen(GetSeedForQueryArguments());
  
        for (auto &database: databases) {
           TpchQueries::UpdateState update_state(1000);
           for (auto &query : database.queries) {
              query.arguments = TpchQueries::GenerateQueryArguments(
                 query.query_id, database.scale_factor, gen, update_state
              );
           }
        }
     }

     void GenerateAndSaveTimeSlotInfo(const string& baseFilename, const vector<int>& pattern_ids, double intensity, int numFiles = 10) {
      // 为每个数据库生成时间槽信息
      for (auto &database: databases) {
          uint64_t totalBudget = use_bytes_mode ? database.scanned_bytes : database.cpu_time;
          
          // 生成10个不同的模式分布
          for (int fileIdx = 0; fileIdx < numFiles; fileIdx++) {
              // 创建一个新的模式分布
              vector<double> newSlots(100, 0.0);
              
              // 使用不同的随机种子生成模式
              mt19937 patternGen(GetSeedForPatterns() + fileIdx + 1);
              uniform_real_distribution<double> rdist(0.0, 1.0);
              
              // 应用所有选择的模式（与原GenerateQueryArrivalDistribution类似的逻辑）
              for (auto pid : pattern_ids) {
                  switch (pid) {
                      case 1: { // Sinusoidal Noise
                          Vector::AddSequence(newSlots, 0.01, 0, 1.0);
                          for (uint32_t i = 0; i < 8; i++) {
                              Vector::AddSinusHead(newSlots, 0.5, 
                                  0.6 * rdist(patternGen) - 0.3, 
                                  0.05 + rdist(patternGen) * 0.05);
                          }
                          Vector::AddRandomNoise(newSlots, patternGen, 1.0, 0.1 * rdist(patternGen));
                          break;
                      }
                      case 2: { // Random Spikes
                          exponential_distribution<double> exp_dist(0.5);
                          uint32_t iterations = round(exp_dist(patternGen) * 2 + 1);
                          for (uint32_t i = 0; i < iterations; i++) {
                              Vector::AddRandomWalk(newSlots, patternGen, 1.0, 
                                  0.1 + rdist(patternGen) * 0.8, 
                                  0.1 * rdist(patternGen));
                          }
                          break;
                      }
                      case 3: { // Burst Pattern
                          Vector::AddRandomWalk(newSlots, patternGen, 1.0, 
                              0.1 + rdist(patternGen) * 0.8, 
                              0.15 + 0.1 * rdist(patternGen));
                          break;
                      }
                      case 4: { // Load Breaker
                          Vector::AddSequence(newSlots, 0.2, 0.0, 1.0);
                          Vector::AddSequence(newSlots, 6.0, rdist(patternGen), 
                              0.05 + 0.15 * rdist(patternGen));
                          Vector::AddSequence(newSlots, -100, rdist(patternGen), 
                              0.05 + 0.15 * rdist(patternGen));
                          break;
                      }
                      case 5: { // Hourly Spikes
                          Vector::AddSequence(newSlots, 0.1, 0.0, 1.0);
                          Vector::OnOffPatternNoise(newSlots, patternGen, 
                              2.0 + 5.0 * rdist(patternGen), 24, 
                              0.4 + rdist(patternGen) * 0.2);
                          break;
                      }
                  }
              }
              
              // 应用全局强度缩放
              for (auto& s : newSlots) {
                  s *= intensity;
                  s = max(s, 0.0);
              }
              
              // 确保数值有效，并将权重转换为实际的CPU时间/字节
              double sum = Vector::Sum(newSlots);
              if (sum <= 1e-9) {
                  cerr << "警告：文件 " << (fileIdx+1) << " 的模式总和为零" << endl;
                  continue;
              }
              
              // 转换为实际的CPU时间/字节分配
              for (auto& s : newSlots) {
                  s = (s / sum) * totalBudget;
              }
              
              // 保存到JSON文件
              string filename = baseFilename + "_slot_info_" + to_string(fileIdx+1) + ".json";
              ofstream os(filename);
              if (!os) {
                  cerr << "无法打开输出文件: " << filename << endl;
                  continue;
              }
              
              // 写入JSON格式
              os << "{\n";
              os << "  \"mode\": \"" << (use_bytes_mode ? "bytes" : "cpu") << "\",\n";
              os << "  \"slot_count\": " << newSlots.size() << ",\n";
              os << "  \"values\": [\n";
              
              for (size_t i = 0; i < newSlots.size(); i++) {
                  os << "    " << fixed << setprecision(0) << newSlots[i];
                  if (i < newSlots.size() - 1) {
                      os << ",";
                  }
                  os << "\n";
              }
              
              os << "  ]\n";
              os << "}\n";
              
              cout << "Generated history cputime of each slot : " << filename << endl;
          }
      }
  }
};

int main(int argc, char **argv) {
    int pattern_id = 1;
    double intensity = 1.0;
    uint64_t scale_factor = 1;
    uint64_t cpu_hours = 1;
    uint64_t duration = 1;
    vector<int> pattern_ids;
    bool use_bytes_mode = false;

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            while (i+1 < argc && argv[i+1][0] != '-') {
                pattern_ids.push_back(atoi(argv[++i]));
            }
        }else if (strcmp(argv[i], "-s") == 0 && i+1 < argc) {
            scale_factor = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 && i+1 < argc) {
            cpu_hours = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i+1 < argc) {
            duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-mode") == 0 && i+1 < argc) {
            if (strcmp(argv[++i], "bytes") == 0) {
                use_bytes_mode = true;
            } else {
                cerr << "Unknown mode. Use 'bytes' for bytes mode or omit for CPU time mode." << endl;
                return 1;
            }
        } else {
            cerr << "Unknown parameter: " << argv[i] << endl;
            cerr << "Usage: " << argv[0] << " -p <pattern> -d <intensity> -s <scale> -c <hours/bytes> -t <duration> [-mode bytes]" << endl;
            return 1;
        }
    }

    if (pattern_id < 1 || pattern_id > 5) {
        cerr << "Invalid pattern ID (1-5)" << endl;
        return 1;
    }

    Generator generator;
    generator.GenerateFixedDatabases(1, scale_factor, cpu_hours,use_bytes_mode);
    try {
        generator.GenerateQueryArrivalDistribution(pattern_ids, intensity);
        generator.GenerateQueryArrivalTimes(duration);
        generator.GenerateQueryArguments();
    } catch (const exception &e) {
        cerr << "Error generating queries: " << e.what() << endl;
        return 1;
    }

    // 创建输出目录
    struct stat st;
    if (stat("../query_streams", &st) != 0) {
        mkdir("..query_streams", 0755);
    }

string pattern_str;
for (auto pid : pattern_ids) {
    if (!pattern_str.empty()) pattern_str += "+";
    pattern_str += to_string(pid);
}

string mode_str = use_bytes_mode ? "bytes" : "cpu";
string baseFilename = "../query_streams/query_stream_p" + pattern_str +
                 "_s" + to_string(scale_factor) +
                 "_" + mode_str + to_string(cpu_hours) +
                 "_t" + to_string(duration);

string filename = baseFilename + ".json";
    
    ofstream os(filename);
    if (!os) {
        cerr << "Cannot open output file: " << filename << endl;
        return 1;
    }
    
    generator.databases[0].WriteJson(os);
    cout << "Generated query stream: " << filename << endl;

    // 生成查询流后
    generator.GenerateAndSaveTimeSlotInfo(baseFilename, pattern_ids, intensity);

    return 0;
}