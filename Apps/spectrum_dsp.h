#ifndef NOVA_SPECTRUM_DSP_H
#define NOVA_SPECTRUM_DSP_H
/* Bounded, integer-only NOVA analyzer. 16/48 kHz PCM, up to 8192 samples.
 * Q8 PCM FFT butterflies scale by 1/2 at every stage. Q30 twiddles and
 * Q20 symmetric windows retain quiet signals without a floating-point ABI.
 * Output is single-sided peak amplitude / 32768 in Q24, corrected by the
 * actual quantized window's coherent gain. DC and Nyquist are NOT doubled.
 * No DC removal, noise suppression, SPL claim, allocation, or libm dependency.
 * Window leakage/scalloping still applies, especially near DC and Nyquist.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SPECTRUM_DSP_RATE 16000u
#define SPECTRUM_DSP_NYQUIST (SPECTRUM_DSP_RATE / 2u)
#define SPECTRUM_DSP_MAX_FFT 8192u
#define SPECTRUM_DSP_CHUNK 256u
#define SPECTRUM_DSP_FLOOR_DB (-12000)
#define SPECTRUM_DSP_DISPLAY_FLOOR_DB (-9000)
#define SPECTRUM_DSP_FULL_SCALE 16777216u

typedef enum {
    SPECTRUM_DSP_RECT, SPECTRUM_DSP_HANN, SPECTRUM_DSP_HAMMING,
    SPECTRUM_DSP_BLACKMAN, SPECTRUM_DSP_FLAT_TOP,
    SPECTRUM_DSP_WINDOW_COUNT
} spectrum_dsp_window;

typedef struct {
    uint32_t sample_rate;
    uint16_t fft_size;
    spectrum_dsp_window window;
    uint16_t low_hz, high_hz;
    bool log_frequency, log_amplitude;
    int8_t gain_db, threshold_db;
} spectrum_dsp_config;

typedef struct {
    int32_t real[SPECTRUM_DSP_MAX_FFT], imag[SPECTRUM_DSP_MAX_FFT];
    uint32_t amplitude_q24[SPECTRUM_DSP_MAX_FFT / 2u + 1u];
    spectrum_dsp_config config;
    uint32_t transforms, gain_q16;
    uint32_t window_phase, window_error, phase_step, phase_remainder;
    int32_t coherent_gain_q20;
    uint16_t used, peak_bin;
    bool initialized, has_transform;
} spectrum_dsp_state;
_Static_assert(sizeof(spectrum_dsp_state) <= 82200u, "NOVA DSP RAM is bounded to 81 KiB");

/* Linking the pinned Xtensa libgcc 64-bit division objects leaves unsupported
 * zero dynamic relocations. This source-level quotient uses only 32-bit division
 * and at most 32 bit steps, so native ELF loading needs no special cases.
 * A zero denominator returns UINT64_MAX; production callers validate first. */
static inline uint64_t spectrum_dsp_div_u64_u32(uint64_t value, uint32_t denominator) {
    if (!denominator) return UINT64_MAX;
    uint32_t high = (uint32_t)(value >> 32), low = (uint32_t)value;
    if (!high) return low / denominator;
    if (!(denominator & (denominator - 1u))) {
        unsigned shift = 0;
        while ((denominator >>= 1) != 0u) ++shift;
        return value >> shift;
    }
    uint32_t quotient_high = high / denominator, remainder = high % denominator, quotient_low = 0;
    for (unsigned bit = 32; bit-- > 0;) {
        uint32_t carry = remainder >> 31;
        remainder = (remainder << 1) | ((low >> bit) & 1u);
        if (carry || remainder >= denominator) {
            remainder -= denominator;
            quotient_low |= 1u << bit;
        }
    }
    return ((uint64_t)quotient_high << 32) | quotient_low;
}

/* round(2^30 sin(pi*i/2048)), quarter wave. Linear interpolation gives
 * at most 0.0000003 absolute twiddle error, including 8192-point FFTs. */
static const int32_t spectrum_dsp_quarter[1025] = {
    0,1647099,3294193,4941281,6588356,8235416,9882456,11529474,
    13176464,14823423,16470347,18117233,19764076,21410872,23057618,24704310,
    26350943,27997515,29644021,31290457,32936819,34583104,36229307,37875426,
    39521455,41167391,42813230,44458968,46104602,47750128,49395541,51040837,
    52686014,54331067,55975992,57620785,59265442,60909960,62554335,64198563,
    65842639,67486561,69130324,70773924,72417357,74060620,75703709,77346620,
    78989349,80631892,82274245,83916404,85558366,87200127,88841683,90483029,
    92124163,93765079,95405776,97046247,98686491,100326502,101966277,103605812,
    105245103,106884147,108522939,110161476,111799753,113437768,115075515,116712992,
    118350194,119987118,121623759,123260114,124896179,126531950,128167423,129802595,
    131437462,133072019,134706263,136340190,137973796,139607077,141240030,142872651,
    144504935,146136880,147768480,149399733,151030634,152661180,154291367,155921191,
    157550647,159179733,160808445,162436778,164064728,165692293,167319468,168946249,
    170572633,172198615,173824192,175449360,177074115,178698453,180322371,181945865,
    183568930,185191564,186813762,188435520,190056834,191677702,193298119,194918080,
    196537583,198156624,199775198,201393302,203010932,204628085,206244756,207860942,
    209476638,211091842,212706549,214320755,215934457,217547651,219160334,220772500,
    222384147,223995270,225605867,227215933,228825464,230434456,232042906,233650811,
    235258165,236864966,238471210,240076892,241682010,243286558,244890535,246493935,
    248096755,249698991,251300640,252901697,254502159,256102022,257701283,259299937,
    260897982,262495412,264092224,265688415,267283981,268878918,270473223,272066891,
    273659918,275252302,276844038,278435122,280025552,281615322,283204430,284792871,
    286380643,287967740,289554160,291139898,292724951,294309316,295892988,297475964,
    299058239,300639811,302220676,303800829,305380268,306958988,308536985,310114257,
    311690799,313266607,314841679,316416009,317989595,319562433,321134518,322705848,
    324276419,325846226,327415267,328983538,330551034,332117752,333683689,335248841,
    336813204,338376774,339939549,341501523,343062693,344623057,346182609,347741347,
    349299266,350856364,352412636,353968079,355522689,357076462,358629395,360181484,
    361732726,363283116,364832652,366381329,367929144,369476093,371022173,372567379,
    374111709,375655159,377197725,378739403,380280190,381820082,383359076,384897167,
    386434353,387970630,389505993,391040440,392573967,394106570,395638246,397168991,
    398698801,400227673,401755603,403282588,404808624,406333708,407857835,409381002,
    410903207,412424444,413944711,415464004,416982319,418499653,420016002,421531363,
    423045732,424559105,426071480,427582852,429093217,430602573,432110916,433618242,
    435124548,436629829,438134084,439637307,441139496,442640647,444140756,445639820,
    447137835,448634799,450130706,451625555,453119340,454612060,456103710,457594286,
    459083786,460572205,462059541,463545789,465030947,466515010,467997976,469479840,
    470960600,472440251,473918791,475396216,476872522,478347705,479821764,481294693,
    482766489,484237150,485706671,487175049,488642281,490108363,491573292,493037064,
    494499676,495961124,497421405,498880516,500338453,501795212,503250791,504705185,
    506158392,507610408,509061229,510510853,511959275,513406493,514852502,516297300,
    517740883,519183248,520624391,522064309,523502998,524940456,526376678,527811662,
    529245404,530677900,532109148,533539144,534967884,536395365,537821584,539246538,
    540670223,542092635,543513772,544933630,546352205,547769495,549185496,550600205,
    552013618,553425732,554836544,556246051,557654248,559061133,560466703,561870954,
    563273883,564675486,566075761,567474703,568872310,570268579,571663506,573057087,
    574449320,575840202,577229728,578617896,580004702,581390144,582774218,584156920,
    585538248,586918198,588296766,589673951,591049748,592424154,593797166,595168781,
    596538995,597907806,599275210,600641203,602005783,603368947,604730691,606091012,
    607449906,608807372,610163404,611518001,612871159,614222875,615573145,616921967,
    618269338,619615253,620959711,622302707,623644239,624984303,626322897,627660017,
    628995660,630329823,631662503,632993696,634323400,635651611,636978327,638303543,
    639627258,640949467,642270169,643589359,644907034,646223192,647537830,648850943,
    650162530,651472587,652781111,654088099,655393548,656697454,657999816,659300629,
    660599890,661897597,663193747,664488336,665781362,667072820,668362709,669651026,
    670937767,672222928,673506508,674788504,676068911,677347728,678624950,679900576,
    681174602,682447025,683717842,684987051,686254647,687520629,688784993,690047736,
    691308855,692568348,693826211,695082441,696337036,697589992,698841307,700090977,
    701339000,702585372,703830092,705073155,706314559,707554301,708792378,710028787,
    711263525,712496590,713727978,714957687,716185713,717412054,718636707,719859669,
    721080937,722300508,723518380,724734549,725949013,727161768,728372813,729582143,
    730789757,731995651,733199822,734402269,735602987,736801974,737999228,739194745,
    740388522,741580558,742770848,743959390,745146182,746331221,747514503,748696026,
    749875788,751053785,752230015,753404474,754577161,755748072,756917205,758084557,
    759250125,760413906,761575898,762736098,763894504,765051111,766205919,767358923,
    768510122,769659512,770807092,771952857,773096806,774238936,775379244,776517728,
    777654384,778789210,779922204,781053363,782182683,783310163,784435800,785559591,
    786681534,787801625,788919863,790036244,791150767,792263427,793374223,794483153,
    795590213,796695401,797798714,798900150,799999706,801097379,802193167,803287068,
    804379079,805469196,806557419,807643743,808728167,809810688,810891304,811970011,
    813046808,814121692,815194659,816265709,817334838,818402043,819467323,820530675,
    821592095,822651583,823709135,824764748,825818421,826870150,827919934,828967769,
    830013654,831057586,832099562,833139580,834177638,835213733,836247863,837280024,
    838310216,839338435,840364679,841388945,842411232,843431536,844449856,845466188,
    846480531,847492882,848503239,849511600,850517961,851522321,852524677,853525028,
    854523370,855519701,856514019,857506321,858496606,859484870,860471112,861455330,
    862437520,863417681,864395810,865371905,866345964,867317984,868287963,869255900,
    870221790,871185633,872147426,873107167,874064853,875020483,875974054,876925563,
    877875009,878822389,879767701,880710943,881652112,882591207,883528225,884463164,
    885396022,886326796,887255485,888182086,889106597,890029016,890949341,891867569,
    892783698,893697727,894609652,895519473,896427186,897332790,898236282,899137661,
    900036924,900934069,901829095,902721998,903612776,904501429,905387953,906272347,
    907154608,908034735,908912725,909788576,910662286,911533853,912403276,913270551,
    914135678,914998653,915859476,916718143,917574653,918429004,919281194,920131221,
    920979082,921824777,922668302,923509656,924348837,925185843,926020672,926853322,
    927683790,928512076,929338177,930162092,930983817,931803352,932620694,933435842,
    934248793,935059546,935868098,936674448,937478595,938280535,939080267,939877790,
    940673101,941466198,942257081,943045745,943832191,944616416,945398418,946178196,
    946955747,947731070,948504163,949275023,950043650,950810042,951574196,952336111,
    953095785,953853216,954608403,955361344,956112036,956860479,957606670,958350608,
    959092290,959831716,960568883,961303790,962036435,962766816,963494932,964220780,
    964944360,965665669,966384706,967101468,967815955,968528165,969238095,969945745,
    970651112,971354196,972054994,972753504,973449725,974143656,974835295,975524639,
    976211688,976896441,977578894,978259047,978936898,979612445,980285688,980956623,
    981625251,982291568,982955574,983617267,984276646,984933708,985588453,986240879,
    986890984,987538766,988184225,988827359,989468165,990106644,990742793,991376610,
    992008094,992637245,993264059,993888536,994510675,995130473,995747930,996363043,
    996975812,997586236,998194311,998800038,999403415,1000004439,1000603111,1001199428,
    1001793390,1002384994,1002974239,1003561124,1004145648,1004727809,1005307605,1005885036,
    1006460100,1007032796,1007603122,1008171077,1008736660,1009299870,1009860704,1010419162,
    1010975242,1011528943,1012080264,1012629204,1013175761,1013719934,1014261721,1014801122,
    1015338134,1015872758,1016404991,1016934832,1017462281,1017987335,1018509994,1019030256,
    1019548121,1020063586,1020576651,1021087314,1021595575,1022101432,1022604883,1023105929,
    1023604567,1024100796,1024594615,1025086024,1025575020,1026061603,1026545772,1027027525,
    1027506862,1027983780,1028458280,1028930359,1029400018,1029867254,1030332067,1030794455,
    1031254418,1031711954,1032167062,1032619742,1033069992,1033517810,1033963197,1034406151,
    1034846671,1035284755,1035720404,1036153615,1036584389,1037012723,1037438617,1037862069,
    1038283080,1038701647,1039117770,1039531448,1039942680,1040351465,1040757802,1041161689,
    1041563127,1041962114,1042358649,1042752731,1043144360,1043533534,1043920252,1044304514,
    1044686319,1045065665,1045442553,1045816980,1046188946,1046558451,1046925492,1047290071,
    1047652185,1048011834,1048369016,1048723732,1049075980,1049425759,1049773069,1050117909,
    1050460278,1050800175,1051137599,1051472550,1051805027,1052135029,1052462555,1052787604,
    1053110176,1053430270,1053747885,1054063021,1054375676,1054685850,1054993543,1055298753,
    1055601479,1055901722,1056199480,1056494753,1056787540,1057077840,1057365653,1057650977,
    1057933813,1058214159,1058492016,1058767381,1059040255,1059310638,1059578527,1059843923,
    1060106826,1060367233,1060625146,1060880563,1061133483,1061383907,1061631833,1061877261,
    1062120190,1062360620,1062598550,1062833980,1063066909,1063297336,1063525261,1063750684,
    1063973603,1064194019,1064411931,1064627338,1064840240,1065050636,1065258526,1065463909,
    1065666786,1065867154,1066065015,1066260367,1066453210,1066643544,1066831367,1067016680,
    1067199483,1067379774,1067557554,1067732821,1067905576,1068075818,1068243547,1068408763,
    1068571464,1068731650,1068889322,1069044479,1069197120,1069347245,1069494854,1069639946,
    1069782521,1069922579,1070060120,1070195142,1070327646,1070457632,1070585099,1070710046,
    1070832474,1070952382,1071069770,1071184638,1071296985,1071406812,1071514117,1071618901,
    1071721163,1071820903,1071918122,1072012818,1072104991,1072194642,1072281769,1072366374,
    1072448455,1072528012,1072605046,1072679556,1072751542,1072821003,1072887940,1072952352,
    1073014240,1073073603,1073130440,1073184753,1073236540,1073285802,1073332538,1073376748,
    1073418433,1073457592,1073494225,1073528332,1073559913,1073588967,1073615496,1073639498,
    1073660973,1073679922,1073696345,1073710241,1073721611,1073730454,1073736771,1073740561,
    1073741824,
};

/* Internal unsigned 32-bit turn retains fractional phase for symmetric windows. */
static inline int32_t spectrum_dsp_sin_phase32(uint32_t phase) {
    unsigned quadrant = phase >> 30;
    uint32_t offset = phase & 1073741823u;
    if (quadrant & 1u) offset = 1073741824u - offset;
    unsigned index = offset >> 20, fraction = offset & 1048575u;
    int32_t value = spectrum_dsp_quarter[index];
    if (fraction) value += (int32_t)(((int64_t)(spectrum_dsp_quarter[index + 1u] - value) * fraction + 524288) / 1048576);
    return quadrant >= 2u ? -value : value;
}

/* Public phase is one unsigned 16-bit turn; larger phases wrap. */
static inline int32_t spectrum_dsp_sin(uint32_t phase) {
    return spectrum_dsp_sin_phase32(phase << 16);
}

static inline uint32_t spectrum_dsp_sqrt(uint64_t value) {
    uint64_t bit = (uint64_t)1 << 62, root = 0;
    while (bit > value) bit >>= 2;
    while (bit) {
        if (value >= root + bit) { value -= root + bit; root = (root >> 1) + bit; }
        else root >>= 1;
        bit >>= 2;
    }
    return (uint32_t)root;
}

/* Binary logarithm with 16 fractional bits; zero is not a valid argument. */
static inline int32_t spectrum_dsp_log2_q16(uint32_t value) {
    if (!value) return INT32_MIN;
    unsigned exponent = 0;
    for (uint32_t v = value; v > 1u; v >>= 1) ++exponent;
    uint64_t normalized = (uint64_t)value << (31u - exponent);
    uint32_t fraction = 0;
    for (unsigned bit = 0; bit < 16u; ++bit) {
        normalized = (normalized * normalized) >> 31;
        if (normalized >= ((uint64_t)1 << 32)) {
            normalized >>= 1;
            fraction |= 1u << (15u - bit);
        }
    }
    return (int32_t)(exponent * 65536u + fraction);
}

/* 2^value returned in Q16, saturated to uint32; used for axes/gain only. */
static inline uint32_t spectrum_dsp_exp2_q16(int32_t value) {
    static const uint32_t factors[16] = {
        1518500250,1276901417,1170923762,1121280436,1097253708,1085434106,1079572136,1076653033,
        1075196443,1074468888,1074105294,1073923544,1073832680,1073787251,1073764537,1073753181
    };
    int32_t exponent = value / 65536, fraction = value % 65536;
    if (fraction < 0) { fraction += 65536; --exponent; }
    if (exponent >= 16) return UINT32_MAX;
    if (exponent < -17) return 0;
    uint64_t result = (uint64_t)1 << 30;
    for (unsigned bit = 0; bit < 16u; ++bit)
        if ((uint32_t)fraction & (1u << (15u - bit)))
            result = (result * factors[bit] + ((uint64_t)1 << 29)) >> 30;
    unsigned shift = (unsigned)(14 - exponent);
    if (exponent == 15) result <<= 1;
    else if (shift) result = (result + ((uint64_t)1 << (shift - 1u))) >> shift;
    return result > UINT32_MAX ? UINT32_MAX : (uint32_t)result;
}

/* Window coefficients are Q20, with negative flat-top lobes preserved. */
static inline int32_t spectrum_dsp_window_phase(spectrum_dsp_window window, uint32_t phase) {
    if (window == SPECTRUM_DSP_RECT) return 1048576;
    int32_t c1 = spectrum_dsp_sin_phase32(phase + 1073741824u);
    if (window == SPECTRUM_DSP_HANN) return (int32_t)((1073741824LL - c1) / 2048);
    if (window == SPECTRUM_DSP_HAMMING)
        return 566231 - (int32_t)((482345LL * c1) / 1073741824LL);
    int32_t c2 = spectrum_dsp_sin_phase32(phase * 2u + 1073741824u);
    if (window == SPECTRUM_DSP_BLACKMAN)
        return 440402 - (int32_t)((524288LL * c1) / 1073741824LL)
                      + (int32_t)((83886LL * c2) / 1073741824LL);
    int32_t c3 = spectrum_dsp_sin_phase32(phase * 3u + 1073741824u);
    int32_t c4 = spectrum_dsp_sin_phase32(phase * 4u + 1073741824u);
    return 226051 - (int32_t)((436870LL * c1) / 1073741824LL)
                  + (int32_t)((290731LL * c2) / 1073741824LL)
                  - (int32_t)((87639LL * c3) / 1073741824LL)
                  + (int32_t)((7285LL * c4) / 1073741824LL);
}
/* Random-access helper for tests/tooling; streaming uses a quotient/remainder
 * phase accumulator so no variable division is needed per PCM sample. */
static inline int32_t spectrum_dsp_window_at(spectrum_dsp_window window, unsigned index, unsigned size) {
    if (size < 2u || index >= size || (unsigned)window >= SPECTRUM_DSP_WINDOW_COUNT) return 0;
    uint32_t phase = (uint32_t)spectrum_dsp_div_u64_u32(((uint64_t)index << 32) + (size - 1u) / 2u, size - 1u);
    return spectrum_dsp_window_phase(window, phase);
}

static inline spectrum_dsp_config spectrum_dsp_defaults(void) {
    spectrum_dsp_config config = {16000, 2048, SPECTRUM_DSP_HANN, 20, 8000, true, true, 12, -60};
    return config;
}

static inline bool spectrum_dsp_config_valid(const spectrum_dsp_config *config) {
    return config && (config->sample_rate == 16000u || config->sample_rate == 48000u) && config->fft_size >= 256u && config->fft_size <= SPECTRUM_DSP_MAX_FFT &&
        !(config->fft_size & (config->fft_size - 1u)) &&
        (unsigned)config->window < SPECTRUM_DSP_WINDOW_COUNT &&
        config->low_hz < config->high_hz && config->high_hz <= config->sample_rate / 2u &&
        (!config->log_frequency || config->high_hz > (config->low_hz < 10u ? 10u : config->low_hz)) &&
        config->gain_db >= -24 && config->gain_db <= 60 &&
        config->threshold_db >= -90 && config->threshold_db <= -20;
}

/* Invalid configuration is atomic: no state change. Axis/gain/detection
 * changes retain data; FFT/window changes discard incomplete and old frames. */
static inline bool spectrum_dsp_configure(spectrum_dsp_state *state, const spectrum_dsp_config *config) {
    if (!state || !spectrum_dsp_config_valid(config)) return false;
    bool reset = !state->initialized || state->config.fft_size != config->fft_size || state->config.window != config->window || state->config.sample_rate != config->sample_rate;
    state->config = *config;
    state->gain_q16 = spectrum_dsp_exp2_q16((int32_t)config->gain_db * 10885294 / 1000);
    if (reset) {
        unsigned denominator = config->fft_size - 1u;
        state->phase_step = (uint32_t)spectrum_dsp_div_u64_u32((uint64_t)1 << 32, denominator);
        state->phase_remainder = (uint32_t)(((uint64_t)1 << 32) - (uint64_t)state->phase_step * denominator);
        uint32_t phase = 0, error = denominator / 2u;
        int64_t sum = 0;
        for (unsigned i = 0; i < config->fft_size; ++i) {
            sum += spectrum_dsp_window_phase(config->window, phase);
            phase += state->phase_step;
            error += state->phase_remainder;
            if (error >= denominator) { ++phase; error -= denominator; }
        }
        state->window_phase = 0;
        state->window_error = denominator / 2u;
        state->coherent_gain_q20 = (int32_t)spectrum_dsp_div_u64_u32((uint64_t)sum + config->fft_size / 2u, config->fft_size);
        state->used = 0;
        state->peak_bin = 0;
        state->has_transform = false;
        memset(state->amplitude_q24, 0, sizeof(state->amplitude_q24));
    }
    state->initialized = true;
    return true;
}

static inline bool spectrum_dsp_init(spectrum_dsp_state *state, const spectrum_dsp_config *config) {
    if (!state || !spectrum_dsp_config_valid(config)) return false;
    spectrum_dsp_config saved = *config; /* Allow reinitializing from state.config. */
    memset(state, 0, sizeof(*state));
    return spectrum_dsp_configure(state, &saved);
}

/* Shared bounded radix-2 kernel; callers own the window and normalization. */
static inline void spectrum_dsp_fft(int32_t *real, int32_t *imag, unsigned size) {
    for (unsigned i = 1, j = 0; i < size; ++i) {
        unsigned bit = size >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { int32_t t = real[i]; real[i] = real[j]; real[j] = t; }
    }
    memset(imag, 0, size * sizeof(imag[0]));
    for (unsigned length = 2; length <= size; length <<= 1) {
        unsigned half = length / 2u, step = 65536u / length;
        for (unsigned base = 0; base < size; base += length) {
            for (unsigned j = 0; j < half; ++j) {
                unsigned a = base + j, b = a + half;
                int32_t wr = spectrum_dsp_sin(j * step + 16384u), wi = -spectrum_dsp_sin(j * step);
                int32_t tr = (int32_t)(((int64_t)wr * real[b] - (int64_t)wi * imag[b]) / 1073741824LL);
                int32_t ti = (int32_t)(((int64_t)wr * imag[b] + (int64_t)wi * real[b]) / 1073741824LL);
                int32_t ar = real[a], ai = imag[a];
                real[a] = (ar + tr) / 2; imag[a] = (ai + ti) / 2;
                real[b] = (ar - tr) / 2; imag[b] = (ai - ti) / 2;
            }
        }
    }
}

static inline void spectrum_dsp_analyze(spectrum_dsp_state *state) {
    unsigned size = state->config.fft_size;
    spectrum_dsp_fft(state->real, state->imag, size);
    uint32_t peak = 0;
    state->peak_bin = 0;
    for (unsigned k = 0; k <= size / 2u; ++k) {
        int64_t re = state->real[k], im = state->imag[k];
        uint32_t magnitude = spectrum_dsp_sqrt((uint64_t)(re * re) + (uint64_t)(im * im));
        unsigned factor = (k == 0 || k == size / 2u) ? 2u : 4u;
        uint64_t scaled = (uint64_t)magnitude * factor * 1048576u;
        uint32_t amplitude = (uint32_t)spectrum_dsp_div_u64_u32(scaled + (uint32_t)state->coherent_gain_q20 / 2u, (uint32_t)state->coherent_gain_q20);
        state->amplitude_q24[k] = amplitude;
        if (amplitude > peak) { peak = amplitude; state->peak_bin = (uint16_t)k; }
    }
    ++state->transforms;
    state->has_transform = true;
}

/* Partial microphone reads assemble without allocations. At most one FFT
 * completes per call. A zero-sized read does not synthesize or advance data. */
static inline bool spectrum_dsp_feed(spectrum_dsp_state *state, const int16_t *pcm, size_t count) {
    if (!state || !pcm || !state->initialized || !spectrum_dsp_config_valid(&state->config) ||
        count > SPECTRUM_DSP_CHUNK || state->used >= state->config.fft_size || state->coherent_gain_q20 <= 0) return false;
    unsigned size = state->config.fft_size;
    /* Lifecycle clients may discard a partial block by setting used to zero. */
    if (count && !state->used) { state->window_phase = 0; state->window_error = (size - 1u) / 2u; }
    while (count--) {
        int32_t window = spectrum_dsp_window_phase(state->config.window, state->window_phase);
        state->real[state->used++] = (int32_t)((int64_t)*pcm++ * window / 4096);
        state->window_phase += state->phase_step;
        state->window_error += state->phase_remainder;
        if (state->window_error >= size - 1u) { ++state->window_phase; state->window_error -= size - 1u; }
        if (state->used == size) {
            spectrum_dsp_analyze(state);
            state->used = 0; state->window_phase = 0; state->window_error = (size - 1u) / 2u;
        }
    }
    return true;
}

/* Raw amplitude to display dBFS, in hundredths of a decibel. Gain shifts the
 * displayed value, not captured samples. Silence remains the -120 dB floor. */
static inline int16_t spectrum_dsp_amplitude_db(uint32_t amplitude_q24, int gain_db) {
    if (!amplitude_q24) return SPECTRUM_DSP_FLOOR_DB;
    int32_t logarithm = spectrum_dsp_log2_q16(amplitude_q24) - 24 * 65536;
    uint64_t numerator = (uint64_t)(logarithm < 0 ? -logarithm : logarithm) * 602060u;
    int32_t db = (int32_t)spectrum_dsp_div_u64_u32(numerator, 65536000u);
    if (logarithm < 0) db = -db;
    db += gain_db * 100;
    if (db < SPECTRUM_DSP_FLOOR_DB) db = SPECTRUM_DSP_FLOOR_DB;
    if (db > INT16_MAX) db = INT16_MAX;
    return (int16_t)db;
}

static inline uint16_t spectrum_dsp_level(const spectrum_dsp_state *state, uint32_t amplitude_q24) {
    if (!state) return 0;
    if (state->config.log_amplitude) {
        int32_t db = spectrum_dsp_amplitude_db(amplitude_q24, state->config.gain_db);
        if (db <= SPECTRUM_DSP_DISPLAY_FLOOR_DB) return 0;
        if (db >= 0) return 32767;
        return (uint16_t)((db - SPECTRUM_DSP_DISPLAY_FLOOR_DB) * 32767 / -SPECTRUM_DSP_DISPLAY_FLOOR_DB);
    }
    uint64_t level = (uint64_t)amplitude_q24 * state->gain_q16 / 33554432u;
    return level >= 32767u ? 32767u : (uint16_t)level;
}

/* Fractional Hz avoids losing low-frequency information during resampling. */
static inline uint32_t spectrum_dsp_frequency_q16(const spectrum_dsp_config *config, unsigned column, unsigned width) {
    if (!spectrum_dsp_config_valid(config)) return 0;
    unsigned low = config->low_hz;
    if (config->log_frequency && low < 10u) low = 10u;
    if (width < 2u || column == 0u) return low * 65536u;
    if (column >= width - 1u) return (uint32_t)config->high_hz * 65536u;
    if (!config->log_frequency)
        return low * 65536u + (uint32_t)spectrum_dsp_div_u64_u32((uint64_t)(config->high_hz - low) * 65536u * column, width - 1u);
    int32_t lo = spectrum_dsp_log2_q16(low), hi = spectrum_dsp_log2_q16(config->high_hz);
    return spectrum_dsp_exp2_q16(lo + (int32_t)spectrum_dsp_div_u64_u32((uint64_t)(hi - lo) * column, width - 1u));
}

static inline uint16_t spectrum_dsp_frequency_at(const spectrum_dsp_config *config, unsigned column, unsigned width) {
    return (uint16_t)((spectrum_dsp_frequency_q16(config, column, width) + 32768u) / 65536u);
}

static inline unsigned spectrum_dsp_column_at(const spectrum_dsp_config *config, unsigned hz, unsigned width) {
    if (!spectrum_dsp_config_valid(config) || width < 2u) return 0;
    unsigned low = config->low_hz;
    if (config->log_frequency && low < 10u) low = 10u;
    if (hz <= low) return 0;
    if (hz >= config->high_hz) return width - 1u;
    uint32_t numerator, denominator;
    if (config->log_frequency) {
        numerator = (uint32_t)(spectrum_dsp_log2_q16(hz) - spectrum_dsp_log2_q16(low));
        denominator = (uint32_t)(spectrum_dsp_log2_q16(config->high_hz) - spectrum_dsp_log2_q16(low));
    } else { numerator = hz - low; denominator = config->high_hz - low; }
    return (unsigned)spectrum_dsp_div_u64_u32((uint64_t)numerator * (width - 1u) + denominator / 2u, denominator);
}

/* Linear amplitude interpolation, not dB interpolation. */
static inline uint32_t spectrum_dsp_amplitude_values_at_q16(const spectrum_dsp_state *state, const uint32_t *values, uint32_t hz_q16) {
    if (!state || !state->initialized || !state->has_transform) return 0;
    uint32_t bin_q16 = (uint32_t)spectrum_dsp_div_u64_u32((uint64_t)hz_q16 * state->config.fft_size, state->config.sample_rate);
    unsigned last = state->config.fft_size / 2u, index = bin_q16 >> 16;
    if (index >= last) return values[last];
    unsigned fraction = bin_q16 & 65535u;
    return (uint32_t)(((uint64_t)values[index] * (65536u - fraction) +
        (uint64_t)values[index + 1u] * fraction + 32768u) / 65536u);
}

static inline uint32_t spectrum_dsp_amplitude_at_q16(const spectrum_dsp_state *state, uint32_t hz_q16) {
    return spectrum_dsp_amplitude_values_at_q16(state, state ? state->amplitude_q24 : NULL, hz_q16);
}

static inline int16_t spectrum_dsp_db_at_hz(const spectrum_dsp_state *state, unsigned hz) {
    if (!state || hz > state->config.sample_rate / 2u) return SPECTRUM_DSP_FLOOR_DB;
    return spectrum_dsp_amplitude_db(spectrum_dsp_amplitude_at_q16(state, hz * 65536u), state->config.gain_db);
}

/* Frequency labels track the strongest bin within max(3%f,2*fs/N).
 * This is a deterministic band detector, not semantic sound recognition. */
static inline int16_t spectrum_dsp_label_db(const spectrum_dsp_state *state, unsigned hz) {
    if (!state || !state->initialized || !state->has_transform || hz > state->config.sample_rate / 2u) return SPECTRUM_DSP_FLOOR_DB;
    unsigned n = state->config.fft_size, tolerance = (2u * state->config.sample_rate + n - 1u) / n;
    unsigned relative = (hz * 3u + 99u) / 100u;
    if (relative > tolerance) tolerance = relative;
    unsigned low = hz > tolerance ? hz - tolerance : 0;
    unsigned high = hz + tolerance;
    if (high > state->config.sample_rate / 2u) high = state->config.sample_rate / 2u;
    unsigned start = (low * n + state->config.sample_rate - 1u) / state->config.sample_rate;
    unsigned end = high * n / state->config.sample_rate;
    uint32_t peak = 0;
    for (unsigned i = start; i <= end; ++i) if (state->amplitude_q24[i] > peak) peak = state->amplitude_q24[i];
    return spectrum_dsp_amplitude_db(peak, state->config.gain_db);
}

static inline bool spectrum_dsp_detected(const spectrum_dsp_state *state, unsigned hz) {
    return state && state->initialized && state->has_transform &&
        spectrum_dsp_label_db(state, hz) > (int)state->config.threshold_db * 100;
}

/* Peak-preserving downsampling; interpolate amplitudes only where display
 * pixels are narrower than an FFT bin. No allocation and no state mutation.
 * The caller supplies width-sized buffers; either output may be NULL. */
static inline bool spectrum_dsp_resample_values(const spectrum_dsp_state *state, const uint32_t *values, uint16_t *levels_q15, int16_t *db_centi, unsigned width) {
    if (!state || !values || !state->initialized || !spectrum_dsp_config_valid(&state->config) ||
        !width || width > 4096u || (!levels_q15 && !db_centi)) return false;
    if (width == 1u) {
        uint32_t lo = spectrum_dsp_frequency_q16(&state->config, 0, 2), hi = spectrum_dsp_frequency_q16(&state->config, 1, 2);
        uint32_t amplitude = spectrum_dsp_amplitude_values_at_q16(state, values, lo), edge = spectrum_dsp_amplitude_values_at_q16(state, values, hi);
        if (edge > amplitude) amplitude = edge;
        unsigned first = (unsigned)((spectrum_dsp_div_u64_u32((uint64_t)lo * state->config.fft_size, state->config.sample_rate) + 65535u) >> 16);
        unsigned end = (unsigned)(spectrum_dsp_div_u64_u32((uint64_t)hi * state->config.fft_size, state->config.sample_rate) >> 16);
        if (state->has_transform)
            for (unsigned k = first; k <= end; ++k) if (values[k] > amplitude) amplitude = values[k];
        if (levels_q15) levels_q15[0] = spectrum_dsp_level(state, amplitude);
        if (db_centi) db_centi[0] = spectrum_dsp_amplitude_db(amplitude, state->config.gain_db);
        return true;
    }
    uint32_t center = spectrum_dsp_frequency_q16(&state->config, 0, width), previous = center;
    unsigned last = state->config.fft_size / 2u;
    for (unsigned x = 0; x < width; ++x) {
        uint32_t next = spectrum_dsp_frequency_q16(&state->config, x + 1u, width);
        uint32_t lo = x ? previous / 2u + center / 2u : center;
        uint32_t hi = x + 1u < width ? center / 2u + next / 2u : center;
        uint32_t bin_lo = (uint32_t)spectrum_dsp_div_u64_u32((uint64_t)lo * state->config.fft_size, state->config.sample_rate);
        uint32_t bin_hi = (uint32_t)spectrum_dsp_div_u64_u32((uint64_t)hi * state->config.fft_size, state->config.sample_rate);
        uint32_t amplitude = spectrum_dsp_amplitude_values_at_q16(state, values, center);
        if (state->has_transform && bin_hi - bin_lo >= 65536u) {
            unsigned first = (bin_lo + 65535u) >> 16, end = bin_hi >> 16;
            if (end > last) end = last;
            for (unsigned k = first; k <= end; ++k) if (values[k] > amplitude) amplitude = values[k];
        }
        if (levels_q15) levels_q15[x] = spectrum_dsp_level(state, amplitude);
        if (db_centi) db_centi[x] = spectrum_dsp_amplitude_db(amplitude, state->config.gain_db);
        previous = center; center = next;
    }
    return true;
}
static inline bool spectrum_dsp_resample(const spectrum_dsp_state *state, uint16_t *levels_q15, int16_t *db_centi, unsigned width) {
    return spectrum_dsp_resample_values(state, state ? state->amplitude_q24 : NULL, levels_q15, db_centi, width);
}
#endif
