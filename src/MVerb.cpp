#include "dcb.h"
#include <random>
#include <Gamma/Gamma.h>
#include <Gamma/Delay.h>
#include <Gamma/Filter.h>
#include <thread>

struct EarlyReturnPreset {
  std::string name;
  std::vector<float> tapsLeft;
  std::vector<float> tapsRight;
};

struct Data {
  std::vector<std::string> getERPresetLabels() {
    INFO("getERPresetLabels");
    std::vector<std::string> ret;
    for(unsigned int k=0;k<ERPresets.size();k++) {
      ret.push_back(ERPresets[k].name);
    }
    return ret;
  }

  unsigned int getERPresetSize() {
    return ERPresets.size();
  }

  std::vector<EarlyReturnPreset> ERPresets={{"None",           {},                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     {}},
                                            {"Small",          {0.0070,0.5281,0.0156,0.5038,0.0233,0.3408,0.0287,0.1764,0.0362,0.2514,0.0427,0.1855,0.0475,0.2229,0.0526,0.1345,0.0567,0.1037,0.0616,0.0837,0.0658,0.0418,0.0687,0.0781,0.0727,0.0654,0.0762,0.0369,0.0796,0.0192,0.0817,0.0278,0.0839,0.0132,0.0862,0.0073,0.0888,0.0089,0.0909,0.0087,0.0924,0.0065,0.0937,0.0015,0.0957,0.0019,0.0968,0.0012,0.0982,0.0004},                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          {0.0097,0.3672,0.0147,0.3860,0.0208,0.4025,0.0274,0.3310,0.0339,0.2234,0.0383,0.1326,0.0441,0.1552,0.0477,0.1477,0.0533,0.2054,0.0582,0.1242,0.0631,0.0707,0.0678,0.1061,0.0702,0.0331,0.0735,0.0354,0.0758,0.0478,0.0778,0.0347,0.0814,0.0185,0.0836,0.0157,0.0855,0.0197,0.0877,0.0171,0.0902,0.0078,0.0915,0.0032,0.0929,0.0026,0.0947,0.0014,0.0958,0.0018,0.0973,0.0007,0.0990,0.0007}},
                                            {"Medium",         {0.0215,0.3607,0.0435,0.2480,0.0566,0.3229,0.0691,0.5000,0.0842,0.1881,0.1010,0.2056,0.1140,0.1224,0.1224,0.3358,0.1351,0.3195,0.1442,0.2803,0.1545,0.1909,0.1605,0.0535,0.1680,0.0722,0.1788,0.1138,0.1886,0.0467,0.1945,0.1731,0.2010,0.0580,0.2096,0.0392,0.2148,0.0314,0.2201,0.0301,0.2278,0.0798,0.2357,0.0421,0.2450,0.0208,0.2530,0.0484,0.2583,0.0525,0.2636,0.0335,0.2694,0.0311,0.2764,0.0455,0.2817,0.0362,0.2874,0.0252,0.2914,0.0113,0.2954,0.0207,0.2977,0.0120,0.3029,0.0067,0.3054,0.0094,0.3084,0.0135,0.3127,0.0095,0.3157,0.0111,0.3178,0.0036,0.3202,0.0064,0.3221,0.0025,0.3252,0.0016,0.3268,0.0051,0.3297,0.0029,0.3318,0.0038,0.3345,0.0016,0.3366,0.0013,0.3386,0.0009,0.3401,0.0019,0.3416,0.0012,0.3431,0.0015,0.3452,0.0011,0.3471,0.0007,0.3488,0.0003},                                                                                                                                                                                                    {0.0146,0.5281,0.0295,0.3325,0.0450,0.3889,0.0605,0.2096,0.0792,0.5082,0.0881,0.1798,0.1051,0.3287,0.1132,0.1872,0.1243,0.1184,0.1338,0.1134,0.1480,0.1400,0.1594,0.2602,0.1721,0.0610,0.1821,0.1736,0.1908,0.0738,0.1978,0.1547,0.2084,0.0842,0.2187,0.0505,0.2256,0.0906,0.2339,0.0996,0.2428,0.0490,0.2493,0.0186,0.2558,0.0164,0.2596,0.0179,0.2658,0.0298,0.2698,0.0343,0.2750,0.0107,0.2789,0.0417,0.2817,0.0235,0.2879,0.0238,0.2938,0.0202,0.2965,0.0242,0.3015,0.0209,0.3050,0.0139,0.3097,0.0039,0.3137,0.0039,0.3165,0.0096,0.3205,0.0073,0.3231,0.0052,0.3255,0.0069,0.3273,0.0044,0.3298,0.0041,0.3326,0.0026,0.3348,0.0028,0.3372,0.0014,0.3389,0.0023,0.3413,0.0012,0.3428,0.0014,0.3443,0.0006,0.3458,0.0003,0.3474,0.0004,0.3486,0.0005}},
                                            {"Large",          {0.0473,0.1344,0.0725,0.5048,0.0997,0.2057,0.1359,0.2231,0.1716,0.4355,0.1963,0.1904,0.2168,0.2274,0.2508,0.0604,0.2660,0.1671,0.2808,0.1725,0.3023,0.0481,0.3154,0.1940,0.3371,0.0651,0.3579,0.0354,0.3718,0.0504,0.3935,0.1609,0.4041,0.1459,0.4166,0.1355,0.4344,0.0747,0.4524,0.0173,0.4602,0.0452,0.4679,0.0643,0.4795,0.0377,0.4897,0.0159,0.4968,0.0433,0.5104,0.0213,0.5170,0.0115,0.5282,0.0102,0.5390,0.0091,0.5451,0.0146,0.5552,0.0371,0.5594,0.0192,0.5667,0.0218,0.5740,0.0176,0.5806,0.0242,0.5871,0.0167,0.5931,0.0184,0.6000,0.0075,0.6063,0.0060,0.6121,0.0068,0.6149,0.0138,0.6183,0.0044,0.6217,0.0035,0.6243,0.0026,0.6274,0.0017,0.6295,0.0098,0.6321,0.0054,0.6352,0.0022,0.6380,0.0011,0.6414,0.0012,0.6432,0.0062,0.6462,0.0024,0.6478,0.0032,0.6506,0.0009},                                                                                                                                                                                                    {0.0271,0.5190,0.0558,0.1827,0.0776,0.3068,0.1186,0.2801,0.1421,0.1526,0.1698,0.3249,0.1918,0.1292,0.2178,0.2828,0.2432,0.1128,0.2743,0.1884,0.2947,0.2023,0.3121,0.1118,0.3338,0.0660,0.3462,0.0931,0.3680,0.1295,0.3889,0.1430,0.4040,0.0413,0.4218,0.1122,0.4381,0.1089,0.4553,0.0691,0.4718,0.0699,0.4832,0.0375,0.4925,0.0119,0.5065,0.0181,0.5180,0.0500,0.5281,0.0228,0.5365,0.0072,0.5458,0.0366,0.5520,0.0065,0.5597,0.0115,0.5644,0.0105,0.5724,0.0063,0.5801,0.0118,0.5836,0.0198,0.5886,0.0172,0.5938,0.0081,0.5987,0.0094,0.6033,0.0029,0.6060,0.0078,0.6096,0.0149,0.6122,0.0102,0.6171,0.0144,0.6204,0.0014,0.6243,0.0038,0.6284,0.0111,0.6309,0.0107,0.6338,0.0036,0.6374,0.0035,0.6401,0.0015,0.6417,0.0052,0.6433,0.0019,0.6461,0.0033,0.6485,0.0007}},
                                            {"Huge",           {0.0588,0.6917,0.1383,0.2512,0.2158,0.5546,0.2586,0.2491,0.3078,0.1830,0.3731,0.3712,0.4214,0.1398,0.4622,0.1870,0.5004,0.1652,0.5365,0.2254,0.5604,0.1423,0.5950,0.1355,0.6233,0.1282,0.6486,0.1312,0.6725,0.1009,0.7063,0.0324,0.7380,0.0968,0.7602,0.0169,0.7854,0.0530,0.8097,0.0342,0.8303,0.0370,0.8404,0.0173,0.8587,0.0281,0.8741,0.0164,0.8825,0.0045,0.8945,0.0181,0.9063,0.0057,0.9136,0.0030,0.9214,0.0065,0.9296,0.0059,0.9373,0.0021,0.9462,0.0087,0.9541,0.0035,0.9605,0.0013,0.9648,0.0043,0.9691,0.0014,0.9746,0.0011,0.9774,0.0032,0.9818,0.0020,0.9853,0.0042,0.9889,0.0030,0.9923,0.0034,0.9941,0.0021,0.9976,0.0009,0.9986,0.0008},                                                                                                                                                                                                                                                                                                                                  {0.0665,0.4406,0.1335,0.6615,0.1848,0.2284,0.2579,0.4064,0.3293,0.1433,0.3756,0.3222,0.4157,0.3572,0.4686,0.3280,0.5206,0.1134,0.5461,0.0540,0.5867,0.0473,0.6281,0.1018,0.6516,0.1285,0.6709,0.0617,0.6979,0.0360,0.7173,0.1026,0.7481,0.0621,0.7690,0.0585,0.7943,0.0340,0.8072,0.0170,0.8177,0.0092,0.8345,0.0369,0.8511,0.0369,0.8621,0.0251,0.8740,0.0109,0.8849,0.0135,0.8956,0.0118,0.9026,0.0187,0.9110,0.0182,0.9225,0.0034,0.9310,0.0083,0.9354,0.0058,0.9420,0.0040,0.9464,0.0028,0.9549,0.0090,0.9590,0.0076,0.9654,0.0030,0.9691,0.0041,0.9729,0.0009,0.9757,0.0024,0.9787,0.0049,0.9823,0.0040,0.9847,0.0025,0.9898,0.0005,0.9922,0.0022,0.9935,0.0025,0.9964,0.0027,0.9992,0.0007}},
                                            {"Long Random",    {0.0131,0.6191,0.0518,0.4595,0.0800,0.4688,0.2461,0.2679,0.3826,0.1198,0.5176,0.2924,0.6806,0.0293,0.8211,0.0327,1.0693,0.3318,1.2952,0.1426,1.3079,0.1021,1.4337,0.1293,1.4977,0.2383,1.6702,0.0181,1.7214,0.2042,1.8849,0.0780,2.1279,0.0160,2.2836,0.0061,2.4276,0.0390,2.5733,0.1090,2.7520,0.0047,2.8650,0.0077,3.1026,0.0005},                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      {0.1591,0.4892,0.2634,0.1430,0.3918,0.0978,0.5004,0.0675,0.7004,0.1285,0.7251,0.0251,0.9368,0.4531,1.0770,0.0022,1.1426,0.0132,1.3189,0.1608,1.3550,0.0512,1.4347,0.0224,1.4739,0.1401,1.6996,0.1680,1.9292,0.1481,2.1435,0.2463,2.1991,0.1748,2.3805,0.1802,2.4796,0.0105,2.6615,0.0049,2.8115,0.0517,2.9687,0.0468,2.9899,0.0095,3.1554,0.0496}},
                                            {"Short Backwards",{0.0022,0.0070,0.0040,0.0014,0.0054,0.0120,0.0085,0.0075,0.0106,0.0156,0.0141,0.0089,0.0176,0.0083,0.0200,0.0227,0.0225,0.0189,0.0253,0.0121,0.0284,0.0118,0.0367,0.0193,0.0431,0.0163,0.0477,0.0260,0.0558,0.0259,0.0632,0.0515,0.0694,0.0266,0.0790,0.0279,0.0873,0.0712,0.1075,0.1212,0.1286,0.0938,0.1433,0.1305,0.1591,0.0929,0.1713,0.2410,0.1982,0.1409,0.2144,0.3512,0.2672,0.5038,0.3293,0.3827},                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                {0.0019,0.0107,0.0030,0.0031,0.0049,0.0068,0.0066,0.0050,0.0098,0.0090,0.0132,0.0080,0.0165,0.0085,0.0196,0.0071,0.0221,0.0143,0.0247,0.0086,0.0316,0.0164,0.0374,0.0160,0.0416,0.0110,0.0511,0.0167,0.0619,0.0191,0.0730,0.0233,0.0887,0.0313,0.1037,0.0484,0.1114,0.0912,0.1219,0.0980,0.1482,0.1220,0.1806,0.2021,0.2057,0.2059,0.2382,0.2379,0.2550,0.2536,0.3112,0.6474}},
                                            {"Long Backwards", {0.0021,0.0008,0.0050,0.0006,0.0065,0.0007,0.0092,0.0014,0.0124,0.0028,0.0145,0.0032,0.0166,0.0015,0.0225,0.0018,0.0294,0.0030,0.0345,0.0077,0.0405,0.0056,0.0454,0.0096,0.0508,0.0088,0.0593,0.0082,0.0643,0.0074,0.0743,0.0182,0.0874,0.0103,0.0986,0.0270,0.1143,0.0135,0.1370,0.0327,0.1633,0.0420,0.1823,0.0708,0.2028,0.0842,0.2258,0.0962,0.2482,0.0513,0.2856,0.1035,0.3132,0.1229,0.3398,0.0721,0.3742,0.0996,0.4199,0.1817,0.4914,0.3000,0.5557,0.1649,0.6181,0.4180,0.6689,0.5216,0.7310,0.5185},                                                                                                                                                                                                                                                                                                                                                                                                                                                                              {0.0024,0.0007,0.0053,0.0006,0.0090,0.0034,0.0138,0.0026,0.0196,0.0016,0.0250,0.0080,0.0292,0.0051,0.0346,0.0039,0.0409,0.0089,0.0459,0.0067,0.0589,0.0132,0.0702,0.0192,0.0781,0.0211,0.0964,0.0239,0.1052,0.0201,0.1212,0.0226,0.1428,0.0147,0.1547,0.0418,0.1849,0.0232,0.2110,0.0975,0.2425,0.0620,0.2851,0.0963,0.3366,0.1248,0.3645,0.1321,0.4079,0.1293,0.4433,0.1425,0.5031,0.3787,0.5416,0.5061,0.6336,0.2865,0.7434,0.6477}},
                                            {"Strange1",       {0.0137,0.2939,0.0763,0.8381,0.2189,0.7019,0.2531,0.2366,0.3822,0.3756,0.4670,0.0751,0.4821,0.0870,0.4930,0.0794,0.5087,0.1733,0.5633,0.0657,0.6078,0.0218,0.6410,0.0113,0.6473,0.0246,0.6575,0.0513,0.6669,0.0431,0.6693,0.0392,0.6916,0.0050,0.6997,0.0192,0.7091,0.0186,0.7174,0.0105,0.7284,0.0254,0.7366,0.0221,0.7390,0.0112,0.7446,0.0029,0.7470,0.0211,0.7495,0.0006},                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            {0.0036,0.0052,0.0069,0.0105,0.0096,0.0190,0.0138,0.0235,0.0150,0.0018,0.0231,0.0012,0.0340,0.0022,0.0355,0.0154,0.0415,0.0057,0.0538,0.0237,0.0722,0.0037,0.0839,0.0291,0.1027,0.0500,0.1163,0.0367,0.1375,0.0114,0.1749,0.0156,0.2002,0.0635,0.2215,0.0660,0.2777,0.0517,0.3481,0.1666,0.3871,0.2406,0.4851,0.1022,0.5305,0.2043,0.5910,0.4109,0.6346,0.5573,0.7212,0.5535,0.8981,0.5854}},
                                            {"Strange2",       {0.0306,0.3604,0.2779,0.6327,0.3687,0.2979,0.5186,0.4202,0.6927,0.3695,0.7185,0.2370,0.8703,0.3283,0.9138,0.1334,0.9610,0.1183,1.0656,0.2089,1.1153,0.0835,1.1933,0.0954,1.1974,0.0609,1.2972,0.1078,1.3243,0.0720,1.3498,0.0840,1.4191,0.0694,1.4479,0.0572,1.4992,0.0449,1.5256,0.0186,1.5704,0.0470,1.5852,0.0202,1.6090,0.0106,1.6165,0.0302,1.6440,0.0204,1.6557,0.0042,1.6582,0.0223,1.6810,0.0054,1.6814,0.0064,1.6943,0.0075,1.6988,0.0032,1.7064,0.0027,1.7073,0.0064,1.7124,0.0091,1.7150,0.0015,1.7218,0.0043,1.7308,0.0116,1.7335,0.0122,1.7355,0.0011,1.7433,0.0154,1.7466,0.0084,1.7487,0.0139,1.7503,0.0123,1.7520,0.0036,1.7561,0.0097,1.7565,0.0041,1.7586,0.0016,1.7657,0.0132,1.7704,0.0038,1.7748,0.0020,1.7777,0.0053,1.7783,0.0056,1.7791,0.0017,1.7818,0.0058,1.7822,0.0089,1.7844,0.0074,1.7863,0.0009,1.7878,0.0016,1.7899,0.0061,1.7919,0.0073,1.7925,0.0025,1.7941,0.0045,1.7956,0.0060,1.7958,0.0088,1.7963,0.0010,1.7965,0.0006,1.7977,0.0078,1.7988,0.0026},{0.0011,0.0055,0.0022,0.0063,0.0027,0.0089,0.0034,0.0009,0.0049,0.0010,0.0064,0.0005,0.0069,0.0044,0.0091,0.0027,0.0103,0.0099,0.0112,0.0017,0.0131,0.0018,0.0142,0.0008,0.0159,0.0010,0.0188,0.0034,0.0207,0.0055,0.0245,0.0005,0.0262,0.0094,0.0312,0.0057,0.0344,0.0051,0.0402,0.0044,0.0404,0.0102,0.0433,0.0044,0.0435,0.0034,0.0489,0.0087,0.0512,0.0108,0.0605,0.0046,0.0702,0.0010,0.0734,0.0121,0.0839,0.0135,0.0985,0.0151,0.1014,0.0203,0.1041,0.0043,0.1114,0.0150,0.1216,0.0182,0.1293,0.0220,0.1299,0.0169,0.1312,0.0046,0.1453,0.0046,0.1527,0.0062,0.1545,0.0192,0.1578,0.0092,0.1655,0.0053,0.1754,0.0301,0.1967,0.0122,0.2289,0.0233,0.2353,0.0131,0.2632,0.0396,0.2873,0.0171,0.3348,0.0454,0.3872,0.0398,0.4484,0.0244,0.4913,0.0693,0.5424,0.0820,0.5668,0.1112,0.6054,0.0635,0.6669,0.1016,0.7211,0.1217,0.7541,0.1756,0.8759,0.1688,0.9106,0.1932,1.0384,0.1542,1.0732,0.1598,1.0767,0.2409,1.0988,0.1879,1.2422,0.3049,1.3480,0.3001,1.4961,0.3374,1.5886,0.2791,1.5957,0.3366,1.8248,0.2962}}};
};

struct Multitaps {
  gam::Delay<> delay;
  std::vector<float> times;
  std::vector<float> gains;
  unsigned int tapCount;
  bool isInitializing=false;

  void initialize(const std::vector<float> &parameters) {
    isInitializing=true;
    times.resize(0);
    gains.resize(0);
    float maxDelay=0;
    tapCount=0;
    for(unsigned int i=0,n=parameters.size();i<n;) {
      if(maxDelay<parameters[i]) {
        maxDelay=parameters[i];
      }
      times.push_back((float)parameters[i++]);
      gains.push_back((float)parameters[i++]);
      tapCount++;
    }
    delay.maxDelay(maxDelay+.1f);
    isInitializing=false;
  }

  float process(float input,float amp) {
    float output=0;
    if(!isInitializing) {
      if(tapCount>0) {
        delay.write(input);
        for(unsigned int tap=0;tap<tapCount;tap++) {
          output+=(delay.read(times[tap])*gains[tap]*amp);
        }
      }
    }
    return output;
  }
};

struct MeshNode {
  gam::Delay<> delay[4];
  float up=0.f;
  float right=0.f;
  float down=0.f;
  float left=0.f;
  float dly=0.f;
  float fb=0.f;

  MeshNode() {
    delay[0].maxDelay(1.);
    delay[1].maxDelay(1.);
    delay[2].maxDelay(1.);
    delay[3].maxDelay(1.);
  };

  void process(float upIn,float rightIn,float downIn,float leftIn) {
    float factor=(upIn+rightIn+downIn+leftIn)*-.5f;
    delay[0].write(upIn+factor);
    up=delay[0].read(dly)*fb;
    delay[1].write(rightIn+factor);
    right=delay[1].read(dly)*fb;
    delay[2].write(downIn+factor);
    down=delay[2].read(dly)*fb;
    delay[3].write(leftIn+factor);
    left=delay[3].read(dly)*fb;
  };
};

struct MVerb : Module {
  enum ParamId {
    ER_PRESET_PARAM,ER_AMP_PARAM,FB_PARAM,WET_PARAM,MOD_AMT_PARAM,RES_PARAMS,PARAMS_LEN=RES_PARAMS+25
  };
  enum InputId {
    RND_0_15_INPUT,RND_16_24_INPUT,L_INPUT,R_INPUT,WET_CV_INPUT,FB_CV_INPUT,ER_CV_INPUT,ERAMP_INPUT,INPUTS_LEN
  };
  enum OutputId {
    L_OUTPUT,R_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  gam::BlockDC<> blockDcINL;
  gam::BlockDC<> blockDcINR;
  gam::BlockDC<> blockDcOutL;
  gam::BlockDC<> blockDcOutR;

  Multitaps multiTapsLeft;
  Multitaps multiTapsRight;
  Data data;
  MeshNode m[25];
  float res[25];
  bool updateER;
  float wet=0;
  float fbIn=0;
  float erAmp=0;
  float sampleTime=1.f/48000;
  bool useThread=true;
  std::atomic<bool> run;
  rack::dsp::RingBuffer<Vec,64> outBuffer;
  rack::dsp::RingBuffer<Vec,64> inBuffer;
  //rack::dsp::RingBuffer<float,64> outBuffer[2];
  //rack::dsp::RingBuffer<float,64> inBuffer[2];
  dsp::ClockDivider paramDivider;
  std::thread thread=std::thread([this] {
    run=true;
    while(run) {
      if(!outBuffer.full()&&!inBuffer.empty()) {
        Vec in=inBuffer.shift();
        Vec out=_process(in.x,in.y);
        outBuffer.push(out);
      } else {
        std::this_thread::sleep_for(std::chrono::duration<double>(sampleTime));
      }
    }
  });

  void onRemove() override {
    run=false;
    if(thread.joinable()) {
      thread.join();
    }
  }

  void initializeER() {
    int er=params[ER_PRESET_PARAM].getValue();
    INFO("init ER %d",er);
    multiTapsLeft.initialize(data.ERPresets[er].tapsLeft);
    multiTapsRight.initialize(data.ERPresets[er].tapsRight);
  }

  MVerb() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configSwitch(ER_PRESET_PARAM,0,data.getERPresetSize()-1,0,"Early Reflection Presets",data.getERPresetLabels());
    getParamQuantity(ER_PRESET_PARAM)->snapEnabled=true;
    configParam(ER_AMP_PARAM,0,1,0,"Early Reflection Amp");
    configParam(WET_PARAM,0,1,0.5,"Wet");
    configParam(FB_PARAM,0,1,0.5,"Feedback");
    configParam(MOD_AMT_PARAM,0,1,0.f,"Mod Amount"," %",0.f,100.f);

    for(int k=0;k<25;k++) {
      configParam(RES_PARAMS+k,1,14,1,"Freq "+std::to_string(k+1)," HZ",2,1);
    }
    configInput(L_INPUT,"Left");
    configInput(R_INPUT,"Right");
    configInput(WET_CV_INPUT,"Wet");
    configInput(FB_CV_INPUT,"Feedback");
    configInput(ER_CV_INPUT,"Early Reflection Select Input");
    configInput(ERAMP_INPUT,"Early Reflection Amp Input");
    configOutput(L_OUTPUT,"Left");
    configOutput(R_OUTPUT,"Right");
    configBypass(L_INPUT,L_OUTPUT);
    configBypass(R_INPUT,R_OUTPUT);
    paramDivider.setDivision(32);
  }

  void onSampleRateChange(const SampleRateChangeEvent &e) override {
    gam::sampleRate(APP->engine->getSampleRate());
  }

  void onAdd(const AddEvent &e) override {
    gam::sampleRate(APP->engine->getSampleRate());
    initializeER();
  }

  float getRes(int k) {
    float p=params[RES_PARAMS+k].getValue();
    float amt=params[MOD_AMT_PARAM].getValue();
    if(k<16&&inputs[RND_0_15_INPUT].isConnected()) {
      p+=(inputs[RND_0_15_INPUT].getVoltage(k)*amt);
    } else {
      p+=(inputs[RND_16_24_INPUT].getVoltage(k-16)*amt);
    }
    return p;
  }

  Vec _process(float inL,float inR) {
    float fb=tanhf(fbIn);
    inL=blockDcINL((float)inL);
    inR=blockDcINR((float)inR);
    float aL=multiTapsLeft.process(inL,erAmp);
    float aR=multiTapsRight.process(inR,erAmp);
    for(int k=0;k<25;k++) {
      m[k].dly=1.f/powf(2.0f,getRes(k));
      m[k].fb=fb;
    }
    m[0].process(m[0].up,m[1].left,m[5].up,m[0].left);
    m[1].process(m[1].up,m[2].left,m[6].up,m[0].right);
    m[2].process(m[2].up,m[3].left,m[7].up,m[1].right);
    m[3].process(m[3].up,m[4].left,m[8].up,m[2].right);
    m[4].process(m[4].up,m[4].right,m[9].up,m[3].right);
    m[5].process(m[0].down,m[6].left,m[10].up,m[5].left);
    m[6].process(m[1].down,m[7].left,m[11].up,m[5].right);
    m[7].process(m[2].down,m[8].left,m[12].up,m[6].right);
    m[8].process(m[3].down,m[9].left,m[13].up,m[7].right);
    m[9].process(m[4].down,m[9].right,m[14].up,m[8].right);
    m[10].process(m[5].down,m[11].left,m[15].up,m[10].left);
    m[11].process(inL+aL+m[6].down,m[12].left,m[16].up,m[10].right);
    m[12].process(m[7].down,m[13].left,m[17].up,m[11].right);
    m[13].process(inR+aR+m[8].down,m[14].left,m[18].up,m[12].right);
    m[14].process(m[9].down,m[14].right,m[19].up,m[13].right);
    m[15].process(m[10].down,m[16].left,m[20].up,m[15].left);
    m[16].process(m[11].down,m[17].left,m[21].up,m[15].right);
    m[17].process(m[12].down,m[18].left,m[22].up,m[16].right);
    m[18].process(m[13].down,m[19].left,m[23].up,m[17].right);
    m[19].process(m[14].down,m[19].right,m[24].up,m[18].right);
    m[20].process(m[15].down,m[21].left,m[20].down,m[20].left);
    m[21].process(m[16].down,m[22].left,m[21].down,m[20].right);
    m[22].process(m[17].down,m[23].left,m[22].down,m[21].right);
    m[23].process(m[18].down,m[24].left,m[23].down,m[22].right);
    m[24].process(m[19].down,m[24].right,m[24].down,m[23].right);
    float reverbL=blockDcOutL(m[6].left);
    float reverbR=blockDcOutR(m[8].right);
    float outL=(reverbL*wet)+(inL*(1-wet));
    float outR=(reverbR*wet)+(inR*(1-wet));
    return {outL,outR};
  }

  void process(const ProcessArgs &args) override {
    sampleTime=args.sampleTime;
    if(updateER) {
      updateER=false;
      initializeER();
    }
    float inL=0;
    float inR=0;
    if(inputs[L_INPUT].isConnected()) {
      inL=inputs[L_INPUT].getVoltage();
      if(inputs[R_INPUT].isConnected()) {
        inR=inputs[R_INPUT].getVoltage();
      } else {
        inR=inL;
      }
      wet=inputs[WET_CV_INPUT].isConnected()?clamp(inputs[WET_CV_INPUT].getVoltage()*0.1f,0.f,1.f):params[WET_PARAM].getValue();
      if(wet==0.f) {
        outputs[L_OUTPUT].setVoltage(inL);
        outputs[R_OUTPUT].setVoltage(inR);
        return;
      }
      if(paramDivider.process()) {
        fbIn=inputs[FB_CV_INPUT].isConnected()?clamp(inputs[FB_CV_INPUT].getVoltage(),0.f,10.f)*0.5f:params[FB_PARAM].getValue()*5.f;
        erAmp=inputs[ERAMP_INPUT].isConnected()?inputs[ERAMP_INPUT].getVoltage()*0.1f:params[ER_AMP_PARAM].getValue();
      }
      Vec out={};
      if(useThread) {
        if(!inBuffer.full())
          inBuffer.push({inL,inR});
        if(!outBuffer.empty()) {
          out=outBuffer.shift();
        }
      } else {
        out=_process(inL,inR);
      }
      outputs[L_OUTPUT].setVoltage(out.x);
      outputs[R_OUTPUT].setVoltage(out.y);
    }

  }

};

template<typename T>
struct ERKnob : TrimbotWhite {
  bool *update=nullptr;

  ERKnob() : TrimbotWhite() {

  }

  void onChange(const ChangeEvent &e) override {
    if(update)
      *update=true;
    TrimbotWhite::onChange(e);
  }
};

struct MVerbWidget : ModuleWidget {
  MVerbWidget(MVerb *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/MVerb.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    for(int k=0;k<5;k++) {
      for(int j=0;j<5;j++) {
        addParam(createParam<TrimbotWhite>(mm2px(Vec(3.25f+j*7.f,MHEIGHT-110.274f+k*7.f)),module,MVerb::RES_PARAMS+k*5+j));
      }
    }
    addInput(createInput<SmallPort>(mm2px(Vec(3.25f,MHEIGHT-69.4f)),module,MVerb::RND_0_15_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(17.25f,MHEIGHT-69.4f)),module,MVerb::RND_16_24_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(31.25f,MHEIGHT-69.4f)),module,MVerb::MOD_AMT_PARAM));

    auto erPresetParam=createParam<ERKnob<MVerb>>(mm2px(Vec(6.879f,MHEIGHT-52.3f)),module,MVerb::ER_PRESET_PARAM);
    if(module) {
      erPresetParam->update=&module->updateER;
    }
    addParam(erPresetParam);

    addParam(createParam<TrimbotWhite>(mm2px(Vec(17.4f,MHEIGHT-52.3)),module,MVerb::ER_AMP_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(17.4f,MHEIGHT-44.287f)),module,MVerb::ERAMP_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(28.274f,MHEIGHT-52.3)),module,MVerb::FB_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(28.274f,MHEIGHT-44.287f)),module,MVerb::FB_CV_INPUT));


    addInput(createInput<SmallPort>(mm2px(Vec(6.8f,MHEIGHT-30.287f)),module,MVerb::L_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(6.8f,MHEIGHT-18.287f)),module,MVerb::R_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(17.295f,MHEIGHT-30.f)),module,MVerb::WET_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(17.295f,MHEIGHT-22.f)),module,MVerb::WET_CV_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(27.283f,MHEIGHT-30.287f)),module,MVerb::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(27.283f,MHEIGHT-18.287f)),module,MVerb::R_OUTPUT));

  }

  void appendContextMenu(Menu *menu) override {
    auto *module=dynamic_cast<MVerb *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(createBoolPtrMenuItem("Use Thread","",&module->useThread));
  }
};


Model *modelMVerb=createModel<MVerb,MVerbWidget>("MVerb");