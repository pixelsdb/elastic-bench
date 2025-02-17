#include "database_new.h"

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

struct Utility {
    static vector<double> GenerateDeterministicLogNormal(uint64_t count, double mean, double sd, uint64_t cut_off, uint64_t seed)
    {
       // ???????????????????????????
       // Generate large sample
       mt19937 gen(seed);// ???????????????????
       lognormal_distribution<> dist(mean, sd);// ????????
       vector<double> over_samples(1000 * count);// ???????????1000??????
       for (double &over_sample : over_samples) {
          over_sample = dist(gen);//???????
       }
       sort(over_samples.begin(), over_samples.end());//??
 
       // Sample the sample
       vector<double> result(count); // ?????????? count
       double segment_count = count + cut_off * 2 + 1;//cut_off * 2 ????????????cut_off ?????????+1 ???????????????
       double segment_count_width = over_samples.size() / segment_count;
       for (uint64_t i = cut_off; i<count + cut_off * 2; i++) {
          result[i] = over_samples[segment_count_width * i]; // TODO: fix don't start at 0????????????i-cutoff??
       }
 
       return result;
    }
    //??????????????????
 
    // https://en.wikipedia.org/wiki/Triangular_distribution
    static double GenerateTriangleValue(double a, double b, double c, mt19937 &gen)
    {
       uniform_real_distribution dist;
       double U = dist(gen);
       double F = (c - a) / (b - a);
       if (U<F) {
          return a + sqrt(U * (b - a) * (c - a));
       } else {
          return b - sqrt((1 - U) * (b - a) * (b - c));//????????????
       }
    }
    //??????????????
    static double GenerateNonExtremeNormal(double mean, double sd, mt19937 &gen)
    {
       normal_distribution<> dist(mean, sd);
       double result = dist(gen);
       while (abs(result - mean)>2.0 * sd) { // ~95% of values are fine
          result = dist(gen);
       }
       return result;
    }
    //????????????
 };

 struct TpchQueries {
    //??????
    struct Arguments {
       // TPC-H 4.2.3
       inline constexpr static std::array<const char *, 5> regions = {"AFRICA", "AMERICA", "ASIA", "EUROPE", "MIDDLE EAST"};
       inline constexpr static std::array<int32_t, 25> nation_to_region = {0, 1, 1, 1, 4, 0, 3, 3, 2, 2, 4, 4, 2, 4, 0, 0, 0, 1, 2, 3, 4, 2, 3, 3, 1};
       //???????????
       inline constexpr static std::array<const char *, 25> nations = {"ALGERIA", "ARGENTINA", "BRAZIL", "CANADA", "EGYPT", "ETHIOPIA", "FRANCE", "GERMANY", "INDIA", "INDONESIA", "IRAN", "IRAQ", "JAPAN", "JORDAN", "KENYA", "MOROCCO", "MOZAMBIQUE", "PERU", "CHINA", "ROMANIA", "SAUDI ARABIA", "VIETNAM", "RUSSIA", "UNITED KINGDOM", "UNITED STATES"};
       //constexpr ??????? ?????????????????????????????????
       // TPC-H 4.2.2.13
       struct Type {
          inline constexpr static std::array<const char *, 6> syllable1 = {"STANDARD", "SMALL", "MEDIUM", "LARGE", "ECONOMY", "PROMO"};
          inline constexpr static std::array<const char *, 5> syllable2 = {"ANODIZED", "BURNISHED", "PLATED", "POLISHED", "BRUSHED"};
          inline constexpr static std::array<const char *, 5> syllable3 = {"TIN", "NICKEL", "BRASS", "STEEL", "COPPER"};
       };
       //??????
       struct Container {
          inline constexpr static std::array<const char *, 5> syllable1 = {"SM", "LG", "MED", "JUMBO", "WRAP"};
          inline constexpr static std::array<const char *, 8> syllable2 = {"CASE", "BOX", "BAG", "JAR", "PKG", "PACK", "CAN", "DRUM"};
       };
       //???????
       inline constexpr static std::array<const char *, 5> segments = {"AUTOMOBILE", "BUILDING", "FURNITURE", "MACHINERY", "HOUSEHOLD"};
       inline constexpr static std::array<const char *, 92> colors = {"almond", "antique", "aquamarine", "azure", "beige", "bisque", "black", "blanched", "blue", "blush", "brown", "burlywood", "burnished", "chartreuse", "chiffon", "chocolate", "coral", "cornflower", "cornsilk", "cream", "cyan", "dark", "deep", "dim", "dodger", "drab", "firebrick", "floral", "forest", "frosted", "gainsboro", "ghost", "goldenrod", "green", "grey", "honeydew", "hot", "indian", "ivory", "khaki", "lace", "lavender", "lawn", "lemon", "light", "lime", "linen", "magenta", "maroon", "medium", "metallic", "midnight", "mint", "misty", "moccasin", "navajo", "navy", "olive", "orange", "orchid", "pale", "papaya", "peach", "peru", "pink", "plum", "powder", "puff", "purple", "red", "rose", "rosy", "royal", "saddle", "salmon", "sandy", "seashell", "sienna", "sky", "slate", "smoke", "snow", "spring", "steel", "tan", "thistle", "tomato", "turquoise", "violet", "wheat", "white", "yellow"};
       inline constexpr static std::array<const char *, 7> mode = {"REG AIR", "AIR", "RAIL", "SHIP", "TRUCK", "MAIL", "FOB"};
       //????????
    };
    //UpdateState?????????????????????????? TPC-H ???????????????
    struct UpdateState {
       uint32_t update_count; // for one rotation; splits relation into 'update_count' blocks that are updated together
       //?????????????????? update_count ??????????????rotation????????????????????
       uint32_t rotation; // which of the keys 0: 0-7, 1: 8-15, 2: 16-23, 3: 24-31
       //?????????????????????
       uint32_t update; // current update update_count ... 1
       //????????
 
       explicit UpdateState(uint32_t update_count)
               : update_count(update_count)
                 , rotation(0)
                 , update(update_count) {}
       //????
       //GenerateNext?? scale_factor????????????????????????????????????????????
       std::vector<std::string> GenerateNext(uint64_t scale_factor)
       {
          const uint64_t order_count = scale_factor * 1500000; //1500000 ???????
          const uint64_t max_order_key = 1 + order_count * 4;//???????? 4 ?????
          const uint64_t group_count = 1 + max_order_key / 32;
          const uint64_t first_group = (group_count * (update - 1)) / update_count;
          const uint64_t last_group = (group_count * update) / update_count;
          assert(group_count>=update_count);  
          //??????????????????????????
          //assert??????????????
 
          std::vector<std::string> result;
          result.push_back(to_string(first_group * 32));
          result.push_back(to_string(last_group * 32));
          result.push_back(to_string(rotation * 8));
          result.push_back(to_string((rotation + 1) * 8 - 1));
 
          //?????????????????
          // which of the keys 0: 0-7, 1: 8-15, 2: 16-23, 3: 24-31
          update--;
          if (update == 0) {
             update = update_count;
             rotation = (rotation + 1) % 4;
          }
          //????4?1
          return result;
       }
    };
    //???
    //UpdateState state(4); // update_count = 4, rotation = 0, update = 4
    //result = {"4500000", "6000032", "0", "7"};
    //
 
 
    static string TwoDigitNumber(uint32_t num)
    {
       return (num<10 ? "0" : "") + to_string(num);
    }
    //???????10?0
 
    static string QStr(const string &str)
    {
       return "\"" + str + "\"";
    }
    //???
 
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
          //?22??????22?????????
          case 23: {
             return update_state.GenerateNext(scale_factor);
          }
          default: {
             throw;
          }
          //?23??????? update_count ? rotation ???????????????????????????
       }
    }
 };

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
    //????
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
    //??????
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
    //???????????
 
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
    //??????
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
    //??????????
 
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
    //?????????????
 
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
    //?????????
    static vector<uint64_t> ToIoBytes(vector<double> &scales, uint64_t total_io_bytes) {
        double sum = Sum(scales);
        assert(sum > 0);
  
        vector<uint64_t> io_bytes_in_slot(scales.size());
        for (uint32_t idx = 0; idx < scales.size(); idx++) {
           io_bytes_in_slot[idx] = static_cast<uint64_t>(total_io_bytes * scales[idx] / sum);
        }
        return io_bytes_in_slot;
     }
    //???cpu??
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
   // Seed management
   static uint64_t GetSeedForDatabaseCount() { return 6; }
   static uint64_t GetSeedForQueryCount() { return 28; }
   static uint64_t GetSeedForPatterns() { return 496; }
   static uint64_t GetSeedForQueries() { return 8128; }
   static uint64_t GetSeedForQueryArguments() { return 33550336; }

   vector<Database> databases;

   // Database generation
   void GenerateDatabases(uint64_t database_count, uint64_t total_size) {
      vector<double> db_sizes;
      double mean = 24.66794;
      double sd = 2.575434;
      
      while (true) {
         db_sizes = Utility::GenerateDeterministicLogNormal(database_count, mean, sd, 0, GetSeedForDatabaseCount());
         double sum = accumulate(db_sizes.begin(), db_sizes.end(), 0.0);

         if (sum > total_size) {
            mean *= 0.95;
            sd *= 0.95;
         } else if (sum < 0.95 * total_size) {
            mean *= 1.05;
            sd *= 1.05;
         } else {
            break;
         }
      }

      databases.resize(db_sizes.size());
      for (uint64_t idx = 0; idx < db_sizes.size(); idx++) {
         databases[idx].database_id = idx;
         databases[idx].scale_factor = static_cast<uint64_t>(db_sizes[idx] / 1_GB);
      }
   }

   // IO bytes generation
   static uint64_t RollIoBytes(const Database &database, mt19937 &gen) {
      uint32_t size_bucket = database.GetSizeBucket();
      std::cout<<size_bucket<<endl;
      // 对较小的分桶使用默认值
      if (size_bucket < 8) {
          return exp(Utility::GenerateNonExtremeNormal(19.0, 2.5, gen));
      }
      
      switch (size_bucket) {
          case 8:
          case 9: 
              return exp(Utility::GenerateNonExtremeNormal(19.8, 2.8, gen));
          case 10: 
              return exp(Utility::GenerateNonExtremeNormal(21.5, 2.9, gen));
          case 11: 
              return exp(Utility::GenerateNonExtremeNormal(22.3, 3.4, gen));
          case 12: 
              return exp(Utility::GenerateNonExtremeNormal(23.7, 3.3, gen));
          case 13: 
              return exp(Utility::GenerateNonExtremeNormal(24.0, 3.4, gen));
          case 14: 
              return exp(Utility::GenerateNonExtremeNormal(24.3, 4.2, gen));
          default: // 对更大的分桶
              return exp(Utility::GenerateNonExtremeNormal(25.0, 4.5, gen));
      }
  }

   void GenerateIoBytesForDatabases(uint64_t total_io_gb) {
      mt19937 gen(GetSeedForQueryCount());
      uint64_t total_io_bytes = total_io_gb * 1_GB;
      uint64_t actual_io = 0;

      for (auto &db : databases) {
         db.io_bytes = RollIoBytes(db, gen);
         actual_io += db.io_bytes;
      }

      double ratio = static_cast<double>(total_io_bytes) / actual_io;
      for (auto &db : databases) {
         db.io_bytes = static_cast<uint64_t>(db.io_bytes * ratio);
      }
   }

   // Query pattern generation
   void GenerateQueryArrivalDistribution() {
      mt19937 gen(GetSeedForPatterns());
      uniform_real_distribution<double> rdist(0.0, 1.0);

      vector<Pattern> patterns = {
         {1, "Random with sines", 10.0, [&](auto &s) {
            Vector::AddSequence(s, 0.2, 0, 1.0);
            Vector::AddSequenceRandomNoise(s, gen, 1.0, 0, 1.0);
            for (int i = 0; i < 8; ++i)
               Vector::AddSinusHead(s, 0.5, rdist(gen)*0.6, 0.05+rdist(gen)*0.05);
         }},
         // ... 其他模式定义
      };

      double total_weight = accumulate(patterns.begin(), patterns.end(), 0.0, 
         [](double s, const Pattern &p) { return s + p.likelihood; });

      uniform_real_distribution<double> pattern_dist(0.0, total_weight);

      for (auto &db : databases) {
         double pick = pattern_dist(gen);
         for (auto &p : patterns) {
            if ((pick -= p.likelihood) <= 0) {
               db.query_count_slots.resize(100, 0);
               p.generate(db.query_count_slots);
               db.pattern_id = p.pattern_id;
               db.pattern_description = p.description;
               break;
            }
         }
         if (Vector::Sum(db.query_count_slots) == 0) {
            db.query_count_slots[0] = 1.0; // 确保至少有一个查询
         }
      }
   }

   // Query timing generation
   void GenerateQueryArrivalTimes(uint64_t total_duration_hours) {
      mt19937 gen(GetSeedForQueries());
      const uint32_t total_ms = total_duration_hours * 3600 * 1000;

      for (auto &db : databases) {
         vector<uint64_t> io_slots = Vector::ToIoBytes(db.query_count_slots, db.io_bytes);
         vector<Database::Query> queries;
         uint32_t ms_per_slot = total_ms / io_slots.size();

         for (size_t slot = 0; slot < io_slots.size(); ++slot) {
            if (io_slots[slot] == 0) continue;

            exponential_distribution<double> exp_dist(1.0);
            uint32_t slot_start = slot * ms_per_slot;
            uint32_t current_ms = slot_start;
            uint64_t remaining_io = io_slots[slot];

            while (remaining_io > 0) {
               double interval = exp_dist(gen) * 1000.0;
               current_ms += static_cast<uint32_t>(interval);

               if (current_ms >= slot_start + ms_per_slot) break;

               uint32_t qid = GetRandomQuery(gen, db);
               uint64_t query_io = EstimateIoForQuery(qid, db.scale_factor);
               
               if (query_io > remaining_io) break;

               queries.push_back({current_ms, qid, {}});
               remaining_io -= query_io;
            }
         }

         sort(queries.begin(), queries.end(), [](const auto &a, const auto &b) {
            return a.start < b.start;
         });
         db.queries = move(queries);
      }
   }

   // Query arguments generation
   void GenerateQueryArguments() {
      mt19937 gen(GetSeedForQueryArguments());

      for (auto &db : databases) {
         TpchQueries::UpdateState update_state(1000);
         
         for (auto &query : db.queries) {
            query.arguments = TpchQueries::GenerateQueryArguments(
               query.query_id, db.scale_factor, gen, update_state);
         }
      }
   }

   // Helper methods
   static uint32_t GetRandomQuery(mt19937 &gen, Database &db) {
      uniform_int_distribution<uint32_t> dist(1, db.is_read_only ? 22 : 23);
      return dist(gen);
   }

   static uint64_t EstimateIoForQuery(uint32_t query_id, uint64_t scale_factor) {
      const array<uint64_t, 23> base_io = {
         150, 55, 112, 58, 66, 17, 61, 67, 127, 95,
         15, 64, 146, 29, 46, 32, 42, 205, 51, 46, 113, 19, 170
      };
      return base_io[query_id-1] * 1_MB * scale_factor;
   }

   // Debug output
   void DumpDatabaseIoForR() const {
      cout << "io_bytes=c(";
      for (size_t i = 0; i < databases.size(); ++i) {
         cout << databases[i].io_bytes << (i+1 < databases.size() ? "," : "");
      }
      cout << ")\n";
   }

   void DumpQueryArrivalsCSV(ostream &os) const {
      os << "db_id,pattern_id,query_time,query_id,io_bytes\n";
      for (const auto &db : databases) {
         for (const auto &q : db.queries) {
            os << db.database_id << ","
               << db.pattern_id << ","
               << q.start << ","
               << q.query_id << ","
               << EstimateIoForQuery(q.query_id, db.scale_factor) << "\n";
         }
      }
   }

private:
   struct Pattern {
      uint32_t pattern_id;
      string description;
      double likelihood;
      function<void(vector<double>&)> generate;
   };
};

int main(int argc, char** argv) {
   const uint64_t total_size = 4_TB;
   const uint64_t total_io_gb = 40000; // 40TB?IO
   const uint64_t duration_hours = 24;
   const uint32_t db_count = 20;

   Generator gen;
   gen.GenerateDatabases(db_count, total_size);
   gen.GenerateIoBytesForDatabases(total_io_gb);
   gen.GenerateQueryArrivalDistribution(); // ????????
   gen.GenerateQueryArrivalTimes(duration_hours);
   gen.GenerateQueryArguments();

   // ????
   gen.DumpDatabaseIoForR();
   for (uint32_t i = 0; i < gen.databases.size(); ++i) {
      ofstream os("query_streams/io_stream_" + to_string(i) + ".json");
      gen.databases[i].WriteJson(os);
   }

   return 0;
}