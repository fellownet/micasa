#include "ed25519-donna.h"

#define curve25519_mul_noinline curve25519_mul

#define mul32x32_64(a,b) (((uint64_t)(a))*(b))

#define S1_SWINDOWSIZE 5
#define S1_TABLE_SIZE (1<<(S1_SWINDOWSIZE-2))
#define S2_SWINDOWSIZE 7
#define S2_TABLE_SIZE (1<<(S2_SWINDOWSIZE-2))

#define ALIGN(x) __attribute__((aligned(x)))

static const uint32_t reduce_mask_25 = (1 << 25) - 1;
static const uint32_t reduce_mask_26 = (1 << 26) - 1;

// multiples of p
static const uint32_t twoP0       = 0x07ffffda;
static const uint32_t twoP13579   = 0x03fffffe;
static const uint32_t twoP2468    = 0x07fffffe;
static const uint32_t fourP0      = 0x0fffffb4;
static const uint32_t fourP13579  = 0x07fffffc;
static const uint32_t fourP2468   = 0x0ffffffc;

static const bignum256modm modm_m = {
	0x1cf5d3ed, 0x20498c69, 0x2f79cd65, 0x37be77a8,
	0x00000014,	0x00000000, 0x00000000,	0x00000000,
	0x00001000
};

static const bignum256modm modm_mu = {
	0x0a2c131b, 0x3673968c, 0x06329a7e, 0x01885742,
	0x3fffeb21, 0x3fffffff, 0x3fffffff, 0x3fffffff,
	0x000fffff
};

static const ge25519 ALIGN(16) ge25519_basepoint = {
	{0x0325d51a,0x018b5823,0x00f6592a,0x0104a92d,0x01a4b31d,0x01d6dc5c,0x027118fe,0x007fd814,0x013cd6e5,0x0085a4db},
	{0x02666658,0x01999999,0x00cccccc,0x01333333,0x01999999,0x00666666,0x03333333,0x00cccccc,0x02666666,0x01999999},
	{0x00000001,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000},
	{0x01b7dda3,0x01a2ace9,0x025eadbb,0x0003ba8a,0x0083c27e,0x00abe37d,0x01274732,0x00ccacdd,0x00fd78b7,0x019e1d7c}
};

static const bignum25519 ALIGN(16) ge25519_ecd = {
	0x035978a3,0x00d37284,0x03156ebd,0x006a0a0e,0x0001c029,0x0179e898,0x03a03cbb,0x01ce7198,0x02e2b6ff,0x01480db3
};

static const bignum25519 ALIGN(16) ge25519_ec2d = {
	0x02b2f159,0x01a6e509,0x022add7a,0x00d4141d,0x00038052,0x00f3d130,0x03407977,0x019ce331,0x01c56dff,0x00901b67
};

static const bignum25519 ALIGN(16) ge25519_sqrtneg1 = {
	0x020ea0b0,0x0186c9d2,0x008f189d,0x0035697f,0x00bd0c60,0x01fbd7a7,0x02804c9e,0x01e16569,0x0004fc1d,0x00ae0c92
};

static const ge25519_niels ALIGN(16) ge25519_niels_sliding_multiples[32] = {
	{{0x0340913e,0x000e4175,0x03d673a2,0x002e8a05,0x03f4e67c,0x008f8a09,0x00c21a34,0x004cf4b8,0x01298f81,0x0113f4be},{0x018c3b85,0x0124f1bd,0x01c325f7,0x0037dc60,0x033e4cb7,0x003d42c2,0x01a44c32,0x014ca4e1,0x03a33d4b,0x001f3e74},{0x037aaa68,0x00448161,0x0093d579,0x011e6556,0x009b67a0,0x0143598c,0x01bee5ee,0x00b50b43,0x0289f0c6,0x01bc45ed}},
	{{0x00fcd265,0x0047fa29,0x034faacc,0x01ef2e0d,0x00ef4d4f,0x014bd6bd,0x00f98d10,0x014c5026,0x007555bd,0x00aae456},{0x00ee9730,0x016c2a13,0x017155e4,0x01874432,0x00096a10,0x01016732,0x01a8014f,0x011e9823,0x01b9a80f,0x01e85938},{0x01d0d889,0x01a4cfc3,0x034c4295,0x0110e1ae,0x0162508c,0x00f2db4c,0x0072a2c6,0x0098da2e,0x02f12b9b,0x0168a09a}},
	{{0x0047d6ba,0x0060b0e9,0x0136eff2,0x008a5939,0x03540053,0x0064a087,0x02788e5c,0x00be7c67,0x033eb1b5,0x005529f9},{0x00a5bb33,0x00af1102,0x01a05442,0x001e3af7,0x02354123,0x00bfec44,0x01f5862d,0x00dd7ba3,0x03146e20,0x00a51733},{0x012a8285,0x00f6fc60,0x023f9797,0x003e85ee,0x009c3820,0x01bda72d,0x01b3858d,0x00d35683,0x0296b3bb,0x010eaaf9}},
	{{0x023221b1,0x01cb26aa,0x0074f74d,0x0099ddd1,0x01b28085,0x00192c3a,0x013b27c9,0x00fc13bd,0x01d2e531,0x0075bb75},{0x004ea3bf,0x00973425,0x001a4d63,0x01d59cee,0x01d1c0d4,0x00542e49,0x01294114,0x004fce36,0x029283c9,0x01186fa9},{0x01b8b3a2,0x00db7200,0x00935e30,0x003829f5,0x02cc0d7d,0x0077adf3,0x0220dd2c,0x0014ea53,0x01c6a0f9,0x01ea7eec}},
	{{0x039d8064,0x01885f80,0x00337e6d,0x01b7a902,0x02628206,0x015eb044,0x01e30473,0x0191f2d9,0x011fadc9,0x01270169},{0x02a8632f,0x0199e2a9,0x00d8b365,0x017a8de2,0x02994279,0x0086f5b5,0x0119e4e3,0x01eb39d6,0x0338add7,0x00d2e7b4},{0x0045af1b,0x013a2fe4,0x0245e0d6,0x014538ce,0x038bfe0f,0x01d4cf16,0x037e14c9,0x0160d55e,0x0021b008,0x01cf05c8}},
	{{0x01864348,0x01d6c092,0x0070262b,0x014bb844,0x00fb5acd,0x008deb95,0x003aaab5,0x00eff474,0x00029d5c,0x0062ad66},{0x02802ade,0x01c02122,0x01c4e5f7,0x00781181,0x039767fb,0x01703406,0x0342388b,0x01f5e227,0x022546d8,0x0109d6ab},{0x016089e9,0x00cb317f,0x00949b05,0x01099417,0x000c7ad2,0x011a8622,0x0088ccda,0x01290886,0x022b53df,0x00f71954}},
	{{0x027fbf93,0x01c04ecc,0x01ed6a0d,0x004cdbbb,0x02bbf3af,0x00ad5968,0x01591955,0x0094f3a2,0x02d17602,0x00099e20},{0x02007f6d,0x003088a8,0x03db77ee,0x00d5ade6,0x02fe12ce,0x0107ba07,0x0107097d,0x00482a6f,0x02ec346f,0x008d3f5f},{0x032ea378,0x0028465c,0x028e2a6c,0x018efc6e,0x0090df9a,0x01a7e533,0x039bfc48,0x010c745d,0x03daa097,0x0125ee9b}},
	{{0x028ccf0b,0x00f36191,0x021ac081,0x012154c8,0x034e0a6e,0x01b25192,0x00180403,0x01d7eea1,0x00218d05,0x010ed735},{0x03cfeaa0,0x01b300c4,0x008da499,0x0068c4e1,0x0219230a,0x01f2d4d0,0x02defd60,0x00e565b7,0x017f12de,0x018788a4},{0x03d0b516,0x009d8be6,0x03ddcbb3,0x0071b9fe,0x03ace2bd,0x01d64270,0x032d3ec9,0x01084065,0x0210ae4d,0x01447584}},
	{{0x0020de87,0x00e19211,0x01b68102,0x00b5ac97,0x022873c0,0x01942d25,0x01271394,0x0102073f,0x02fe2482,0x01c69ff9},{0x010e9d81,0x019dbbe5,0x0089f258,0x006e06b8,0x02951883,0x018f1248,0x019b3237,0x00bc7553,0x024ddb85,0x01b4c964},{0x01c8c854,0x0060ae29,0x01406d8e,0x01cff2f9,0x00cff451,0x01778d0c,0x03ac8c41,0x01552e59,0x036559ee,0x011d1b12}},
	{{0x00741147,0x0151b219,0x01092690,0x00e877e6,0x01f4d6bb,0x0072a332,0x01cd3b03,0x00dadff2,0x0097db5e,0x0086598d},{0x01c69a2b,0x01decf1b,0x02c2fa6e,0x013b7c4f,0x037beac8,0x013a16b5,0x028e7bda,0x01f6e8ac,0x01e34fe9,0x01726947},{0x01f10e67,0x003c73de,0x022b7ea2,0x010f32c2,0x03ff776a,0x00142277,0x01d38b88,0x00776138,0x03c60822,0x01201140}},
	{{0x0236d175,0x0008748e,0x03c6476d,0x013f4cdc,0x02eed02a,0x00838a47,0x032e7210,0x018bcbb3,0x00858de4,0x01dc7826},{0x00a37fc7,0x0127b40b,0x01957884,0x011d30ad,0x02816683,0x016e0e23,0x00b76be4,0x012db115,0x02516506,0x0154ce62},{0x00451edf,0x00bd749e,0x03997342,0x01cc2c4c,0x00eb6975,0x01a59508,0x03a516cf,0x00c228ef,0x0168ff5a,0x01697b47}},
	{{0x00527359,0x01783156,0x03afd75c,0x00ce56dc,0x00e4b970,0x001cabe9,0x029e0f6d,0x0188850c,0x0135fefd,0x00066d80},{0x02150e83,0x01448abf,0x02bb0232,0x012bf259,0x033c8268,0x00711e20,0x03fc148f,0x005e0e70,0x017d8bf9,0x0112b2e2},{0x02134b83,0x001a0517,0x0182c3cc,0x00792182,0x0313d799,0x001a3ed7,0x0344547e,0x01f24a0d,0x03de6ad2,0x00543127}},
	{{0x00dca868,0x00618f27,0x015a1709,0x00ddc38a,0x0320fd13,0x0036168d,0x0371ab06,0x01783fc7,0x0391e05f,0x01e29b5d},{0x01471138,0x00fca542,0x00ca31cf,0x01ca7bad,0x0175bfbc,0x01a708ad,0x03bce212,0x01244215,0x0075bb99,0x01acad68},{0x03a0b976,0x01dc12d1,0x011aab17,0x00aba0ba,0x029806cd,0x0142f590,0x018fd8ea,0x01a01545,0x03c4ad55,0x01c971ff}},
	{{0x00d098c0,0x000afdc7,0x006cd230,0x01276af3,0x03f905b2,0x0102994c,0x002eb8a4,0x015cfbeb,0x025f855f,0x01335518},{0x01cf99b2,0x0099c574,0x01a69c88,0x00881510,0x01cd4b54,0x0112109f,0x008abdc5,0x0074647a,0x0277cb1f,0x01e53324},{0x02ac5053,0x01b109b0,0x024b095e,0x016997b3,0x02f26bb6,0x00311021,0x00197885,0x01d0a55a,0x03b6fcc8,0x01c020d5}},
	{{0x02584a34,0x00e7eee0,0x03257a03,0x011e95a3,0x011ead91,0x00536202,0x00b1ce24,0x008516c6,0x03669d6d,0x004ea4a8},{0x00773f01,0x0019c9ce,0x019f6171,0x01d4afde,0x02e33323,0x01ad29b6,0x02ead1dc,0x01ed51a5,0x01851ad0,0x001bbdfa},{0x00577de5,0x00ddc730,0x038b9952,0x00f281ae,0x01d50390,0x0002e071,0x000780ec,0x010d448d,0x01f8a2af,0x00f0a5b7}},
	{{0x031f2541,0x00d34bae,0x0323ff9d,0x003a056d,0x02e25443,0x00a1ad05,0x00d1bee8,0x002f7f8e,0x03007477,0x002a24b1},{0x0114a713,0x01457e76,0x032255d5,0x01cc647f,0x02a4bdef,0x0153d730,0x00118bcf,0x00f755ff,0x013490c7,0x01ea674e},{0x02bda3e8,0x00bb490d,0x00f291ea,0x000abf40,0x01dea321,0x002f9ce0,0x00b2b193,0x00fa54b5,0x0128302f,0x00a19d8b}},
	{{0x022ef5bd,0x01638af3,0x038c6f8a,0x01a33a3d,0x039261b2,0x01bb89b8,0x010bcf9d,0x00cf42a9,0x023d6f17,0x01da1bca},{0x00e35b25,0x000d824f,0x0152e9cf,0x00ed935d,0x020b8460,0x01c7b83f,0x00c969e5,0x01a74198,0x0046a9d9,0x00cbc768},{0x01597c6a,0x0144a99b,0x00a57551,0x0018269c,0x023c464c,0x0009b022,0x00ee39e1,0x0114c7f2,0x038a9ad2,0x01584c17}},
	{{0x03b0c0d5,0x00b30a39,0x038a6ce4,0x01ded83a,0x01c277a6,0x01010a61,0x0346d3eb,0x018d995e,0x02f2c57c,0x000c286b},{0x0092aed1,0x0125e37b,0x027ca201,0x001a6b6b,0x03290f55,0x0047ba48,0x018d916c,0x01a59062,0x013e35d4,0x0002abb1},{0x003ad2aa,0x007ddcc0,0x00c10f76,0x0001590b,0x002cfca6,0x000ed23e,0x00ee4329,0x00900f04,0x01c24065,0x0082fa70}},
	{{0x02025e60,0x003912b8,0x0327041c,0x017e5ee5,0x02c0ecec,0x015a0d1c,0x02b1ce7c,0x0062220b,0x0145067e,0x01a5d931},{0x009673a6,0x00e1f609,0x00927c2a,0x016faa37,0x01650ef0,0x016f63b5,0x03cd40e1,0x003bc38f,0x0361f0ac,0x01d42acc},{0x02f81037,0x008ca0e8,0x017e23d1,0x011debfe,0x01bcbb68,0x002e2563,0x03e8add6,0x000816e5,0x03fb7075,0x0153e5ac}},
	{{0x02b11ecd,0x016bf185,0x008f22ef,0x00e7d2bb,0x0225d92e,0x00ece785,0x00508873,0x017e16f5,0x01fbe85d,0x01e39a0e},{0x01669279,0x017c810a,0x024941f5,0x0023ebeb,0x00eb7688,0x005760f1,0x02ca4146,0x0073cde7,0x0052bb75,0x00f5ffa7},{0x03b8856b,0x00cb7dcd,0x02f14e06,0x001820d0,0x01d74175,0x00e59e22,0x03fba550,0x00484641,0x03350088,0x01c3c9a3}},
	{{0x00dcf355,0x0104481c,0x0022e464,0x01f73fe7,0x00e03325,0x0152b698,0x02ef769a,0x00973663,0x00039b8c,0x0101395b},{0x01805f47,0x019160ec,0x03832cd0,0x008b06eb,0x03d4d717,0x004cb006,0x03a75b8f,0x013b3d30,0x01cfad88,0x01f034d1},{0x0078338a,0x01c7d2e3,0x02bc2b23,0x018b3f05,0x0280d9aa,0x005f3d44,0x0220a95a,0x00eeeb97,0x0362aaec,0x00835d51}},
	{{0x01b9f543,0x013fac4d,0x02ad93ae,0x018ef464,0x0212cdf7,0x01138ba9,0x011583ab,0x019c3d26,0x028790b4,0x00e2e2b6},{0x033bb758,0x01f0dbf1,0x03734bd1,0x0129b1e5,0x02b3950e,0x003bc922,0x01a53ec8,0x018c5532,0x006f3cee,0x00ae3c79},{0x0351f95d,0x0012a737,0x03d596b8,0x017658fe,0x00ace54a,0x008b66da,0x0036c599,0x012a63a2,0x032ceba1,0x00126bac}},
	{{0x03dcfe7e,0x019f4f18,0x01c81aee,0x0044bc2b,0x00827165,0x014f7c13,0x03b430f0,0x00bf96cc,0x020c8d62,0x01471997},{0x01fc7931,0x001f42dd,0x00ba754a,0x005bd339,0x003fbe49,0x016b3930,0x012a159c,0x009f83b0,0x03530f67,0x01e57b85},{0x02ecbd81,0x0096c294,0x01fce4a9,0x017701a5,0x0175047d,0x00ee4a31,0x012686e5,0x008efcd4,0x0349dc54,0x01b3466f}},
	{{0x02179ca3,0x01d86414,0x03f0afd0,0x00305964,0x015c7428,0x0099711e,0x015d5442,0x00c71014,0x01b40b2e,0x01d483cf},{0x01afc386,0x01984859,0x036203ff,0x0045c6a8,0x0020a8aa,0x00990baa,0x03313f10,0x007ceede,0x027429e4,0x017806ce},{0x039357a1,0x0142f8f4,0x0294a7b6,0x00eaccf4,0x0259edb3,0x01311e6e,0x004d326f,0x0130c346,0x01ccef3c,0x01c424b2}},
	{{0x0364918c,0x00148fc0,0x01638a7b,0x01a1fd5b,0x028ad013,0x0081e5a4,0x01a54f33,0x0174e101,0x003d0257,0x003a856c},{0x00051dcf,0x00f62b1d,0x0143d0ad,0x0042adbd,0x000fda90,0x01743ceb,0x0173e5e4,0x017bc749,0x03b7137a,0x0105ce96},{0x00f9218a,0x015b8c7c,0x00e102f8,0x0158d7e2,0x0169a5b8,0x00b2f176,0x018b347a,0x014cfef2,0x0214a4e3,0x017f1595}},
	{{0x006d7ae5,0x0195c371,0x0391e26d,0x0062a7c6,0x003f42ab,0x010dad86,0x024f8198,0x01542b2a,0x0014c454,0x0189c471},{0x0390988e,0x00b8799d,0x02e44912,0x0078e2e6,0x00075654,0x01923eed,0x0040cd72,0x00a37c76,0x0009d466,0x00c8531d},{0x02651770,0x00609d01,0x0286c265,0x0134513c,0x00ee9281,0x005d223c,0x035c760c,0x00679b36,0x0073ecb8,0x016faa50}},
	{{0x02c89be4,0x016fc244,0x02f38c83,0x018beb72,0x02b3ce2c,0x0097b065,0x034f017b,0x01dd957f,0x00148f61,0x00eab357},{0x0343d2f8,0x003398fc,0x011e368e,0x00782a1f,0x00019eea,0x00117b6f,0x0128d0d1,0x01a5e6bb,0x01944f1b,0x012b41e1},{0x03318301,0x018ecd30,0x0104d0b1,0x0038398b,0x03726701,0x019da88c,0x002d9769,0x00a7a681,0x031d9028,0x00ebfc32}},
	{{0x0220405e,0x0171face,0x02d930f8,0x017f6d6a,0x023b8c47,0x0129d5f9,0x02972456,0x00a3a524,0x006f4cd2,0x004439fa},{0x00c53505,0x0190c2fd,0x00507244,0x009930f9,0x01a39270,0x01d327c6,0x0399bc47,0x01cfe13d,0x0332bd99,0x00b33e7d},{0x0203f5e4,0x003627b5,0x00018af8,0x01478581,0x004a2218,0x002e3bb7,0x039384d0,0x0146ea62,0x020b9693,0x0017155f}},
	{{0x03c97e6f,0x00738c47,0x03b5db1f,0x01808fcf,0x01e8fc98,0x01ed25dd,0x01bf5045,0x00eb5c2b,0x0178fe98,0x01b85530},{0x01c20eb0,0x01aeec22,0x030b9eee,0x01b7d07e,0x0187e16f,0x014421fb,0x009fa731,0x0040b6d7,0x00841861,0x00a27fbc},{0x02d69abf,0x0058cdbf,0x0129f9ec,0x013c19ae,0x026c5b93,0x013a7fe7,0x004bb2ba,0x0063226f,0x002a95ca,0x01abefd9}},
	{{0x02f5d2c1,0x00378318,0x03734fb5,0x01258073,0x0263f0f6,0x01ad70e0,0x01b56d06,0x01188fbd,0x011b9503,0x0036d2e1},{0x0113a8cc,0x01541c3e,0x02ac2bbc,0x01d95867,0x01f47459,0x00ead489,0x00ab5b48,0x01db3b45,0x00edb801,0x004b024f},{0x00b8190f,0x011fe4c2,0x00621f82,0x010508d7,0x001a5a76,0x00c7d7fd,0x03aab96d,0x019cd9dc,0x019c6635,0x00ceaa1e}},
	{{0x01085cf2,0x01fd47af,0x03e3f5e1,0x004b3e99,0x01e3d46a,0x0060033c,0x015ff0a8,0x0150cdd8,0x029e8e21,0x008cf1bc},{0x00156cb1,0x003d623f,0x01a4f069,0x00d8d053,0x01b68aea,0x01ca5ab6,0x0316ae43,0x0134dc44,0x001c8d58,0x0084b343},{0x0318c781,0x0135441f,0x03a51a5e,0x019293f4,0x0048bb37,0x013d3341,0x0143151e,0x019c74e1,0x00911914,0x0076ddde}},
	{{0x006bc26f,0x00d48e5f,0x00227bbe,0x00629ea8,0x01ea5f8b,0x0179a330,0x027a1d5f,0x01bf8f8e,0x02d26e2a,0x00c6b65e},{0x01701ab6,0x0051da77,0x01b4b667,0x00a0ce7c,0x038ae37b,0x012ac852,0x03a0b0fe,0x0097c2bb,0x00a017d2,0x01eb8b2a},{0x0120b962,0x0005fb42,0x0353b6fd,0x0061f8ce,0x007a1463,0x01560a64,0x00e0a792,0x01907c92,0x013a6622,0x007b47f1}}
};

int ed25519_verify(const unsigned char *x, const unsigned char *y, size_t len) {
	size_t differentbits = 0;
	while (len--) {
		differentbits |= (*x++ ^ *y++);
	}
	return (int) (1 & ((differentbits - 1) >> 8));
};

/*
 * In:  b =   2^5 - 2^0
 * Out: b = 2^250 - 2^0
 */
void curve25519_pow_two5mtwo0_two250mtwo0(bignum25519 b) {
	bignum25519 ALIGN(16) t0,c;

	/* 2^5  - 2^0 */ /* b */
	/* 2^10 - 2^5 */ curve25519_square_times(t0, b, 5);
	/* 2^10 - 2^0 */ curve25519_mul_noinline(b, t0, b);
	/* 2^20 - 2^10 */ curve25519_square_times(t0, b, 10);
	/* 2^20 - 2^0 */ curve25519_mul_noinline(c, t0, b);
	/* 2^40 - 2^20 */ curve25519_square_times(t0, c, 20);
	/* 2^40 - 2^0 */ curve25519_mul_noinline(t0, t0, c);
	/* 2^50 - 2^10 */ curve25519_square_times(t0, t0, 10);
	/* 2^50 - 2^0 */ curve25519_mul_noinline(b, t0, b);
	/* 2^100 - 2^50 */ curve25519_square_times(t0, b, 50);
	/* 2^100 - 2^0 */ curve25519_mul_noinline(c, t0, b);
	/* 2^200 - 2^100 */ curve25519_square_times(t0, c, 100);
	/* 2^200 - 2^0 */ curve25519_mul_noinline(t0, t0, c);
	/* 2^250 - 2^50 */ curve25519_square_times(t0, t0, 50);
	/* 2^250 - 2^0 */ curve25519_mul_noinline(b, t0, b);
 };

/*
 * z^(p - 2) = z(2^255 - 21)
 */
void curve25519_recip(bignum25519 out, const bignum25519 z) {
	bignum25519 ALIGN(16) a,t0,b;

	/* 2 */ curve25519_square_times(a, z, 1); /* a = 2 */
	/* 8 */ curve25519_square_times(t0, a, 2);
	/* 9 */ curve25519_mul_noinline(b, t0, z); /* b = 9 */
	/* 11 */ curve25519_mul_noinline(a, b, a); /* a = 11 */
	/* 22 */ curve25519_square_times(t0, a, 1);
	/* 2^5 - 2^0 = 31 */ curve25519_mul_noinline(b, t0, b);
	/* 2^250 - 2^0 */ curve25519_pow_two5mtwo0_two250mtwo0(b);
	/* 2^255 - 2^5 */ curve25519_square_times(b, b, 5);
	/* 2^255 - 21 */ curve25519_mul_noinline(out, b, a);
};

/*
 * z^((p-5)/8) = z^(2^252 - 3)
 */
void curve25519_pow_two252m3(bignum25519 two252m3, const bignum25519 z) {
	bignum25519 ALIGN(16) b,c,t0;

	/* 2 */ curve25519_square_times(c, z, 1); /* c = 2 */
	/* 8 */ curve25519_square_times(t0, c, 2); /* t0 = 8 */
	/* 9 */ curve25519_mul_noinline(b, t0, z); /* b = 9 */
	/* 11 */ curve25519_mul_noinline(c, b, c); /* c = 11 */
	/* 22 */ curve25519_square_times(t0, c, 1);
	/* 2^5 - 2^0 = 31 */ curve25519_mul_noinline(b, t0, b);
	/* 2^250 - 2^0 */ curve25519_pow_two5mtwo0_two250mtwo0(b);
	/* 2^252 - 2^2 */ curve25519_square_times(b, b, 2);
	/* 2^252 - 3 */ curve25519_mul_noinline(two252m3, b, z);
};

/* out = in */
void curve25519_copy(bignum25519 out, const bignum25519 in) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	out[3] = in[3];
	out[4] = in[4];
	out[5] = in[5];
	out[6] = in[6];
	out[7] = in[7];
	out[8] = in[8];
	out[9] = in[9];
};

/* out = a + b */
void curve25519_add(bignum25519 out, const bignum25519 a, const bignum25519 b) {
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
	out[3] = a[3] + b[3];
	out[4] = a[4] + b[4];
	out[5] = a[5] + b[5];
	out[6] = a[6] + b[6];
	out[7] = a[7] + b[7];
	out[8] = a[8] + b[8];
	out[9] = a[9] + b[9];
};

void curve25519_add_after_basic(bignum25519 out, const bignum25519 a, const bignum25519 b) {
	uint32_t c;
	out[0] = a[0] + b[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
	out[1] = a[1] + b[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
	out[2] = a[2] + b[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
	out[3] = a[3] + b[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
	out[4] = a[4] + b[4] + c; c = (out[4] >> 26); out[4] &= reduce_mask_26;
	out[5] = a[5] + b[5] + c; c = (out[5] >> 25); out[5] &= reduce_mask_25;
	out[6] = a[6] + b[6] + c; c = (out[6] >> 26); out[6] &= reduce_mask_26;
	out[7] = a[7] + b[7] + c; c = (out[7] >> 25); out[7] &= reduce_mask_25;
	out[8] = a[8] + b[8] + c; c = (out[8] >> 26); out[8] &= reduce_mask_26;
	out[9] = a[9] + b[9] + c; c = (out[9] >> 25); out[9] &= reduce_mask_25;
	out[0] += 19 * c;
};

void curve25519_add_reduce(bignum25519 out, const bignum25519 a, const bignum25519 b) {
	uint32_t c;
	out[0] = a[0] + b[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
	out[1] = a[1] + b[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
	out[2] = a[2] + b[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
	out[3] = a[3] + b[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
	out[4] = a[4] + b[4] + c; c = (out[4] >> 26); out[4] &= reduce_mask_26;
	out[5] = a[5] + b[5] + c; c = (out[5] >> 25); out[5] &= reduce_mask_25;
	out[6] = a[6] + b[6] + c; c = (out[6] >> 26); out[6] &= reduce_mask_26;
	out[7] = a[7] + b[7] + c; c = (out[7] >> 25); out[7] &= reduce_mask_25;
	out[8] = a[8] + b[8] + c; c = (out[8] >> 26); out[8] &= reduce_mask_26;
	out[9] = a[9] + b[9] + c; c = (out[9] >> 25); out[9] &= reduce_mask_25;
	out[0] += 19 * c;
};

/* out = a - b */
void curve25519_sub(bignum25519 out, const bignum25519 a, const bignum25519 b) {
	uint32_t c;
	out[0] = twoP0     + a[0] - b[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
	out[1] = twoP13579 + a[1] - b[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
	out[2] = twoP2468  + a[2] - b[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
	out[3] = twoP13579 + a[3] - b[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
	out[4] = twoP2468  + a[4] - b[4] + c;
	out[5] = twoP13579 + a[5] - b[5]    ;
	out[6] = twoP2468  + a[6] - b[6]    ;
	out[7] = twoP13579 + a[7] - b[7]    ;
	out[8] = twoP2468  + a[8] - b[8]    ;
	out[9] = twoP13579 + a[9] - b[9]    ;
};

/* out = a - b, where a is the result of a basic op (add,sub) */
void curve25519_sub_after_basic(bignum25519 out, const bignum25519 a, const bignum25519 b) {
	uint32_t c;
	out[0] = fourP0     + a[0] - b[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
	out[1] = fourP13579 + a[1] - b[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
	out[2] = fourP2468  + a[2] - b[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
	out[3] = fourP13579 + a[3] - b[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
	out[4] = fourP2468  + a[4] - b[4] + c; c = (out[4] >> 26); out[4] &= reduce_mask_26;
	out[5] = fourP13579 + a[5] - b[5] + c; c = (out[5] >> 25); out[5] &= reduce_mask_25;
	out[6] = fourP2468  + a[6] - b[6] + c; c = (out[6] >> 26); out[6] &= reduce_mask_26;
	out[7] = fourP13579 + a[7] - b[7] + c; c = (out[7] >> 25); out[7] &= reduce_mask_25;
	out[8] = fourP2468  + a[8] - b[8] + c; c = (out[8] >> 26); out[8] &= reduce_mask_26;
	out[9] = fourP13579 + a[9] - b[9] + c; c = (out[9] >> 25); out[9] &= reduce_mask_25;
	out[0] += 19 * c;
};

void curve25519_sub_reduce(bignum25519 out, const bignum25519 a, const bignum25519 b) {
	uint32_t c;
	out[0] = fourP0     + a[0] - b[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
	out[1] = fourP13579 + a[1] - b[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
	out[2] = fourP2468  + a[2] - b[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
	out[3] = fourP13579 + a[3] - b[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
	out[4] = fourP2468  + a[4] - b[4] + c; c = (out[4] >> 26); out[4] &= reduce_mask_26;
	out[5] = fourP13579 + a[5] - b[5] + c; c = (out[5] >> 25); out[5] &= reduce_mask_25;
	out[6] = fourP2468  + a[6] - b[6] + c; c = (out[6] >> 26); out[6] &= reduce_mask_26;
	out[7] = fourP13579 + a[7] - b[7] + c; c = (out[7] >> 25); out[7] &= reduce_mask_25;
	out[8] = fourP2468  + a[8] - b[8] + c; c = (out[8] >> 26); out[8] &= reduce_mask_26;
	out[9] = fourP13579 + a[9] - b[9] + c; c = (out[9] >> 25); out[9] &= reduce_mask_25;
	out[0] += 19 * c;
};

/* out = -a */
void curve25519_neg(bignum25519 out, const bignum25519 a) {
	uint32_t c;
	out[0] = twoP0     - a[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
	out[1] = twoP13579 - a[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
	out[2] = twoP2468  - a[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
	out[3] = twoP13579 - a[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
	out[4] = twoP2468  - a[4] + c; c = (out[4] >> 26); out[4] &= reduce_mask_26;
	out[5] = twoP13579 - a[5] + c; c = (out[5] >> 25); out[5] &= reduce_mask_25;
	out[6] = twoP2468  - a[6] + c; c = (out[6] >> 26); out[6] &= reduce_mask_26;
	out[7] = twoP13579 - a[7] + c; c = (out[7] >> 25); out[7] &= reduce_mask_25;
	out[8] = twoP2468  - a[8] + c; c = (out[8] >> 26); out[8] &= reduce_mask_26;
	out[9] = twoP13579 - a[9] + c; c = (out[9] >> 25); out[9] &= reduce_mask_25;
	out[0] += 19 * c;
};

void curve25519_mul(bignum25519 out, const bignum25519 a, const bignum25519 b) {
	uint32_t r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
	uint32_t s0,s1,s2,s3,s4,s5,s6,s7,s8,s9;
	uint64_t m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,c;
	uint32_t p;

	r0 = b[0];
	r1 = b[1];
	r2 = b[2];
	r3 = b[3];
	r4 = b[4];
	r5 = b[5];
	r6 = b[6];
	r7 = b[7];
	r8 = b[8];
	r9 = b[9];

	s0 = a[0];
	s1 = a[1];
	s2 = a[2];
	s3 = a[3];
	s4 = a[4];
	s5 = a[5];
	s6 = a[6];
	s7 = a[7];
	s8 = a[8];
	s9 = a[9];

	m1 = mul32x32_64(r0, s1) + mul32x32_64(r1, s0);
	m3 = mul32x32_64(r0, s3) + mul32x32_64(r1, s2) + mul32x32_64(r2, s1) + mul32x32_64(r3, s0);
	m5 = mul32x32_64(r0, s5) + mul32x32_64(r1, s4) + mul32x32_64(r2, s3) + mul32x32_64(r3, s2) + mul32x32_64(r4, s1) + mul32x32_64(r5, s0);
	m7 = mul32x32_64(r0, s7) + mul32x32_64(r1, s6) + mul32x32_64(r2, s5) + mul32x32_64(r3, s4) + mul32x32_64(r4, s3) + mul32x32_64(r5, s2) + mul32x32_64(r6, s1) + mul32x32_64(r7, s0);
	m9 = mul32x32_64(r0, s9) + mul32x32_64(r1, s8) + mul32x32_64(r2, s7) + mul32x32_64(r3, s6) + mul32x32_64(r4, s5) + mul32x32_64(r5, s4) + mul32x32_64(r6, s3) + mul32x32_64(r7, s2) + mul32x32_64(r8, s1) + mul32x32_64(r9, s0);

	r1 *= 2;
	r3 *= 2;
	r5 *= 2;
	r7 *= 2;

	m0 = mul32x32_64(r0, s0);
	m2 = mul32x32_64(r0, s2) + mul32x32_64(r1, s1) + mul32x32_64(r2, s0);
	m4 = mul32x32_64(r0, s4) + mul32x32_64(r1, s3) + mul32x32_64(r2, s2) + mul32x32_64(r3, s1) + mul32x32_64(r4, s0);
	m6 = mul32x32_64(r0, s6) + mul32x32_64(r1, s5) + mul32x32_64(r2, s4) + mul32x32_64(r3, s3) + mul32x32_64(r4, s2) + mul32x32_64(r5, s1) + mul32x32_64(r6, s0);
	m8 = mul32x32_64(r0, s8) + mul32x32_64(r1, s7) + mul32x32_64(r2, s6) + mul32x32_64(r3, s5) + mul32x32_64(r4, s4) + mul32x32_64(r5, s3) + mul32x32_64(r6, s2) + mul32x32_64(r7, s1) + mul32x32_64(r8, s0);

	r1 *= 19;
	r2 *= 19;
	r3 = (r3 / 2) * 19;
	r4 *= 19;
	r5 = (r5 / 2) * 19;
	r6 *= 19;
	r7 = (r7 / 2) * 19;
	r8 *= 19;
	r9 *= 19;

	m1 += (mul32x32_64(r9, s2) + mul32x32_64(r8, s3) + mul32x32_64(r7, s4) + mul32x32_64(r6, s5) + mul32x32_64(r5, s6) + mul32x32_64(r4, s7) + mul32x32_64(r3, s8) + mul32x32_64(r2, s9));
	m3 += (mul32x32_64(r9, s4) + mul32x32_64(r8, s5) + mul32x32_64(r7, s6) + mul32x32_64(r6, s7) + mul32x32_64(r5, s8) + mul32x32_64(r4, s9));
	m5 += (mul32x32_64(r9, s6) + mul32x32_64(r8, s7) + mul32x32_64(r7, s8) + mul32x32_64(r6, s9));
	m7 += (mul32x32_64(r9, s8) + mul32x32_64(r8, s9));

	r3 *= 2;
	r5 *= 2;
	r7 *= 2;
	r9 *= 2;

	m0 += (mul32x32_64(r9, s1) + mul32x32_64(r8, s2) + mul32x32_64(r7, s3) + mul32x32_64(r6, s4) + mul32x32_64(r5, s5) + mul32x32_64(r4, s6) + mul32x32_64(r3, s7) + mul32x32_64(r2, s8) + mul32x32_64(r1, s9));
	m2 += (mul32x32_64(r9, s3) + mul32x32_64(r8, s4) + mul32x32_64(r7, s5) + mul32x32_64(r6, s6) + mul32x32_64(r5, s7) + mul32x32_64(r4, s8) + mul32x32_64(r3, s9));
	m4 += (mul32x32_64(r9, s5) + mul32x32_64(r8, s6) + mul32x32_64(r7, s7) + mul32x32_64(r6, s8) + mul32x32_64(r5, s9));
	m6 += (mul32x32_64(r9, s7) + mul32x32_64(r8, s8) + mul32x32_64(r7, s9));
	m8 += (mul32x32_64(r9, s9));

	                             r0 = (uint32_t)m0 & reduce_mask_26; c = (m0 >> 26);
	m1 += c;                     r1 = (uint32_t)m1 & reduce_mask_25; c = (m1 >> 25);
	m2 += c;                     r2 = (uint32_t)m2 & reduce_mask_26; c = (m2 >> 26);
	m3 += c;                     r3 = (uint32_t)m3 & reduce_mask_25; c = (m3 >> 25);
	m4 += c;                     r4 = (uint32_t)m4 & reduce_mask_26; c = (m4 >> 26);
	m5 += c;                     r5 = (uint32_t)m5 & reduce_mask_25; c = (m5 >> 25);
	m6 += c;                     r6 = (uint32_t)m6 & reduce_mask_26; c = (m6 >> 26);
	m7 += c;                     r7 = (uint32_t)m7 & reduce_mask_25; c = (m7 >> 25);
	m8 += c;                     r8 = (uint32_t)m8 & reduce_mask_26; c = (m8 >> 26);
	m9 += c;                     r9 = (uint32_t)m9 & reduce_mask_25; p = (uint32_t)(m9 >> 25);
	m0 = r0 + mul32x32_64(p,19); r0 = (uint32_t)m0 & reduce_mask_26; p = (uint32_t)(m0 >> 26);
	r1 += p;

	out[0] = r0;
	out[1] = r1;
	out[2] = r2;
	out[3] = r3;
	out[4] = r4;
	out[5] = r5;
	out[6] = r6;
	out[7] = r7;
	out[8] = r8;
	out[9] = r9;
};

/* out = in*in */
void curve25519_square(bignum25519 out, const bignum25519 in) {
	uint32_t r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
	uint32_t d6,d7,d8,d9;
	uint64_t m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,c;
	uint32_t p;

	r0 = in[0];
	r1 = in[1];
	r2 = in[2];
	r3 = in[3];
	r4 = in[4];
	r5 = in[5];
	r6 = in[6];
	r7 = in[7];
	r8 = in[8];
	r9 = in[9];

	m0 = mul32x32_64(r0, r0);
	r0 *= 2;
	m1 = mul32x32_64(r0, r1);
	m2 = mul32x32_64(r0, r2) + mul32x32_64(r1, r1 * 2);
	r1 *= 2;
	m3 = mul32x32_64(r0, r3) + mul32x32_64(r1, r2    );
	m4 = mul32x32_64(r0, r4) + mul32x32_64(r1, r3 * 2) + mul32x32_64(r2, r2);
	r2 *= 2;
	m5 = mul32x32_64(r0, r5) + mul32x32_64(r1, r4    ) + mul32x32_64(r2, r3);
	m6 = mul32x32_64(r0, r6) + mul32x32_64(r1, r5 * 2) + mul32x32_64(r2, r4) + mul32x32_64(r3, r3 * 2);
	r3 *= 2;
	m7 = mul32x32_64(r0, r7) + mul32x32_64(r1, r6    ) + mul32x32_64(r2, r5) + mul32x32_64(r3, r4    );
	m8 = mul32x32_64(r0, r8) + mul32x32_64(r1, r7 * 2) + mul32x32_64(r2, r6) + mul32x32_64(r3, r5 * 2) + mul32x32_64(r4, r4    );
	m9 = mul32x32_64(r0, r9) + mul32x32_64(r1, r8    ) + mul32x32_64(r2, r7) + mul32x32_64(r3, r6    ) + mul32x32_64(r4, r5 * 2);

	d6 = r6 * 19;
	d7 = r7 * 2 * 19;
	d8 = r8 * 19;
	d9 = r9 * 2 * 19;

	m0 += (mul32x32_64(d9, r1    ) + mul32x32_64(d8, r2    ) + mul32x32_64(d7, r3    ) + mul32x32_64(d6, r4 * 2) + mul32x32_64(r5, r5 * 2 * 19));
	m1 += (mul32x32_64(d9, r2 / 2) + mul32x32_64(d8, r3    ) + mul32x32_64(d7, r4    ) + mul32x32_64(d6, r5 * 2));
	m2 += (mul32x32_64(d9, r3    ) + mul32x32_64(d8, r4 * 2) + mul32x32_64(d7, r5 * 2) + mul32x32_64(d6, r6    ));
	m3 += (mul32x32_64(d9, r4    ) + mul32x32_64(d8, r5 * 2) + mul32x32_64(d7, r6    ));
	m4 += (mul32x32_64(d9, r5 * 2) + mul32x32_64(d8, r6 * 2) + mul32x32_64(d7, r7    ));
	m5 += (mul32x32_64(d9, r6    ) + mul32x32_64(d8, r7 * 2));
	m6 += (mul32x32_64(d9, r7 * 2) + mul32x32_64(d8, r8    ));
	m7 += (mul32x32_64(d9, r8    ));
	m8 += (mul32x32_64(d9, r9    ));

	                             r0 = (uint32_t)m0 & reduce_mask_26; c = (m0 >> 26);
	m1 += c;                     r1 = (uint32_t)m1 & reduce_mask_25; c = (m1 >> 25);
	m2 += c;                     r2 = (uint32_t)m2 & reduce_mask_26; c = (m2 >> 26);
	m3 += c;                     r3 = (uint32_t)m3 & reduce_mask_25; c = (m3 >> 25);
	m4 += c;                     r4 = (uint32_t)m4 & reduce_mask_26; c = (m4 >> 26);
	m5 += c;                     r5 = (uint32_t)m5 & reduce_mask_25; c = (m5 >> 25);
	m6 += c;                     r6 = (uint32_t)m6 & reduce_mask_26; c = (m6 >> 26);
	m7 += c;                     r7 = (uint32_t)m7 & reduce_mask_25; c = (m7 >> 25);
	m8 += c;                     r8 = (uint32_t)m8 & reduce_mask_26; c = (m8 >> 26);
	m9 += c;                     r9 = (uint32_t)m9 & reduce_mask_25; p = (uint32_t)(m9 >> 25);
	m0 = r0 + mul32x32_64(p,19); r0 = (uint32_t)m0 & reduce_mask_26; p = (uint32_t)(m0 >> 26);
	r1 += p;

	out[0] = r0;
	out[1] = r1;
	out[2] = r2;
	out[3] = r3;
	out[4] = r4;
	out[5] = r5;
	out[6] = r6;
	out[7] = r7;
	out[8] = r8;
	out[9] = r9;
};

/* out = in ^ (2 * count) */
void curve25519_square_times(bignum25519 out, const bignum25519 in, int count) {
	uint32_t r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
	uint32_t d6,d7,d8,d9;
	uint64_t m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,c;
	uint32_t p;

	r0 = in[0];
	r1 = in[1];
	r2 = in[2];
	r3 = in[3];
	r4 = in[4];
	r5 = in[5];
	r6 = in[6];
	r7 = in[7];
	r8 = in[8];
	r9 = in[9];

	do {
		m0 = mul32x32_64(r0, r0);
		r0 *= 2;
		m1 = mul32x32_64(r0, r1);
		m2 = mul32x32_64(r0, r2) + mul32x32_64(r1, r1 * 2);
		r1 *= 2;
		m3 = mul32x32_64(r0, r3) + mul32x32_64(r1, r2    );
		m4 = mul32x32_64(r0, r4) + mul32x32_64(r1, r3 * 2) + mul32x32_64(r2, r2);
		r2 *= 2;
		m5 = mul32x32_64(r0, r5) + mul32x32_64(r1, r4    ) + mul32x32_64(r2, r3);
		m6 = mul32x32_64(r0, r6) + mul32x32_64(r1, r5 * 2) + mul32x32_64(r2, r4) + mul32x32_64(r3, r3 * 2);
		r3 *= 2;
		m7 = mul32x32_64(r0, r7) + mul32x32_64(r1, r6    ) + mul32x32_64(r2, r5) + mul32x32_64(r3, r4    );
		m8 = mul32x32_64(r0, r8) + mul32x32_64(r1, r7 * 2) + mul32x32_64(r2, r6) + mul32x32_64(r3, r5 * 2) + mul32x32_64(r4, r4    );
		m9 = mul32x32_64(r0, r9) + mul32x32_64(r1, r8    ) + mul32x32_64(r2, r7) + mul32x32_64(r3, r6    ) + mul32x32_64(r4, r5 * 2);

		d6 = r6 * 19;
		d7 = r7 * 2 * 19;
		d8 = r8 * 19;
		d9 = r9 * 2 * 19;

		m0 += (mul32x32_64(d9, r1    ) + mul32x32_64(d8, r2    ) + mul32x32_64(d7, r3    ) + mul32x32_64(d6, r4 * 2) + mul32x32_64(r5, r5 * 2 * 19));
		m1 += (mul32x32_64(d9, r2 / 2) + mul32x32_64(d8, r3    ) + mul32x32_64(d7, r4    ) + mul32x32_64(d6, r5 * 2));
		m2 += (mul32x32_64(d9, r3    ) + mul32x32_64(d8, r4 * 2) + mul32x32_64(d7, r5 * 2) + mul32x32_64(d6, r6    ));
		m3 += (mul32x32_64(d9, r4    ) + mul32x32_64(d8, r5 * 2) + mul32x32_64(d7, r6    ));
		m4 += (mul32x32_64(d9, r5 * 2) + mul32x32_64(d8, r6 * 2) + mul32x32_64(d7, r7    ));
		m5 += (mul32x32_64(d9, r6    ) + mul32x32_64(d8, r7 * 2));
		m6 += (mul32x32_64(d9, r7 * 2) + mul32x32_64(d8, r8    ));
		m7 += (mul32x32_64(d9, r8    ));
		m8 += (mul32x32_64(d9, r9    ));

		                             r0 = (uint32_t)m0 & reduce_mask_26; c = (m0 >> 26);
		m1 += c;                     r1 = (uint32_t)m1 & reduce_mask_25; c = (m1 >> 25);
		m2 += c;                     r2 = (uint32_t)m2 & reduce_mask_26; c = (m2 >> 26);
		m3 += c;                     r3 = (uint32_t)m3 & reduce_mask_25; c = (m3 >> 25);
		m4 += c;                     r4 = (uint32_t)m4 & reduce_mask_26; c = (m4 >> 26);
		m5 += c;                     r5 = (uint32_t)m5 & reduce_mask_25; c = (m5 >> 25);
		m6 += c;                     r6 = (uint32_t)m6 & reduce_mask_26; c = (m6 >> 26);
		m7 += c;                     r7 = (uint32_t)m7 & reduce_mask_25; c = (m7 >> 25);
		m8 += c;                     r8 = (uint32_t)m8 & reduce_mask_26; c = (m8 >> 26);
		m9 += c;                     r9 = (uint32_t)m9 & reduce_mask_25; p = (uint32_t)(m9 >> 25);
		m0 = r0 + mul32x32_64(p,19); r0 = (uint32_t)m0 & reduce_mask_26; p = (uint32_t)(m0 >> 26);
		r1 += p;
	} while (--count);

	out[0] = r0;
	out[1] = r1;
	out[2] = r2;
	out[3] = r3;
	out[4] = r4;
	out[5] = r5;
	out[6] = r6;
	out[7] = r7;
	out[8] = r8;
	out[9] = r9;
};

// Take a little-endian, 32-byte number and expand it into polynomial form.
void curve25519_expand(bignum25519 out, const unsigned char in[32]) {
	static const union { uint8_t b[2]; uint16_t s; } endian_check = {{1,0}};
	uint32_t x0,x1,x2,x3,x4,x5,x6,x7;

	if (endian_check.s == 1) {
		x0 = *(uint32_t *)(in + 0);
		x1 = *(uint32_t *)(in + 4);
		x2 = *(uint32_t *)(in + 8);
		x3 = *(uint32_t *)(in + 12);
		x4 = *(uint32_t *)(in + 16);
		x5 = *(uint32_t *)(in + 20);
		x6 = *(uint32_t *)(in + 24);
		x7 = *(uint32_t *)(in + 28);
    } else {
		#define F(s)                         \
			((((uint32_t)in[s + 0])      ) | \
			 (((uint32_t)in[s + 1]) <<  8) | \
			 (((uint32_t)in[s + 2]) << 16) | \
			 (((uint32_t)in[s + 3]) << 24))
		x0 = F(0);
		x1 = F(4);
		x2 = F(8);
		x3 = F(12);
		x4 = F(16);
		x5 = F(20);
		x6 = F(24);
		x7 = F(28);
		#undef F
	}

	out[0] = (                        x0       ) & 0x3ffffff;
	out[1] = ((((uint64_t)x1 << 32) | x0) >> 26) & 0x1ffffff;
	out[2] = ((((uint64_t)x2 << 32) | x1) >> 19) & 0x3ffffff;
	out[3] = ((((uint64_t)x3 << 32) | x2) >> 13) & 0x1ffffff;
	out[4] = ((                       x3) >>  6) & 0x3ffffff;
	out[5] = (                        x4       ) & 0x1ffffff;
	out[6] = ((((uint64_t)x5 << 32) | x4) >> 25) & 0x3ffffff;
	out[7] = ((((uint64_t)x6 << 32) | x5) >> 19) & 0x1ffffff;
	out[8] = ((((uint64_t)x7 << 32) | x6) >> 12) & 0x3ffffff;
	out[9] = ((                       x7) >>  6) & 0x1ffffff;
};

// Take a fully reduced polynomial form number and contract it into a
// little-endian, 32-byte array
void curve25519_contract(unsigned char out[32], const bignum25519 in) {
	bignum25519 f;
	curve25519_copy(f, in);

	#define carry_pass() \
		f[1] += f[0] >> 26; f[0] &= reduce_mask_26; \
		f[2] += f[1] >> 25; f[1] &= reduce_mask_25; \
		f[3] += f[2] >> 26; f[2] &= reduce_mask_26; \
		f[4] += f[3] >> 25; f[3] &= reduce_mask_25; \
		f[5] += f[4] >> 26; f[4] &= reduce_mask_26; \
		f[6] += f[5] >> 25; f[5] &= reduce_mask_25; \
		f[7] += f[6] >> 26; f[6] &= reduce_mask_26; \
		f[8] += f[7] >> 25; f[7] &= reduce_mask_25; \
		f[9] += f[8] >> 26; f[8] &= reduce_mask_26;

	#define carry_pass_full() \
		carry_pass() \
		f[0] += 19 * (f[9] >> 25); f[9] &= reduce_mask_25;

	#define carry_pass_final() \
		carry_pass() \
		f[9] &= reduce_mask_25;

	carry_pass_full()
	carry_pass_full()

	/* now t is between 0 and 2^255-1, properly carried. */
	/* case 1: between 0 and 2^255-20. case 2: between 2^255-19 and 2^255-1. */
	f[0] += 19;
	carry_pass_full()

	/* now between 19 and 2^255-1 in both cases, and offset by 19. */
	f[0] += (reduce_mask_26 + 1) - 19;
	f[1] += (reduce_mask_25 + 1) - 1;
	f[2] += (reduce_mask_26 + 1) - 1;
	f[3] += (reduce_mask_25 + 1) - 1;
	f[4] += (reduce_mask_26 + 1) - 1;
	f[5] += (reduce_mask_25 + 1) - 1;
	f[6] += (reduce_mask_26 + 1) - 1;
	f[7] += (reduce_mask_25 + 1) - 1;
	f[8] += (reduce_mask_26 + 1) - 1;
	f[9] += (reduce_mask_25 + 1) - 1;

	/* now between 2^255 and 2^256-20, and offset by 2^255. */
	carry_pass_final()

	#undef carry_pass
	#undef carry_full
	#undef carry_final

	f[1] <<= 2;
	f[2] <<= 3;
	f[3] <<= 5;
	f[4] <<= 6;
	f[6] <<= 1;
	f[7] <<= 3;
	f[8] <<= 4;
	f[9] <<= 6;

	#define F(i, s) \
		out[s+0] |= (unsigned char )(f[i] & 0xff); \
		out[s+1] = (unsigned char )((f[i] >> 8) & 0xff); \
		out[s+2] = (unsigned char )((f[i] >> 16) & 0xff); \
		out[s+3] = (unsigned char )((f[i] >> 24) & 0xff);

	out[0] = 0;
	out[16] = 0;
	F(0,0);
	F(1,3);
	F(2,6);
	F(3,9);
	F(4,12);
	F(5,16);
	F(6,19);
	F(7,22);
	F(8,25);
	F(9,28);
	#undef F
};

// out = (flag) ? in : out
void curve25519_move_conditional_bytes(uint8_t out[96], const uint8_t in[96], uint32_t flag) {
	const uint32_t nb = flag - 1, b = ~nb;
	const uint32_t *inl = (const uint32_t *)in;
	uint32_t *outl = (uint32_t *)out;
	outl[0] = (outl[0] & nb) | (inl[0] & b);
	outl[1] = (outl[1] & nb) | (inl[1] & b);
	outl[2] = (outl[2] & nb) | (inl[2] & b);
	outl[3] = (outl[3] & nb) | (inl[3] & b);
	outl[4] = (outl[4] & nb) | (inl[4] & b);
	outl[5] = (outl[5] & nb) | (inl[5] & b);
	outl[6] = (outl[6] & nb) | (inl[6] & b);
	outl[7] = (outl[7] & nb) | (inl[7] & b);
	outl[8] = (outl[8] & nb) | (inl[8] & b);
	outl[9] = (outl[9] & nb) | (inl[9] & b);
	outl[10] = (outl[10] & nb) | (inl[10] & b);
	outl[11] = (outl[11] & nb) | (inl[11] & b);
	outl[12] = (outl[12] & nb) | (inl[12] & b);
	outl[13] = (outl[13] & nb) | (inl[13] & b);
	outl[14] = (outl[14] & nb) | (inl[14] & b);
	outl[15] = (outl[15] & nb) | (inl[15] & b);
	outl[16] = (outl[16] & nb) | (inl[16] & b);
	outl[17] = (outl[17] & nb) | (inl[17] & b);
	outl[18] = (outl[18] & nb) | (inl[18] & b);
	outl[19] = (outl[19] & nb) | (inl[19] & b);
	outl[20] = (outl[20] & nb) | (inl[20] & b);
	outl[21] = (outl[21] & nb) | (inl[21] & b);
	outl[22] = (outl[22] & nb) | (inl[22] & b);
	outl[23] = (outl[23] & nb) | (inl[23] & b);
};

// if (iswap) swap(a, b)
void curve25519_swap_conditional(bignum25519 a, bignum25519 b, uint32_t iswap) {
	const uint32_t swap = (uint32_t)(-(int32_t)iswap);
	uint32_t x0,x1,x2,x3,x4,x5,x6,x7,x8,x9;

	x0 = swap & (a[0] ^ b[0]); a[0] ^= x0; b[0] ^= x0;
	x1 = swap & (a[1] ^ b[1]); a[1] ^= x1; b[1] ^= x1;
	x2 = swap & (a[2] ^ b[2]); a[2] ^= x2; b[2] ^= x2;
	x3 = swap & (a[3] ^ b[3]); a[3] ^= x3; b[3] ^= x3;
	x4 = swap & (a[4] ^ b[4]); a[4] ^= x4; b[4] ^= x4;
	x5 = swap & (a[5] ^ b[5]); a[5] ^= x5; b[5] ^= x5;
	x6 = swap & (a[6] ^ b[6]); a[6] ^= x6; b[6] ^= x6;
	x7 = swap & (a[7] ^ b[7]); a[7] ^= x7; b[7] ^= x7;
	x8 = swap & (a[8] ^ b[8]); a[8] ^= x8; b[8] ^= x8;
	x9 = swap & (a[9] ^ b[9]); a[9] ^= x9; b[9] ^= x9;
};

bignum256modm_element_t lt_modm(bignum256modm_element_t a, bignum256modm_element_t b) {
	return (a - b) >> 31;
};

// see HAC, Alg. 14.42 Step 4
void reduce256_modm(bignum256modm r) {
	bignum256modm t;
	bignum256modm_element_t b = 0, pb, mask;

	/* t = r - m */
	pb = 0;
	pb += modm_m[0]; b = lt_modm(r[0], pb); t[0] = (r[0] - pb + (b << 30)); pb = b;
	pb += modm_m[1]; b = lt_modm(r[1], pb); t[1] = (r[1] - pb + (b << 30)); pb = b;
	pb += modm_m[2]; b = lt_modm(r[2], pb); t[2] = (r[2] - pb + (b << 30)); pb = b;
	pb += modm_m[3]; b = lt_modm(r[3], pb); t[3] = (r[3] - pb + (b << 30)); pb = b;
	pb += modm_m[4]; b = lt_modm(r[4], pb); t[4] = (r[4] - pb + (b << 30)); pb = b;
	pb += modm_m[5]; b = lt_modm(r[5], pb); t[5] = (r[5] - pb + (b << 30)); pb = b;
	pb += modm_m[6]; b = lt_modm(r[6], pb); t[6] = (r[6] - pb + (b << 30)); pb = b;
	pb += modm_m[7]; b = lt_modm(r[7], pb); t[7] = (r[7] - pb + (b << 30)); pb = b;
	pb += modm_m[8]; b = lt_modm(r[8], pb); t[8] = (r[8] - pb + (b << 16));

	/* keep r if r was smaller than m */
	mask = b - 1;
	r[0] ^= mask & (r[0] ^ t[0]);
	r[1] ^= mask & (r[1] ^ t[1]);
	r[2] ^= mask & (r[2] ^ t[2]);
	r[3] ^= mask & (r[3] ^ t[3]);
	r[4] ^= mask & (r[4] ^ t[4]);
	r[5] ^= mask & (r[5] ^ t[5]);
	r[6] ^= mask & (r[6] ^ t[6]);
	r[7] ^= mask & (r[7] ^ t[7]);
	r[8] ^= mask & (r[8] ^ t[8]);
};

// Barrett reduction,  see HAC, Alg. 14.42
// Instead of passing in x, pre-process in to q1 and r1 for efficiency
void barrett_reduce256_modm(bignum256modm r, const bignum256modm q1, const bignum256modm r1) {
	bignum256modm q3, r2;
	uint64_t c;
	bignum256modm_element_t f, b, pb;

	/* q1 = x >> 248 = 264 bits = 9 30 bit elements
	   q2 = mu * q1
	   q3 = (q2 / 256(32+1)) = q2 / (2^8)^(32+1) = q2 >> 264 */
	c  = mul32x32_64(modm_mu[0], q1[7]) + mul32x32_64(modm_mu[1], q1[6]) + mul32x32_64(modm_mu[2], q1[5]) + mul32x32_64(modm_mu[3], q1[4]) + mul32x32_64(modm_mu[4], q1[3]) + mul32x32_64(modm_mu[5], q1[2]) + mul32x32_64(modm_mu[6], q1[1]) + mul32x32_64(modm_mu[7], q1[0]);
	c >>= 30;
	c += mul32x32_64(modm_mu[0], q1[8]) + mul32x32_64(modm_mu[1], q1[7]) + mul32x32_64(modm_mu[2], q1[6]) + mul32x32_64(modm_mu[3], q1[5]) + mul32x32_64(modm_mu[4], q1[4]) + mul32x32_64(modm_mu[5], q1[3]) + mul32x32_64(modm_mu[6], q1[2]) + mul32x32_64(modm_mu[7], q1[1]) + mul32x32_64(modm_mu[8], q1[0]);
	f = (bignum256modm_element_t)c; q3[0] = (f >> 24) & 0x3f; c >>= 30;
	c += mul32x32_64(modm_mu[1], q1[8]) + mul32x32_64(modm_mu[2], q1[7]) + mul32x32_64(modm_mu[3], q1[6]) + mul32x32_64(modm_mu[4], q1[5]) + mul32x32_64(modm_mu[5], q1[4]) + mul32x32_64(modm_mu[6], q1[3]) + mul32x32_64(modm_mu[7], q1[2]) + mul32x32_64(modm_mu[8], q1[1]);
	f = (bignum256modm_element_t)c; q3[0] |= (f << 6) & 0x3fffffff; q3[1] = (f >> 24) & 0x3f; c >>= 30;
	c += mul32x32_64(modm_mu[2], q1[8]) + mul32x32_64(modm_mu[3], q1[7]) + mul32x32_64(modm_mu[4], q1[6]) + mul32x32_64(modm_mu[5], q1[5]) + mul32x32_64(modm_mu[6], q1[4]) + mul32x32_64(modm_mu[7], q1[3]) + mul32x32_64(modm_mu[8], q1[2]);
	f = (bignum256modm_element_t)c; q3[1] |= (f << 6) & 0x3fffffff; q3[2] = (f >> 24) & 0x3f; c >>= 30;
	c += mul32x32_64(modm_mu[3], q1[8]) + mul32x32_64(modm_mu[4], q1[7]) + mul32x32_64(modm_mu[5], q1[6]) + mul32x32_64(modm_mu[6], q1[5]) + mul32x32_64(modm_mu[7], q1[4]) + mul32x32_64(modm_mu[8], q1[3]);
	f = (bignum256modm_element_t)c; q3[2] |= (f << 6) & 0x3fffffff; q3[3] = (f >> 24) & 0x3f; c >>= 30;
	c += mul32x32_64(modm_mu[4], q1[8]) + mul32x32_64(modm_mu[5], q1[7]) + mul32x32_64(modm_mu[6], q1[6]) + mul32x32_64(modm_mu[7], q1[5]) + mul32x32_64(modm_mu[8], q1[4]);
	f = (bignum256modm_element_t)c; q3[3] |= (f << 6) & 0x3fffffff; q3[4] = (f >> 24) & 0x3f; c >>= 30;
	c += mul32x32_64(modm_mu[5], q1[8]) + mul32x32_64(modm_mu[6], q1[7]) + mul32x32_64(modm_mu[7], q1[6]) + mul32x32_64(modm_mu[8], q1[5]);
	f = (bignum256modm_element_t)c; q3[4] |= (f << 6) & 0x3fffffff; q3[5] = (f >> 24) & 0x3f; c >>= 30;
	c += mul32x32_64(modm_mu[6], q1[8]) + mul32x32_64(modm_mu[7], q1[7]) + mul32x32_64(modm_mu[8], q1[6]);
	f = (bignum256modm_element_t)c; q3[5] |= (f << 6) & 0x3fffffff; q3[6] = (f >> 24) & 0x3f; c >>= 30;
	c += mul32x32_64(modm_mu[7], q1[8]) + mul32x32_64(modm_mu[8], q1[7]);
	f = (bignum256modm_element_t)c; q3[6] |= (f << 6) & 0x3fffffff; q3[7] = (f >> 24) & 0x3f; c >>= 30;
	c += mul32x32_64(modm_mu[8], q1[8]);
	f = (bignum256modm_element_t)c; q3[7] |= (f << 6) & 0x3fffffff; q3[8] = (bignum256modm_element_t)(c >> 24);

	/* r1 = (x mod 256^(32+1)) = x mod (2^8)(31+1) = x & ((1 << 264) - 1)
	   r2 = (q3 * m) mod (256^(32+1)) = (q3 * m) & ((1 << 264) - 1) */
	c = mul32x32_64(modm_m[0], q3[0]);
	r2[0] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
	c += mul32x32_64(modm_m[0], q3[1]) + mul32x32_64(modm_m[1], q3[0]);
	r2[1] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
	c += mul32x32_64(modm_m[0], q3[2]) + mul32x32_64(modm_m[1], q3[1]) + mul32x32_64(modm_m[2], q3[0]);
	r2[2] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
	c += mul32x32_64(modm_m[0], q3[3]) + mul32x32_64(modm_m[1], q3[2]) + mul32x32_64(modm_m[2], q3[1]) + mul32x32_64(modm_m[3], q3[0]);
	r2[3] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
	c += mul32x32_64(modm_m[0], q3[4]) + mul32x32_64(modm_m[1], q3[3]) + mul32x32_64(modm_m[2], q3[2]) + mul32x32_64(modm_m[3], q3[1]) + mul32x32_64(modm_m[4], q3[0]);
	r2[4] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
	c += mul32x32_64(modm_m[0], q3[5]) + mul32x32_64(modm_m[1], q3[4]) + mul32x32_64(modm_m[2], q3[3]) + mul32x32_64(modm_m[3], q3[2]) + mul32x32_64(modm_m[4], q3[1]) + mul32x32_64(modm_m[5], q3[0]);
	r2[5] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
	c += mul32x32_64(modm_m[0], q3[6]) + mul32x32_64(modm_m[1], q3[5]) + mul32x32_64(modm_m[2], q3[4]) + mul32x32_64(modm_m[3], q3[3]) + mul32x32_64(modm_m[4], q3[2]) + mul32x32_64(modm_m[5], q3[1]) + mul32x32_64(modm_m[6], q3[0]);
	r2[6] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
	c += mul32x32_64(modm_m[0], q3[7]) + mul32x32_64(modm_m[1], q3[6]) + mul32x32_64(modm_m[2], q3[5]) + mul32x32_64(modm_m[3], q3[4]) + mul32x32_64(modm_m[4], q3[3]) + mul32x32_64(modm_m[5], q3[2]) + mul32x32_64(modm_m[6], q3[1]) + mul32x32_64(modm_m[7], q3[0]);
	r2[7] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
	c += mul32x32_64(modm_m[0], q3[8]) + mul32x32_64(modm_m[1], q3[7]) + mul32x32_64(modm_m[2], q3[6]) + mul32x32_64(modm_m[3], q3[5]) + mul32x32_64(modm_m[4], q3[4]) + mul32x32_64(modm_m[5], q3[3]) + mul32x32_64(modm_m[6], q3[2]) + mul32x32_64(modm_m[7], q3[1]) + mul32x32_64(modm_m[8], q3[0]);
	r2[8] = (bignum256modm_element_t)(c & 0xffffff);

	/* r = r1 - r2
	   if (r < 0) r += (1 << 264) */
	pb = 0;
	pb += r2[0]; b = lt_modm(r1[0], pb); r[0] = (r1[0] - pb + (b << 30)); pb = b;
	pb += r2[1]; b = lt_modm(r1[1], pb); r[1] = (r1[1] - pb + (b << 30)); pb = b;
	pb += r2[2]; b = lt_modm(r1[2], pb); r[2] = (r1[2] - pb + (b << 30)); pb = b;
	pb += r2[3]; b = lt_modm(r1[3], pb); r[3] = (r1[3] - pb + (b << 30)); pb = b;
	pb += r2[4]; b = lt_modm(r1[4], pb); r[4] = (r1[4] - pb + (b << 30)); pb = b;
	pb += r2[5]; b = lt_modm(r1[5], pb); r[5] = (r1[5] - pb + (b << 30)); pb = b;
	pb += r2[6]; b = lt_modm(r1[6], pb); r[6] = (r1[6] - pb + (b << 30)); pb = b;
	pb += r2[7]; b = lt_modm(r1[7], pb); r[7] = (r1[7] - pb + (b << 30)); pb = b;
	pb += r2[8]; b = lt_modm(r1[8], pb); r[8] = (r1[8] - pb + (b << 24));

	reduce256_modm(r);
	reduce256_modm(r);
};

// addition modulo m
void add256_modm(bignum256modm r, const bignum256modm x, const bignum256modm y) {
	bignum256modm_element_t c;

	c  = x[0] + y[0]; r[0] = c & 0x3fffffff; c >>= 30;
	c += x[1] + y[1]; r[1] = c & 0x3fffffff; c >>= 30;
	c += x[2] + y[2]; r[2] = c & 0x3fffffff; c >>= 30;
	c += x[3] + y[3]; r[3] = c & 0x3fffffff; c >>= 30;
	c += x[4] + y[4]; r[4] = c & 0x3fffffff; c >>= 30;
	c += x[5] + y[5]; r[5] = c & 0x3fffffff; c >>= 30;
	c += x[6] + y[6]; r[6] = c & 0x3fffffff; c >>= 30;
	c += x[7] + y[7]; r[7] = c & 0x3fffffff; c >>= 30;
	c += x[8] + y[8]; r[8] = c;

	reduce256_modm(r);
};

// multiplication modulo m
void mul256_modm(bignum256modm r, const bignum256modm x, const bignum256modm y) {
	bignum256modm r1, q1;
	uint64_t c;
	bignum256modm_element_t f;

	/* r1 = (x mod 256^(32+1)) = x mod (2^8)(31+1) = x & ((1 << 264) - 1)
	   q1 = x >> 248 = 264 bits = 9 30 bit elements */
	c = mul32x32_64(x[0], y[0]);
	f = (bignum256modm_element_t)c; r1[0] = (f & 0x3fffffff); c >>= 30;
	c += mul32x32_64(x[0], y[1]) + mul32x32_64(x[1], y[0]);
	f = (bignum256modm_element_t)c; r1[1] = (f & 0x3fffffff); c >>= 30;
	c += mul32x32_64(x[0], y[2]) + mul32x32_64(x[1], y[1]) + mul32x32_64(x[2], y[0]);
	f = (bignum256modm_element_t)c; r1[2] = (f & 0x3fffffff); c >>= 30;
	c += mul32x32_64(x[0], y[3]) + mul32x32_64(x[1], y[2]) + mul32x32_64(x[2], y[1]) + mul32x32_64(x[3], y[0]);
	f = (bignum256modm_element_t)c; r1[3] = (f & 0x3fffffff); c >>= 30;
	c += mul32x32_64(x[0], y[4]) + mul32x32_64(x[1], y[3]) + mul32x32_64(x[2], y[2]) + mul32x32_64(x[3], y[1]) + mul32x32_64(x[4], y[0]);
	f = (bignum256modm_element_t)c; r1[4] = (f & 0x3fffffff); c >>= 30;
	c += mul32x32_64(x[0], y[5]) + mul32x32_64(x[1], y[4]) + mul32x32_64(x[2], y[3]) + mul32x32_64(x[3], y[2]) + mul32x32_64(x[4], y[1]) + mul32x32_64(x[5], y[0]);
	f = (bignum256modm_element_t)c; r1[5] = (f & 0x3fffffff); c >>= 30;
	c += mul32x32_64(x[0], y[6]) + mul32x32_64(x[1], y[5]) + mul32x32_64(x[2], y[4]) + mul32x32_64(x[3], y[3]) + mul32x32_64(x[4], y[2]) + mul32x32_64(x[5], y[1]) + mul32x32_64(x[6], y[0]);
	f = (bignum256modm_element_t)c; r1[6] = (f & 0x3fffffff); c >>= 30;
	c += mul32x32_64(x[0], y[7]) + mul32x32_64(x[1], y[6]) + mul32x32_64(x[2], y[5]) + mul32x32_64(x[3], y[4]) + mul32x32_64(x[4], y[3]) + mul32x32_64(x[5], y[2]) + mul32x32_64(x[6], y[1]) + mul32x32_64(x[7], y[0]);
	f = (bignum256modm_element_t)c; r1[7] = (f & 0x3fffffff); c >>= 30;
	c += mul32x32_64(x[0], y[8]) + mul32x32_64(x[1], y[7]) + mul32x32_64(x[2], y[6]) + mul32x32_64(x[3], y[5]) + mul32x32_64(x[4], y[4]) + mul32x32_64(x[5], y[3]) + mul32x32_64(x[6], y[2]) + mul32x32_64(x[7], y[1]) + mul32x32_64(x[8], y[0]);
	f = (bignum256modm_element_t)c; r1[8] = (f & 0x00ffffff); q1[0] = (f >> 8) & 0x3fffff; c >>= 30;
	c += mul32x32_64(x[1], y[8]) + mul32x32_64(x[2], y[7]) + mul32x32_64(x[3], y[6]) + mul32x32_64(x[4], y[5]) + mul32x32_64(x[5], y[4]) + mul32x32_64(x[6], y[3]) + mul32x32_64(x[7], y[2]) + mul32x32_64(x[8], y[1]);
	f = (bignum256modm_element_t)c; q1[0] = (q1[0] | (f << 22)) & 0x3fffffff; q1[1] = (f >> 8) & 0x3fffff; c >>= 30;
	c += mul32x32_64(x[2], y[8]) + mul32x32_64(x[3], y[7]) + mul32x32_64(x[4], y[6]) + mul32x32_64(x[5], y[5]) + mul32x32_64(x[6], y[4]) + mul32x32_64(x[7], y[3]) + mul32x32_64(x[8], y[2]);
	f = (bignum256modm_element_t)c; q1[1] = (q1[1] | (f << 22)) & 0x3fffffff; q1[2] = (f >> 8) & 0x3fffff; c >>= 30;
	c += mul32x32_64(x[3], y[8]) + mul32x32_64(x[4], y[7]) + mul32x32_64(x[5], y[6]) + mul32x32_64(x[6], y[5]) + mul32x32_64(x[7], y[4]) + mul32x32_64(x[8], y[3]);
	f = (bignum256modm_element_t)c; q1[2] = (q1[2] | (f << 22)) & 0x3fffffff; q1[3] = (f >> 8) & 0x3fffff; c >>= 30;
	c += mul32x32_64(x[4], y[8]) + mul32x32_64(x[5], y[7]) + mul32x32_64(x[6], y[6]) + mul32x32_64(x[7], y[5]) + mul32x32_64(x[8], y[4]);
	f = (bignum256modm_element_t)c; q1[3] = (q1[3] | (f << 22)) & 0x3fffffff; q1[4] = (f >> 8) & 0x3fffff; c >>= 30;
	c += mul32x32_64(x[5], y[8]) + mul32x32_64(x[6], y[7]) + mul32x32_64(x[7], y[6]) + mul32x32_64(x[8], y[5]);
	f = (bignum256modm_element_t)c; q1[4] = (q1[4] | (f << 22)) & 0x3fffffff; q1[5] = (f >> 8) & 0x3fffff; c >>= 30;
	c += mul32x32_64(x[6], y[8]) + mul32x32_64(x[7], y[7]) + mul32x32_64(x[8], y[6]);
	f = (bignum256modm_element_t)c; q1[5] = (q1[5] | (f << 22)) & 0x3fffffff; q1[6] = (f >> 8) & 0x3fffff; c >>= 30;
	c += mul32x32_64(x[7], y[8]) + mul32x32_64(x[8], y[7]);
	f = (bignum256modm_element_t)c; q1[6] = (q1[6] | (f << 22)) & 0x3fffffff; q1[7] = (f >> 8) & 0x3fffff; c >>= 30;
	c += mul32x32_64(x[8], y[8]);
	f = (bignum256modm_element_t)c; q1[7] = (q1[7] | (f << 22)) & 0x3fffffff; q1[8] = (f >> 8) & 0x3fffff;

	barrett_reduce256_modm(r, q1, r1);
};

void expand256_modm(bignum256modm out, const unsigned char *in, size_t len) {
	unsigned char work[64] = {0};
	bignum256modm_element_t x[16];
	bignum256modm q1;

	memcpy(work, in, len);
	x[0] = U8TO32_LE(work +  0);
	x[1] = U8TO32_LE(work +  4);
	x[2] = U8TO32_LE(work +  8);
	x[3] = U8TO32_LE(work + 12);
	x[4] = U8TO32_LE(work + 16);
	x[5] = U8TO32_LE(work + 20);
	x[6] = U8TO32_LE(work + 24);
	x[7] = U8TO32_LE(work + 28);
	x[8] = U8TO32_LE(work + 32);
	x[9] = U8TO32_LE(work + 36);
	x[10] = U8TO32_LE(work + 40);
	x[11] = U8TO32_LE(work + 44);
	x[12] = U8TO32_LE(work + 48);
	x[13] = U8TO32_LE(work + 52);
	x[14] = U8TO32_LE(work + 56);
	x[15] = U8TO32_LE(work + 60);

	/* r1 = (x mod 256^(32+1)) = x mod (2^8)(31+1) = x & ((1 << 264) - 1) */
	out[0] = (                         x[0]) & 0x3fffffff;
	out[1] = ((x[ 0] >> 30) | (x[ 1] <<  2)) & 0x3fffffff;
	out[2] = ((x[ 1] >> 28) | (x[ 2] <<  4)) & 0x3fffffff;
	out[3] = ((x[ 2] >> 26) | (x[ 3] <<  6)) & 0x3fffffff;
	out[4] = ((x[ 3] >> 24) | (x[ 4] <<  8)) & 0x3fffffff;
	out[5] = ((x[ 4] >> 22) | (x[ 5] << 10)) & 0x3fffffff;
	out[6] = ((x[ 5] >> 20) | (x[ 6] << 12)) & 0x3fffffff;
	out[7] = ((x[ 6] >> 18) | (x[ 7] << 14)) & 0x3fffffff;
	out[8] = ((x[ 7] >> 16) | (x[ 8] << 16)) & 0x00ffffff;

	/* 8*31 = 248 bits, no need to reduce */
	if (len < 32)
		return;

	/* q1 = x >> 248 = 264 bits = 9 30 bit elements */
	q1[0] = ((x[ 7] >> 24) | (x[ 8] <<  8)) & 0x3fffffff;
	q1[1] = ((x[ 8] >> 22) | (x[ 9] << 10)) & 0x3fffffff;
	q1[2] = ((x[ 9] >> 20) | (x[10] << 12)) & 0x3fffffff;
	q1[3] = ((x[10] >> 18) | (x[11] << 14)) & 0x3fffffff;
	q1[4] = ((x[11] >> 16) | (x[12] << 16)) & 0x3fffffff;
	q1[5] = ((x[12] >> 14) | (x[13] << 18)) & 0x3fffffff;
	q1[6] = ((x[13] >> 12) | (x[14] << 20)) & 0x3fffffff;
	q1[7] = ((x[14] >> 10) | (x[15] << 22)) & 0x3fffffff;
	q1[8] = ((x[15] >>  8)                );

	barrett_reduce256_modm(out, q1, out);
};

void expand_raw256_modm(bignum256modm out, const unsigned char in[32]) {
	bignum256modm_element_t x[8];

	x[0] = U8TO32_LE(in +  0);
	x[1] = U8TO32_LE(in +  4);
	x[2] = U8TO32_LE(in +  8);
	x[3] = U8TO32_LE(in + 12);
	x[4] = U8TO32_LE(in + 16);
	x[5] = U8TO32_LE(in + 20);
	x[6] = U8TO32_LE(in + 24);
	x[7] = U8TO32_LE(in + 28);

	out[0] = (                         x[0]) & 0x3fffffff;
	out[1] = ((x[ 0] >> 30) | (x[ 1] <<  2)) & 0x3fffffff;
	out[2] = ((x[ 1] >> 28) | (x[ 2] <<  4)) & 0x3fffffff;
	out[3] = ((x[ 2] >> 26) | (x[ 3] <<  6)) & 0x3fffffff;
	out[4] = ((x[ 3] >> 24) | (x[ 4] <<  8)) & 0x3fffffff;
	out[5] = ((x[ 4] >> 22) | (x[ 5] << 10)) & 0x3fffffff;
	out[6] = ((x[ 5] >> 20) | (x[ 6] << 12)) & 0x3fffffff;
	out[7] = ((x[ 6] >> 18) | (x[ 7] << 14)) & 0x3fffffff;
	out[8] = ((x[ 7] >> 16)                ) & 0x0000ffff;
};

void contract256_modm(unsigned char out[32], const bignum256modm in) {
	U32TO8_LE(out +  0, (in[0]      ) | (in[1] << 30));
	U32TO8_LE(out +  4, (in[1] >>  2) | (in[2] << 28));
	U32TO8_LE(out +  8, (in[2] >>  4) | (in[3] << 26));
	U32TO8_LE(out + 12, (in[3] >>  6) | (in[4] << 24));
	U32TO8_LE(out + 16, (in[4] >>  8) | (in[5] << 22));
	U32TO8_LE(out + 20, (in[5] >> 10) | (in[6] << 20));
	U32TO8_LE(out + 24, (in[6] >> 12) | (in[7] << 18));
	U32TO8_LE(out + 28, (in[7] >> 14) | (in[8] << 16));
};

void contract256_window4_modm(signed char r[64], const bignum256modm in) {
	char carry;
	signed char *quads = r;
	bignum256modm_element_t i, j, v;

	for (i = 0; i < 8; i += 2) {
		v = in[i];
		for (j = 0; j < 7; j++) {
			*quads++ = (v & 15);
			v >>= 4;
		}
		v |= (in[i+1] << 2);
		for (j = 0; j < 8; j++) {
			*quads++ = (v & 15);
			v >>= 4;
		}
	}
	v = in[8];
	*quads++ = (v & 15); v >>= 4;
	*quads++ = (v & 15); v >>= 4;
	*quads++ = (v & 15); v >>= 4;
	*quads++ = (v & 15); v >>= 4;

	/* making it signed */
	carry = 0;
	for(i = 0; i < 63; i++) {
		r[i] += carry;
		r[i+1] += (r[i] >> 4);
		r[i] &= 15;
		carry = (r[i] >> 3);
		r[i] -= (carry << 4);
	}
	r[63] += carry;
};

void contract256_slidingwindow_modm(signed char r[256], const bignum256modm s, int windowsize) {
	int i,j,k,b;
	int m = (1 << (windowsize - 1)) - 1, soplen = 256;
	signed char *bits = r;
	bignum256modm_element_t v;

	/* first put the binary expansion into r  */
	for (i = 0; i < 8; i++) {
		v = s[i];
		for (j = 0; j < 30; j++, v >>= 1)
			*bits++ = (v & 1);
	}
	v = s[8];
	for (j = 0; j < 16; j++, v >>= 1)
		*bits++ = (v & 1);

	/* Making it sliding window */
	for (j = 0; j < soplen; j++) {
		if (!r[j])
			continue;

		for (b = 1; (b < (soplen - j)) && (b <= 6); b++) {
			if ((r[j] + (r[j + b] << b)) <= m) {
				r[j] += r[j + b] << b;
				r[j + b] = 0;
			} else if ((r[j] - (r[j + b] << b)) >= -m) {
				r[j] -= r[j + b] << b;
				for (k = j + b; k < soplen; k++) {
					if (!r[k]) {
						r[k] = 1;
						break;
					}
					r[k] = 0;
				}
			} else if (r[j + b]) {
				break;
			}
		}
	}
};

/**
 * conversions
 */

void ge25519_p1p1_to_partial(ge25519 *r, const ge25519_p1p1 *p) {
	curve25519_mul(r->x, p->x, p->t);
	curve25519_mul(r->y, p->y, p->z);
	curve25519_mul(r->z, p->z, p->t);
};

void ge25519_p1p1_to_full(ge25519 *r, const ge25519_p1p1 *p) {
	curve25519_mul(r->x, p->x, p->t);
	curve25519_mul(r->y, p->y, p->z);
	curve25519_mul(r->z, p->z, p->t);
	curve25519_mul(r->t, p->x, p->y);
};

void ge25519_full_to_pniels(ge25519_pniels *p, const ge25519 *r) {
	curve25519_sub(p->ysubx, r->y, r->x);
	curve25519_add(p->xaddy, r->y, r->x);
	curve25519_copy(p->z, r->z);
	curve25519_mul(p->t2d, r->t, ge25519_ec2d);
};

/*
 * adding & doubling
 */

void ge25519_add_p1p1(ge25519_p1p1 *r, const ge25519 *p, const ge25519 *q) {
	bignum25519 a,b,c,d,t,u;

	curve25519_sub(a, p->y, p->x);
	curve25519_add(b, p->y, p->x);
	curve25519_sub(t, q->y, q->x);
	curve25519_add(u, q->y, q->x);
	curve25519_mul(a, a, t);
	curve25519_mul(b, b, u);
	curve25519_mul(c, p->t, q->t);
	curve25519_mul(c, c, ge25519_ec2d);
	curve25519_mul(d, p->z, q->z);
	curve25519_add(d, d, d);
	curve25519_sub(r->x, b, a);
	curve25519_add(r->y, b, a);
	curve25519_add_after_basic(r->z, d, c);
	curve25519_sub_after_basic(r->t, d, c);
};

void ge25519_double_p1p1(ge25519_p1p1 *r, const ge25519 *p) {
	bignum25519 a,b,c;

	curve25519_square(a, p->x);
	curve25519_square(b, p->y);
	curve25519_square(c, p->z);
	curve25519_add_reduce(c, c, c);
	curve25519_add(r->x, p->x, p->y);
	curve25519_square(r->x, r->x);
	curve25519_add(r->y, b, a);
	curve25519_sub(r->z, b, a);
	curve25519_sub_after_basic(r->x, r->x, r->y);
	curve25519_sub_after_basic(r->t, c, r->z);
};

void ge25519_nielsadd2_p1p1(ge25519_p1p1 *r, const ge25519 *p, const ge25519_niels *q, unsigned char signbit) {
	const bignum25519 *qb = (const bignum25519 *)q;
	bignum25519 *rb = (bignum25519 *)r;
	bignum25519 a,b,c;

	curve25519_sub(a, p->y, p->x);
	curve25519_add(b, p->y, p->x);
	curve25519_mul(a, a, qb[signbit]); /* x for +, y for - */
	curve25519_mul(r->x, b, qb[signbit^1]); /* y for +, x for - */
	curve25519_add(r->y, r->x, a);
	curve25519_sub(r->x, r->x, a);
	curve25519_mul(c, p->t, q->t2d);
	curve25519_add_reduce(r->t, p->z, p->z);
	curve25519_copy(r->z, r->t);
	curve25519_add(rb[2+signbit], rb[2+signbit], c); /* z for +, t for - */
	curve25519_sub(rb[2+(signbit^1)], rb[2+(signbit^1)], c); /* t for +, z for - */
};

void ge25519_pnielsadd_p1p1(ge25519_p1p1 *r, const ge25519 *p, const ge25519_pniels *q, unsigned char signbit) {
	const bignum25519 *qb = (const bignum25519 *)q;
	bignum25519 *rb = (bignum25519 *)r;
	bignum25519 a,b,c;

	curve25519_sub(a, p->y, p->x);
	curve25519_add(b, p->y, p->x);
	curve25519_mul(a, a, qb[signbit]); /* ysubx for +, xaddy for - */
	curve25519_mul(r->x, b, qb[signbit^1]); /* xaddy for +, ysubx for - */
	curve25519_add(r->y, r->x, a);
	curve25519_sub(r->x, r->x, a);
	curve25519_mul(c, p->t, q->t2d);
	curve25519_mul(r->t, p->z, q->z);
	curve25519_add_reduce(r->t, r->t, r->t);
	curve25519_copy(r->z, r->t);
	curve25519_add(rb[2+signbit], rb[2+signbit], c); /* z for +, t for - */
	curve25519_sub(rb[2+(signbit^1)], rb[2+(signbit^1)], c); /* t for +, z for - */
};

void ge25519_double_partial(ge25519 *r, const ge25519 *p) {
	ge25519_p1p1 t;
	ge25519_double_p1p1(&t, p);
	ge25519_p1p1_to_partial(r, &t);
};

void ge25519_double(ge25519 *r, const ge25519 *p) {
	ge25519_p1p1 t;
	ge25519_double_p1p1(&t, p);
	ge25519_p1p1_to_full(r, &t);
};

void ge25519_add(ge25519 *r, const ge25519 *p,  const ge25519 *q) {
	ge25519_p1p1 t;
	ge25519_add_p1p1(&t, p, q);
	ge25519_p1p1_to_full(r, &t);
};

void ge25519_nielsadd2(ge25519 *r, const ge25519_niels *q) {
	bignum25519 a,b,c,e,f,g,h;

	curve25519_sub(a, r->y, r->x);
	curve25519_add(b, r->y, r->x);
	curve25519_mul(a, a, q->ysubx);
	curve25519_mul(e, b, q->xaddy);
	curve25519_add(h, e, a);
	curve25519_sub(e, e, a);
	curve25519_mul(c, r->t, q->t2d);
	curve25519_add(f, r->z, r->z);
	curve25519_add_after_basic(g, f, c);
	curve25519_sub_after_basic(f, f, c);
	curve25519_mul(r->x, e, f);
	curve25519_mul(r->y, h, g);
	curve25519_mul(r->z, g, f);
	curve25519_mul(r->t, e, h);
};

void ge25519_pnielsadd(ge25519_pniels *r, const ge25519 *p, const ge25519_pniels *q) {
	bignum25519 a,b,c,x,y,z,t;

	curve25519_sub(a, p->y, p->x);
	curve25519_add(b, p->y, p->x);
	curve25519_mul(a, a, q->ysubx);
	curve25519_mul(x, b, q->xaddy);
	curve25519_add(y, x, a);
	curve25519_sub(x, x, a);
	curve25519_mul(c, p->t, q->t2d);
	curve25519_mul(t, p->z, q->z);
	curve25519_add(t, t, t);
	curve25519_add_after_basic(z, t, c);
	curve25519_sub_after_basic(t, t, c);
	curve25519_mul(r->xaddy, x, t);
	curve25519_mul(r->ysubx, y, z);
	curve25519_mul(r->z, z, t);
	curve25519_mul(r->t2d, x, y);
	curve25519_copy(y, r->ysubx);
	curve25519_sub(r->ysubx, r->ysubx, r->xaddy);
	curve25519_add(r->xaddy, r->xaddy, y);
	curve25519_mul(r->t2d, r->t2d, ge25519_ec2d);
};

/**
 * pack & unpack
 */

void ge25519_pack(unsigned char r[32], const ge25519 *p) {
	bignum25519 tx, ty, zi;
	unsigned char parity[32];
	curve25519_recip(zi, p->z);
	curve25519_mul(tx, p->x, zi);
	curve25519_mul(ty, p->y, zi);
	curve25519_contract(r, ty);
	curve25519_contract(parity, tx);
	r[31] ^= ((parity[0] & 1) << 7);
};

int ge25519_unpack_negative_vartime(ge25519 *r, const unsigned char p[32]) {
	static const unsigned char zero[32] = {0};
	static const bignum25519 one = {1};
	unsigned char parity = p[31] >> 7;
	unsigned char check[32];
	bignum25519 t, root, num, den, d3;

	curve25519_expand(r->y, p);
	curve25519_copy(r->z, one);
	curve25519_square(num, r->y); /* x = y^2 */
	curve25519_mul(den, num, ge25519_ecd); /* den = dy^2 */
	curve25519_sub_reduce(num, num, r->z); /* x = y^1 - 1 */
	curve25519_add(den, den, r->z); /* den = dy^2 + 1 */

	/* Computation of sqrt(num/den) */
	/* 1.: computation of num^((p-5)/8)*den^((7p-35)/8) = (num*den^7)^((p-5)/8) */
	curve25519_square(t, den);
	curve25519_mul(d3, t, den);
	curve25519_square(r->x, d3);
	curve25519_mul(r->x, r->x, den);
	curve25519_mul(r->x, r->x, num);
	curve25519_pow_two252m3(r->x, r->x);

	/* 2. computation of r->x = num * den^3 * (num*den^7)^((p-5)/8) */
	curve25519_mul(r->x, r->x, d3);
	curve25519_mul(r->x, r->x, num);

	/* 3. Check if either of the roots works: */
	curve25519_square(t, r->x);
	curve25519_mul(t, t, den);
	curve25519_sub_reduce(root, t, num);
	curve25519_contract(check, root);
	if (!ed25519_verify(check, zero, 32)) {
		curve25519_add_reduce(t, t, num);
		curve25519_contract(check, t);
		if (!ed25519_verify(check, zero, 32))
			return 0;
		curve25519_mul(r->x, r->x, ge25519_sqrtneg1);
	}

	curve25519_contract(check, r->x);
	if ((check[0] & 1) == parity) {
		curve25519_copy(t, r->x);
		curve25519_neg(r->x, t);
	}
	curve25519_mul(r->t, r->x, r->y);
	return 1;
};

// computes [s1]p1 + [s2]basepoint
void ge25519_double_scalarmult_vartime(ge25519 *r, const ge25519 *p1, const bignum256modm s1, const bignum256modm s2) {
	signed char slide1[256], slide2[256];
	ge25519_pniels pre1[S1_TABLE_SIZE];
	ge25519 d1;
	ge25519_p1p1 t;
	int32_t i;

	contract256_slidingwindow_modm(slide1, s1, S1_SWINDOWSIZE);
	contract256_slidingwindow_modm(slide2, s2, S2_SWINDOWSIZE);

	ge25519_double(&d1, p1);
	ge25519_full_to_pniels(pre1, p1);
	for (i = 0; i < S1_TABLE_SIZE - 1; i++)
		ge25519_pnielsadd(&pre1[i+1], &d1, &pre1[i]);

	/* set neutral */
	memset(r, 0, sizeof(ge25519));
	r->y[0] = 1;
	r->z[0] = 1;

	i = 255;
	while ((i >= 0) && !(slide1[i] | slide2[i]))
		i--;

	for (; i >= 0; i--) {
		ge25519_double_p1p1(&t, r);

		if (slide1[i]) {
			ge25519_p1p1_to_full(r, &t);
			ge25519_pnielsadd_p1p1(&t, r, &pre1[abs(slide1[i]) / 2], (unsigned char)slide1[i] >> 7);
		}

		if (slide2[i]) {
			ge25519_p1p1_to_full(r, &t);
			ge25519_nielsadd2_p1p1(&t, r, &ge25519_niels_sliding_multiples[abs(slide2[i]) / 2], (unsigned char)slide2[i] >> 7);
		}

		ge25519_p1p1_to_partial(r, &t);
	}
};

uint32_t ge25519_windowb_equal(uint32_t b, uint32_t c) {
	return ((b ^ c) - 1) >> 31;
};

void ge25519_scalarmult_base_choose_niels(ge25519_niels *t, const uint8_t table[256][96], uint32_t pos, signed char b) {
	bignum25519 neg;
	uint32_t sign = (uint32_t)((unsigned char)b >> 7);
	uint32_t mask = ~(sign - 1);
	uint32_t u = (b + mask) ^ mask;
	uint32_t i;

	/* ysubx, xaddy, t2d in packed form. initialize to ysubx = 1, xaddy = 1, t2d = 0 */
	uint8_t packed[96] = {0};
	packed[0] = 1;
	packed[32] = 1;

	for (i = 0; i < 8; i++)
		curve25519_move_conditional_bytes(packed, table[(pos * 8) + i], ge25519_windowb_equal(u, i + 1));

	/* expand in to t */
	curve25519_expand(t->ysubx, packed +  0);
	curve25519_expand(t->xaddy, packed + 32);
	curve25519_expand(t->t2d  , packed + 64);

	/* adjust for sign */
	curve25519_swap_conditional(t->ysubx, t->xaddy, sign);
	curve25519_neg(neg, t->t2d);
	curve25519_swap_conditional(t->t2d, neg, sign);
};

// computes [s]basepoint
void ge25519_scalarmult_base_niels(ge25519 *r, const uint8_t basepoint_table[256][96], const bignum256modm s) {
	signed char b[64];
	uint32_t i;
	ge25519_niels t;

	contract256_window4_modm(b, s);

	ge25519_scalarmult_base_choose_niels(&t, basepoint_table, 0, b[1]);
	curve25519_sub_reduce(r->x, t.xaddy, t.ysubx);
	curve25519_add_reduce(r->y, t.xaddy, t.ysubx);
	memset(r->z, 0, sizeof(bignum25519));
	curve25519_copy(r->t, t.t2d);
	r->z[0] = 2;
	for (i = 3; i < 64; i += 2) {
		ge25519_scalarmult_base_choose_niels(&t, basepoint_table, i / 2, b[i]);
		ge25519_nielsadd2(r, &t);
	}
	ge25519_double_partial(r, r);
	ge25519_double_partial(r, r);
	ge25519_double_partial(r, r);
	ge25519_double(r, r);
	ge25519_scalarmult_base_choose_niels(&t, basepoint_table, 0, b[0]);
	curve25519_mul(t.t2d, t.t2d, ge25519_ecd);
	ge25519_nielsadd2(r, &t);
	for(i = 2; i < 64; i += 2) {
		ge25519_scalarmult_base_choose_niels(&t, basepoint_table, i / 2, b[i]);
		ge25519_nielsadd2(r, &t);
	}
};

/*
 * endianness
 */

void U32TO8_LE(unsigned char *p, const uint32_t v) {
	p[0] = (unsigned char)(v      );
	p[1] = (unsigned char)(v >>  8);
	p[2] = (unsigned char)(v >> 16);
	p[3] = (unsigned char)(v >> 24);
};

uint32_t U8TO32_LE(const unsigned char *p) {
	return
	(((uint32_t)(p[0])      ) |
	 ((uint32_t)(p[1]) <<  8) |
	 ((uint32_t)(p[2]) << 16) |
	 ((uint32_t)(p[3]) << 24));
};
