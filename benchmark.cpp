#include "database.h"

#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <cassert>
#include <cmath>

using namespace std;
//都是生成各种随机数的
struct Utility {
   static vector<double> GenerateDeterministicLogNormal(uint64_t count, double mean, double sd, uint64_t cut_off, uint64_t seed)
   {
      // 参数为样本数量、均值、标准差、跳过的样本数量、随机种子
      // Generate large sample
      mt19937 gen(seed);// 初始化随机数生成器，使用给定的随机种子
      lognormal_distribution<> dist(mean, sd);// 定义对数正态分布
      vector<double> over_samples(1000 * count);// 创建一个比目标样本数多1000倍的样本容器
      for (double &over_sample : over_samples) {
         over_sample = dist(gen);//生成对数随机数
      }
      sort(over_samples.begin(), over_samples.end());//排序

      // Sample the sample
      vector<double> result(count); // 最终结果容器，大小为 count
      double segment_count = count + cut_off * 2 + 1;//cut_off * 2 是为了忽略两端的极值段（cut_off 表示跳过的段数）。+1 是为了确保计算分段时包含边界。
      double segment_count_width = over_samples.size() / segment_count;
      for (uint64_t i = cut_off; i<count + cut_off * 2; i++) {
         result[i] = over_samples[segment_count_width * i]; // TODO: fix don't start at 0。先这样吧，或许可以改成i-cutoff试试
      }

      return result;
   }
   //用于生成一组伪随机的对数正态分布样本

   // https://en.wikipedia.org/wiki/Triangular_distribution
   static double GenerateTriangleValue(double a, double b, double c, mt19937 &gen)
   {
      uniform_real_distribution dist;
      double U = dist(gen);
      double F = (c - a) / (b - a);
      if (U<F) {
         return a + sqrt(U * (b - a) * (c - a));
      } else {
         return b - sqrt((1 - U) * (b - a) * (b - c));//公式，概率密度都是线性的
      }
   }
   //用于生成一组三角分布的随机数
   static double GenerateNonExtremeNormal(double mean, double sd, mt19937 &gen)
   {
      normal_distribution<> dist(mean, sd);
      double result = dist(gen);
      while (abs(result - mean)>2.0 * sd) { // ~95% of values are fine
         result = dist(gen);
      }
      return result;
   }
   //去掉两个西格玛外的离群值
};

//主要用于生成 TPC-H（一个标准化的数据库性能测试基准）查询的参数。
struct TpchQueries {
   //基本常量列表
   struct Arguments {
      // TPC-H 4.2.3
      inline constexpr static std::array<const char *, 5> regions = {"AFRICA", "AMERICA", "ASIA", "EUROPE", "MIDDLE EAST"};
      inline constexpr static std::array<int32_t, 25> nation_to_region = {0, 1, 1, 1, 4, 0, 3, 3, 2, 2, 4, 4, 2, 4, 0, 0, 0, 1, 2, 3, 4, 2, 3, 3, 1};
      //这是国家和洲的对应关系
      inline constexpr static std::array<const char *, 25> nations = {"ALGERIA", "ARGENTINA", "BRAZIL", "CANADA", "EGYPT", "ETHIOPIA", "FRANCE", "GERMANY", "INDIA", "INDONESIA", "IRAN", "IRAQ", "JAPAN", "JORDAN", "KENYA", "MOROCCO", "MOZAMBIQUE", "PERU", "CHINA", "ROMANIA", "SAUDI ARABIA", "VIETNAM", "RUSSIA", "UNITED KINGDOM", "UNITED STATES"};
      //constexpr 表示变量是一个 编译时常量。编译器在编译期间计算出其值，并将其视为不可更改的常量。
      // TPC-H 4.2.2.13
      struct Type {
         inline constexpr static std::array<const char *, 6> syllable1 = {"STANDARD", "SMALL", "MEDIUM", "LARGE", "ECONOMY", "PROMO"};
         inline constexpr static std::array<const char *, 5> syllable2 = {"ANODIZED", "BURNISHED", "PLATED", "POLISHED", "BRUSHED"};
         inline constexpr static std::array<const char *, 5> syllable3 = {"TIN", "NICKEL", "BRASS", "STEEL", "COPPER"};
      };
      //三种物品分类
      struct Container {
         inline constexpr static std::array<const char *, 5> syllable1 = {"SM", "LG", "MED", "JUMBO", "WRAP"};
         inline constexpr static std::array<const char *, 8> syllable2 = {"CASE", "BOX", "BAG", "JAR", "PKG", "PACK", "CAN", "DRUM"};
      };
      //包装尺寸和类型
      inline constexpr static std::array<const char *, 5> segments = {"AUTOMOBILE", "BUILDING", "FURNITURE", "MACHINERY", "HOUSEHOLD"};
      inline constexpr static std::array<const char *, 92> colors = {"almond", "antique", "aquamarine", "azure", "beige", "bisque", "black", "blanched", "blue", "blush", "brown", "burlywood", "burnished", "chartreuse", "chiffon", "chocolate", "coral", "cornflower", "cornsilk", "cream", "cyan", "dark", "deep", "dim", "dodger", "drab", "firebrick", "floral", "forest", "frosted", "gainsboro", "ghost", "goldenrod", "green", "grey", "honeydew", "hot", "indian", "ivory", "khaki", "lace", "lavender", "lawn", "lemon", "light", "lime", "linen", "magenta", "maroon", "medium", "metallic", "midnight", "mint", "misty", "moccasin", "navajo", "navy", "olive", "orange", "orchid", "pale", "papaya", "peach", "peru", "pink", "plum", "powder", "puff", "purple", "red", "rose", "rosy", "royal", "saddle", "salmon", "sandy", "seashell", "sienna", "sky", "slate", "smoke", "snow", "spring", "steel", "tan", "thistle", "tomato", "turquoise", "violet", "wheat", "white", "yellow"};
      inline constexpr static std::array<const char *, 7> mode = {"REG AIR", "AIR", "RAIL", "SHIP", "TRUCK", "MAIL", "FOB"};
      //行业、颜色、运输
   };
   //UpdateState负责管理更新状态并生成与更新有关的参数。这些参数用于 TPC-H 查询中动态变化的数据集或场景。
   struct UpdateState {
      uint32_t update_count; // for one rotation; splits relation into 'update_count' blocks that are updated together
      //表示更新分块的总数，将整个数据集分成 update_count 个更新块。在一个完整的轮次（rotation）中，数据被分成若干块，每次更新一个块。
      uint32_t rotation; // which of the keys 0: 0-7, 1: 8-15, 2: 16-23, 3: 24-31
      //表示更新的轮次，用于标识当前更新块的区间。
      uint32_t update; // current update update_count ... 1
      //当前处理的快编号

      explicit UpdateState(uint32_t update_count)
              : update_count(update_count)
                , rotation(0)
                , update(update_count) {}
      //构造函数
      //GenerateNext根据 scale_factor和当前状态生成更新的参数范围。每次调用都会更新状态，返回一个描述更新块范围的字符串列表。
      std::vector<std::string> GenerateNext(uint64_t scale_factor)
      {
         const uint64_t order_count = scale_factor * 1500000; //1500000 是基础数据规模
         const uint64_t max_order_key = 1 + order_count * 4;//每个订单可能对应 4 个键值。、
         const uint64_t group_count = 1 + max_order_key / 32;
         const uint64_t first_group = (group_count * (update - 1)) / update_count;
         const uint64_t last_group = (group_count * update) / update_count;
         assert(group_count>=update_count);  
         //总订单数量、最大订单键，组数，当前更新起始组和中止组
         //assert确保分组数足够分配给更新块。

         std::vector<std::string> result;
         result.push_back(to_string(first_group * 32));
         result.push_back(to_string(last_group * 32));
         result.push_back(to_string(rotation * 8));
         result.push_back(to_string((rotation + 1) * 8 - 1));

         //当前更新组的键值范围、和轮次的范围
         // which of the keys 0: 0-7, 1: 8-15, 2: 16-23, 3: 24-31
         update--;
         if (update == 0) {
            update = update_count;
            rotation = (rotation + 1) % 4;
         }
         //轮次取模4加1
         return result;
      }
   };
   //示例：
   //UpdateState state(4); // update_count = 4, rotation = 0, update = 4
   //result = {"4500000", "6000032", "0", "7"};
   //


   static string TwoDigitNumber(uint32_t num)
   {
      return (num<10 ? "0" : "") + to_string(num);
   }
   //变字符串、小于10补0

   static string QStr(const string &str)
   {
      return "\"" + str + "\"";
   }
   //加引号

   static vector<string> GenerateQueryArguments(int query_id, uint64_t scale_factor, mt19937 &gen, UpdateState &update_state)
   {
      string q = "\"";
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
            return vector<string>{QStr(to_string(date(gen)) + "-01-01"), to_string(discount(gen)), to_string(quantity(gen))}; // note: [discount] / 100 in the query
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
            return vector<string>{QStr(Arguments::nations[selected_nation_idx]), QStr(Arguments::regions[Arguments::nation_to_region[selected_nation_idx]]), //nl
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
            return vector<string>{QStr(Arguments::nations[nation(gen)]), to_string(scale_factor)}; // note: 0.0001 / [scale_factor] in the query
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
            return vector<string>{QStr("Brand#" + to_string(brand(gen)) + to_string(brand(gen))), //nl
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
         //前22个是随机生成22条查询中的可变参数
         case 23: {
            return update_state.GenerateNext(scale_factor);
         }
         default: {
            throw;
         }
         //第23个是根据当前的 update_count 和 rotation 返回更新块的参数范围。更新原始订单数据，如果我没理解错
      }
   }
};
//提供了一些工具方法，用于对一个存储浮点数的向量（vector<double>）进行各种操作，比如：

//添加波形、随机噪声、序列等不同的模式。
//将这些模式组合以模拟复杂的分布。
//进行简单的向量操作（如求和、归一化、时间分配）。
struct Vector {
   static void AddSinusHead(vector<double> &scales, double intensity, double start_ratio, double width)
   {
      double a = 3.1415 / (scales.size() * width);
      double b = 3.1415 * ((start_ratio + (0.5 * (1 - width))) / width);

      for (uint32_t i = 0; i<scales.size(); i++) {
         double x = i * a - b;
         if (0<x && x<3.1415) { // Draw only one hump
            double val = sin(x) * intensity;
            if (val>0) {
               scales[i] += val;
            }
         }
      }
   }
   //加正弦波
   static void AddRandomNoise(vector<double> &scales, mt19937 &gen, double intensity, double likelihood)
   {
      uniform_real_distribution dist(0.0, 1.0);
      for (double &scale : scales) {
         if (dist(gen)<=likelihood) {
            auto fac = dist(gen);
            scale += intensity * fac;
            if (scale<0) {
               scale = 0;
            }
         }
      }
   }
   //添加随机噪声
   static void AddSequence(vector<double> &scales, double intensity, double start_ratio, double length_ratio)
   {
      uint32_t start = start_ratio * scales.size();
      uint32_t length = length_ratio * scales.size();
      for (uint32_t pos = start; pos<start + length; pos++) {
         double &scale = scales[pos % scales.size()];
         scale += intensity;
         if (scale<0) {
            scale = 0;
         }
      }
   }
   //添加固定强度的连续序列

   static void OnOffPattern(vector<double> &scales, mt19937 &gen, double intensity, uint32_t spike_count, double length)
   {
      double spike_width = double(scales.size()) / spike_count;
      for (uint32_t spike_idx = 0; spike_idx<spike_count; spike_idx++) {
         uint32_t spike_start = spike_idx * spike_width;
         uint32_t spike_end = spike_start + spike_width * length;
         for (uint32_t pos = spike_start; pos<spike_end; pos++) {
            double &scale = scales[pos % scales.size()];
            scale += intensity;
            if (scale<0) {
               scale = 0;
            }
         }
      }
   }
   //添加脉冲信号
   static void OnOffPatternNoise(vector<double> &scales, mt19937 &gen, double intensity, uint32_t spike_count, double length)
   {
      uniform_real_distribution dist(0.0, 1.0);
      double spike_width = double(scales.size()) / spike_count;
      for (uint32_t spike_idx = 0; spike_idx<spike_count; spike_idx++) {
         uint32_t spike_start = spike_idx * spike_width;
         uint32_t spike_end = spike_start + spike_width * length;
         double spike_height = dist(gen);
         for (uint32_t pos = spike_start; pos<spike_end; pos++) {
            double &scale = scales[pos % scales.size()];
            scale += intensity * spike_height;
            if (scale<0) {
               scale = 0;
            }
         }
      }
   }
   //添加有随机脉冲的信号

   static void AddSequenceRandomNoise(vector<double> &scales, mt19937 &gen, double intensity, double start_ratio, double length_ratio)
   {
      uniform_real_distribution dist(0.0, 1.0);
      uint32_t start = start_ratio * scales.size();
      uint32_t length = length_ratio * scales.size();
      for (uint32_t pos = start; pos<start + length; pos++) {
         double &scale = scales[pos % scales.size()];
         scale += intensity * dist(gen);
         if (scale<0) {
            scale = 0;
         }
      }
   }
   //添加带随机噪声的连续序列。

   static void AddRandomWalk(vector<double> &scales, mt19937 &gen, double intensity, double start_ratio, double length_ratio)
   {
      uniform_real_distribution dist(0.0, 1.0);
      uint32_t start = start_ratio * scales.size();
      uint32_t length = length_ratio * scales.size();
      double diff = 0.1 * intensity;
      double state = dist(gen) * intensity;
      for (uint32_t pos = start; pos<start + length; pos++) {
         if (dist(gen)>=0.5) {
            state += diff;
         } else {
            state -= diff;
         }
         double &scale = scales[pos % scales.size()];
         scale += state;
         if (scale<0) {
            scale = 0;
         }
         if (scale>intensity) {
            scale = intensity;
         }
      }
   }
   //添加随机游走信号。
   static vector<uint64_t> ToCpuTime(vector<double> &scales, uint64_t total_cpu_time)
   {
      double sum = Sum(scales);
      assert(sum>0);

      vector<uint64_t> cpu_time_in_slot(scales.size());
      for (uint32_t idx = 0; idx<scales.size(); idx++) {
         cpu_time_in_slot[idx] = total_cpu_time * scales[idx] / sum;
      }
      return cpu_time_in_slot;
   }
   //计算总cpu时间
   static double Sum(vector<double> &scales)
   {
      double sum = 0.0;
      for (double scale : scales) {
         assert(scale>=0);
         sum += scale;
      }
      return sum;
   }
};

struct Generator {
    static uint64_t GetSeedForDatabaseCount() { return 6; }
    static uint64_t GetSeedForQueryCount() { return 28; }
    static uint64_t GetSeedForPatterns() { return 496; }
    static uint64_t GetSeedForQueries() { return 8128; }
    static uint64_t GetSeedForQueryArguments() { return 33550336; }
 
    vector<Database> databases;
 
    static double SizeToScaleFactor(double size) {
       const double scale_factor = std::round(size / 1_GB);
       return scale_factor == 0 ? 1 : scale_factor;
    }
 
    // 不再需要这个函数
    void GenerateDatabases(uint64_t database_count, uint64_t total_size) {
       throw std::runtime_error("Deprecated: Use GenerateFixedPatternDatabases instead");
    }
 
    void GenerateFixedPatternDatabases(uint64_t scale_factor, uint64_t cpu_hours) {
       // 固定生成5个实例，对应5种模式
       this->databases.resize(5);
       for (uint64_t idx = 0; idx < 5; idx++) {
          this->databases[idx].database_id = idx;
          this->databases[idx].scale_factor = scale_factor;
          this->databases[idx].cpu_time = cpu_hours * 3600e6;
       }
    }
 
    static uint64_t RollQueryCount(const Database &database, mt19937 &gen) {
       uint32_t size_bucket = database.GetSizeBucket();
       switch (size_bucket) {
          case 8:
          case 9: return pow(2.718282, Utility::GenerateTriangleValue(0, 11.7, 3, gen));
          case 10: return pow(2.718282, Utility::GenerateTriangleValue(0, 11.7, 4.1, gen));
          case 11: return pow(2.718282, Utility::GenerateTriangleValue(0, 11.6, 3.2, gen));
          case 12: return pow(2.718282, Utility::GenerateTriangleValue(0, 11.7, 4.8, gen));
          case 13: return pow(2.718282, Utility::GenerateTriangleValue(0, 11.7, 5.9, gen));
          case 14: return pow(2.718282, Utility::GenerateTriangleValue(0, 11.7, 5.5, gen));
          default: throw;
       }
    }
 
    static uint64_t RollCpuTime(const Database &database, mt19937 &gen) {
       uint32_t size_bucket = database.GetSizeBucket();
       switch (size_bucket) {
          case 8:
          case 9: return pow(2.718282, Utility::GenerateNonExtremeNormal(19.81281, 2.847744, gen));
          case 10: return pow(2.718282, Utility::GenerateNonExtremeNormal(21.51081, 2.949972, gen));
          case 11: return pow(2.718282, Utility::GenerateNonExtremeNormal(22.30084, 3.469075, gen));
          case 12: return pow(2.718282, Utility::GenerateNonExtremeNormal(23.72666, 3.349028, gen));
          case 13: return pow(2.718282, Utility::GenerateNonExtremeNormal(24.03537, 3.401711, gen));
          case 14: return pow(2.718282, Utility::GenerateNonExtremeNormal(24.32934, 4.289488, gen));
          default: throw;
       }
    }
 
    void GenerateCpuTimeForDatabases(uint64_t total_cpu_hours) {
       mt19937 gen(GetSeedForQueryCount());
       uint64_t actual_cpu_time = 0;
       for (auto &database : databases) {
          database.cpu_time = RollCpuTime(database, gen);
          actual_cpu_time += database.cpu_time;
       }
 
       uint64_t wanted_cpu_time = total_cpu_hours * 3600 * 1e6;
       for (auto &database : databases) {
          database.cpu_time = database.cpu_time * (wanted_cpu_time * 1.0 / actual_cpu_time);
       }
    }
 
    void GenerateQueryArrivalDistribution() {
       mt19937 gen(GetSeedForPatterns());
       uniform_real_distribution<double> rdist(0.0, 1.0);
 
       // 定义5种固定模式
       const vector<pair<int, function<void(vector<double>&)>>> patterns = {
          {1, [&](vector<double> &scales) {
             Vector::AddSequence(scales, 0.2, 0, 1.0);
             Vector::AddSequenceRandomNoise(scales, gen, 1.0, 0, 1.0);
             for (uint32_t i = 0; i < 8; i++) {
                Vector::AddSinusHead(scales, 0.5, 0.6 * rdist(gen) - 0.3, 0.05 + rdist(gen) * 0.05);
             }
             Vector::AddRandomNoise(scales, gen, 1.0, 0.1 * rdist(gen));
          }},
          {3, [&](vector<double> &scales) {
             exponential_distribution<double> exp_dist(0.5);
             uint32_t iteration_count = round(exp_dist(gen) * 2 + 1);
             for (uint32_t i = 0; i < iteration_count; i++) {
                Vector::AddRandomWalk(scales, gen, 1.0, 0.1 + rdist(gen) * 0.8, 0.1 * rdist(gen));
             }
          }},
          {4, [&](vector<double> &scales) {
             Vector::AddRandomWalk(scales, gen, 1.0, 0.1 + rdist(gen) * 0.8, 0.15 + 0.1 * rdist(gen));
          }},
          {5, [&](vector<double> &scales) {
             Vector::AddSequence(scales, 4.0, 0.0, 1.0);
             Vector::AddSequence(scales, 6.0, rdist(gen), 0.05 + 0.15 * rdist(gen));
             Vector::AddSequence(scales, -100, rdist(gen), 0.05 + 0.15 * rdist(gen));
          }},
          {6, [&](vector<double> &scales) {
             Vector::AddSequence(scales, 2.0, 0.0, 1.0);
             Vector::OnOffPatternNoise(scales, gen, 2.0 + 5.0 * rdist(gen), 24, 0.4 + rdist(gen) * 0.2);
          }}
       };
 
       // 为每个数据库分配固定模式
       for (size_t i = 0; i < databases.size(); i++) {
          databases[i].query_count_slots.clear();
          databases[i].query_count_slots.resize(100, 0);
          patterns[i].second(databases[i].query_count_slots);
          databases[i].pattern_id = patterns[i].first;
          
          // 确保生成的模式有效
          if (Vector::Sum(databases[i].query_count_slots) == 0) {
             i--; // 重试这个模式
          }
       }
    }
 
    static uint64_t EstimateTimeForQuery(uint32_t query_id, uint64_t scale_factor) {
       switch (query_id) {
          case 1: return scale_factor * 149560 * 8;
          case 2: return scale_factor * 55150 * 8;
          case 3: return scale_factor * 112050 * 8;
          case 4: return scale_factor * 58190 * 8;
          case 5: return scale_factor * 66190 * 8;
          case 6: return scale_factor * 17490 * 8;
          case 7: return scale_factor * 61090 * 8;
          case 8: return scale_factor * 67480 * 8;
          case 9: return scale_factor * 127850 * 8;
          case 10: return scale_factor * 95110 * 8;
          case 11: return scale_factor * 15720 * 8;
          case 12: return scale_factor * 64200 * 8;
          case 13: return scale_factor * 146020 * 8;
          case 14: return scale_factor * 29000 * 8;
          case 15: return scale_factor * 46390 * 8;
          case 16: return scale_factor * 32790 * 8;
          case 17: return scale_factor * 42430 * 8;
          case 18: return scale_factor * 205680 * 8;
          case 19: return scale_factor * 51580 * 8;
          case 20: return scale_factor * 46310 * 8;
          case 21: return scale_factor * 113140 * 8;
          case 22: return scale_factor * 19820 * 8;
          case 23: return scale_factor * (72530 + 97780) * 8;
          default: throw;
       }
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
          uint64_t total_cpu_time = database.cpu_time;
          vector<uint64_t> cpu_time_slots = Vector::ToCpuTime(database.query_count_slots, total_cpu_time);
 
          uint32_t now_ms = 0;
          uint32_t ms_per_slot = (total_duration_in_hours * 3600 * 1000) / cpu_time_slots.size();
          uint64_t average_query_cup_time = AverageQueryTime(database.scale_factor);
 
          for (uint64_t slot_idx = 0; slot_idx < cpu_time_slots.size(); slot_idx++) {
             uint64_t cpu_time_in_slot = cpu_time_slots[slot_idx];
             if (cpu_time_in_slot == 0) continue;
 
             uint64_t query_count_in_slot = cpu_time_in_slot / average_query_cup_time;
             if (query_count_in_slot < 1) query_count_in_slot = 1;
 
             double rate_per_second = query_count_in_slot / (ms_per_slot / 1000.0);
             exponential_distribution dist_s(rate_per_second);
             uint32_t slot_start = ms_per_slot * slot_idx;
             now_ms = max(now_ms, slot_start);
 
             uint64_t generated_cpu_time = 0;
             while (generated_cpu_time < cpu_time_in_slot) {
                double distance_s = dist_s(gen);
                now_ms += distance_s * 1000.0;
                now_ms = (now_ms - slot_start) % ms_per_slot + slot_start;
                uint32_t query_id = GetRandomQuery(gen, database);
                generated_cpu_time += EstimateTimeForQuery(query_id, database.scale_factor);
                database.queries.push_back(Database::Query{now_ms, query_id});
             }
          }
 
          sort(database.queries.begin(), database.queries.end(), 
             [](const Database::Query &lhs, const Database::Query &rhs) {
                return lhs.start < rhs.start;
             });
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
 
    // 保留所有的调试输出函数
    void DumpDatabasesSizesForR() const {
       cout << "db_sizes=c(";
       for (uint64_t idx = 0; idx < databases.size(); idx++) {
          cout << databases[idx].scale_factor * 1_GB << (idx == databases.size() - 1 ? "" : ",");
       }
       cout << ")" << endl;
    }
 
    void DumpDatabaseCpuTimesForR() const {
       cout << "cpu_times=c(";
       for (uint64_t idx = 0; idx < databases.size(); idx++) {
          cout << databases[idx].cpu_time << (idx == databases.size() - 1 ? "" : ",");
       }
       cout << ")" << endl;
    }
 
    void DumpDatabaseSizeBucketsForR() const {
       cout << "size_buckets=c(";
       for (uint64_t idx = 0; idx < databases.size(); idx++) {
          cout << databases[idx].GetSizeBucket() << (idx == databases.size() - 1 ? "" : ",");
       }
       cout << ")" << endl;
    }
 
    void DumpDatabaseWithPattern() const {
       for (uint64_t idx = 0; idx < databases.size(); idx++) {
          cout << idx << ": " << databases[idx].pattern_id << endl;
       }
    }
 
    void DumpDatabaseQueryArrivalsCsv(ostream &os) const {
       os << "db,pattern,time,query" << endl;
       for (auto &database : databases) {
          for (auto &query : database.queries) {
             os << database.database_id << ',' << database.pattern_id << ',' 
                << query.start << ',' << query.query_id << '\n';
          }
       }
       os << flush;
    }
};
int main(int argc, char **argv)
{
   const uint64_t scale_factor = 4000;  // 4TB = 4000GB
   const uint64_t total_cpu_hours = 40;
   const uint64_t total_duration_in_hours = 1;

   Generator generator;
   
   // 生成5个固定大小的数据库实例
   generator.GenerateFixedPatternDatabases(scale_factor, total_cpu_hours);
   
   // 生成5种固定模式的查询分布
   generator.GenerateQueryArrivalDistribution();
   
   // 生成查询到达时间
   generator.GenerateQueryArrivalTimes(total_duration_in_hours);

   generator.DumpDatabaseCpuTimesForR();
   generator.DumpDatabaseSizeBucketsForR();

   // 生成查询参数并写入文件
   generator.GenerateQueryArguments();
   for (uint32_t i = 0; i < generator.databases.size(); i++) {
      ofstream os("query_streams/query_stream_" + to_string(i) + ".json");
      generator.databases[i].WriteJson(os);
   }

   return 0;
}