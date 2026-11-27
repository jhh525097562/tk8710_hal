/****************************************************************************
**
**  Copyright  (c)  2023-2028,  All Rights Reserved.
**
**  File          : Cls_BFSelect.h
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer    	Date        Rev       	Description
**  --------------- ---------- 	-------- ------------------------------
**
**  Notice: Beamforming
**
****************************************************************************/
#ifndef __Cls_BFSelect
#define __Cls_BFSelect

//#define DUMP_MATLAB_BFSelect

#include <cmath>
#include "Lib_complex.h"
#include "Lib_fixpoint.h"
#include "Lib_random.h"
#include "Lib_utility.h"
#include "common_type.h"
#include "Lib_dump.h"
#include <iomanip>
#include "Cls_log10.h"

//#define Table_FIX

typedef struct BFSelect_CFG
{
    //Read Write
    int     ant_num         ;
    int     fft_len         ;
    int     ifft_len        ;
    int     rx_d_win_i      ;
    int     gw_rxbf_mode    ;
    int     DSSS            ;
    int     bf_th           ;
    double  bf_alpha        ;
    int     slidedatanum    ;
}BFSelect_CFG;

int sqrt_table[2049] = {46341, 46352, 46364, 46375, 46386, 46397, 46409, 46420, 46431, 46443, 46454, 46465, 46477, 46488, 46499, 46510, 46522, 46533, 46544, 46555, 46567, 46578, 46589, 46600, 46612, 46623, 46634, 46645, 46657, 46668, 46679, 46690,
                        46702, 46713, 46724, 46735, 46746, 46758, 46769, 46780, 46791, 46803, 46814, 46825, 46836, 46847, 46858, 46870, 46881, 46892, 46903, 46914, 46926, 46937, 46948, 46959, 46970, 46981, 46993, 47004, 47015, 47026, 47037, 47048,
                        47059, 47071, 47082, 47093, 47104, 47115, 47126, 47137, 47149, 47160, 47171, 47182, 47193, 47204, 47215, 47226, 47237, 47248, 47260, 47271, 47282, 47293, 47304, 47315, 47326, 47337, 47348, 47359, 47370, 47381, 47393, 47404,
                        47415, 47426, 47437, 47448, 47459, 47470, 47481, 47492, 47503, 47514, 47525, 47536, 47547, 47558, 47569, 47580, 47591, 47602, 47613, 47624, 47635, 47646, 47657, 47668, 47679, 47690, 47701, 47712, 47723, 47734, 47745, 47756,
                        47767, 47778, 47789, 47800, 47811, 47822, 47833, 47844, 47855, 47866, 47877, 47888, 47899, 47910, 47921, 47932, 47942, 47953, 47964, 47975, 47986, 47997, 48008, 48019, 48030, 48041, 48052, 48063, 48074, 48084, 48095, 48106,
                        48117, 48128, 48139, 48150, 48161, 48172, 48182, 48193, 48204, 48215, 48226, 48237, 48248, 48259, 48269, 48280, 48291, 48302, 48313, 48324, 48335, 48345, 48356, 48367, 48378, 48389, 48400, 48410, 48421, 48432, 48443, 48454,
                        48465, 48475, 48486, 48497, 48508, 48519, 48529, 48540, 48551, 48562, 48573, 48583, 48594, 48605, 48616, 48627, 48637, 48648, 48659, 48670, 48680, 48691, 48702, 48713, 48723, 48734, 48745, 48756, 48766, 48777, 48788, 48799,
                        48809, 48820, 48831, 48842, 48852, 48863, 48874, 48885, 48895, 48906, 48917, 48927, 48938, 48949, 48960, 48970, 48981, 48992, 49002, 49013, 49024, 49035, 49045, 49056, 49067, 49077, 49088, 49099, 49109, 49120, 49131, 49141,
                        49152, 49163, 49173, 49184, 49195, 49205, 49216, 49227, 49237, 49248, 49259, 49269, 49280, 49290, 49301, 49312, 49322, 49333, 49344, 49354, 49365, 49375, 49386, 49397, 49407, 49418, 49429, 49439, 49450, 49460, 49471, 49482,
                        49492, 49503, 49513, 49524, 49535, 49545, 49556, 49566, 49577, 49587, 49598, 49609, 49619, 49630, 49640, 49651, 49661, 49672, 49682, 49693, 49704, 49714, 49725, 49735, 49746, 49756, 49767, 49777, 49788, 49798, 49809, 49819,
                        49830, 49841, 49851, 49862, 49872, 49883, 49893, 49904, 49914, 49925, 49935, 49946, 49956, 49967, 49977, 49988, 49998, 50009, 50019, 50030, 50040, 50050, 50061, 50071, 50082, 50092, 50103, 50113, 50124, 50134, 50145, 50155,
                        50166, 50176, 50186, 50197, 50207, 50218, 50228, 50239, 50249, 50260, 50270, 50280, 50291, 50301, 50312, 50322, 50332, 50343, 50353, 50364, 50374, 50385, 50395, 50405, 50416, 50426, 50437, 50447, 50457, 50468, 50478, 50488,
                        50499, 50509, 50520, 50530, 50540, 50551, 50561, 50572, 50582, 50592, 50603, 50613, 50623, 50634, 50644, 50654, 50665, 50675, 50685, 50696, 50706, 50716, 50727, 50737, 50747, 50758, 50768, 50778, 50789, 50799, 50809, 50820,
                        50830, 50840, 50851, 50861, 50871, 50882, 50892, 50902, 50912, 50923, 50933, 50943, 50954, 50964, 50974, 50985, 50995, 51005, 51015, 51026, 51036, 51046, 51056, 51067, 51077, 51087, 51097, 51108, 51118, 51128, 51139, 51149,
                        51159, 51169, 51180, 51190, 51200, 51210, 51220, 51231, 51241, 51251, 51261, 51272, 51282, 51292, 51302, 51313, 51323, 51333, 51343, 51353, 51364, 51374, 51384, 51394, 51404, 51415, 51425, 51435, 51445, 51455, 51466, 51476,
                        51486, 51496, 51506, 51516, 51527, 51537, 51547, 51557, 51567, 51577, 51588, 51598, 51608, 51618, 51628, 51638, 51649, 51659, 51669, 51679, 51689, 51699, 51709, 51720, 51730, 51740, 51750, 51760, 51770, 51780, 51791, 51801,
                        51811, 51821, 51831, 51841, 51851, 51861, 51871, 51882, 51892, 51902, 51912, 51922, 51932, 51942, 51952, 51962, 51972, 51983, 51993, 52003, 52013, 52023, 52033, 52043, 52053, 52063, 52073, 52083, 52093, 52103, 52113, 52124,
                        52134, 52144, 52154, 52164, 52174, 52184, 52194, 52204, 52214, 52224, 52234, 52244, 52254, 52264, 52274, 52284, 52294, 52304, 52314, 52324, 52334, 52344, 52354, 52364, 52374, 52384, 52394, 52404, 52414, 52424, 52434, 52444,
                        52454, 52464, 52474, 52484, 52494, 52504, 52514, 52524, 52534, 52544, 52554, 52564, 52574, 52584, 52594, 52604, 52614, 52624, 52634, 52644, 52654, 52664, 52674, 52684, 52694, 52704, 52714, 52724, 52734, 52743, 52753, 52763,
                        52773, 52783, 52793, 52803, 52813, 52823, 52833, 52843, 52853, 52863, 52873, 52882, 52892, 52902, 52912, 52922, 52932, 52942, 52952, 52962, 52972, 52981, 52991, 53001, 53011, 53021, 53031, 53041, 53051, 53061, 53070, 53080,
                        53090, 53100, 53110, 53120, 53130, 53140, 53149, 53159, 53169, 53179, 53189, 53199, 53209, 53218, 53228, 53238, 53248, 53258, 53268, 53278, 53287, 53297, 53307, 53317, 53327, 53337, 53346, 53356, 53366, 53376, 53386, 53395,
                        53405, 53415, 53425, 53435, 53445, 53454, 53464, 53474, 53484, 53494, 53503, 53513, 53523, 53533, 53543, 53552, 53562, 53572, 53582, 53592, 53601, 53611, 53621, 53631, 53640, 53650, 53660, 53670, 53679, 53689, 53699, 53709,
                        53719, 53728, 53738, 53748, 53758, 53767, 53777, 53787, 53797, 53806, 53816, 53826, 53836, 53845, 53855, 53865, 53874, 53884, 53894, 53904, 53913, 53923, 53933, 53943, 53952, 53962, 53972, 53981, 53991, 54001, 54011, 54020,
                        54030, 54040, 54049, 54059, 54069, 54078, 54088, 54098, 54108, 54117, 54127, 54137, 54146, 54156, 54166, 54175, 54185, 54195, 54204, 54214, 54224, 54233, 54243, 54253, 54262, 54272, 54282, 54291, 54301, 54311, 54320, 54330,
                        54340, 54349, 54359, 54369, 54378, 54388, 54397, 54407, 54417, 54426, 54436, 54446, 54455, 54465, 54474, 54484, 54494, 54503, 54513, 54523, 54532, 54542, 54551, 54561, 54571, 54580, 54590, 54599, 54609, 54619, 54628, 54638,
                        54647, 54657, 54667, 54676, 54686, 54695, 54705, 54715, 54724, 54734, 54743, 54753, 54762, 54772, 54782, 54791, 54801, 54810, 54820, 54829, 54839, 54849, 54858, 54868, 54877, 54887, 54896, 54906, 54915, 54925, 54935, 54944,
                        54954, 54963, 54973, 54982, 54992, 55001, 55011, 55020, 55030, 55039, 55049, 55058, 55068, 55077, 55087, 55097, 55106, 55116, 55125, 55135, 55144, 55154, 55163, 55173, 55182, 55192, 55201, 55211, 55220, 55230, 55239, 55249,
                        55258, 55268, 55277, 55287, 55296, 55305, 55315, 55324, 55334, 55343, 55353, 55362, 55372, 55381, 55391, 55400, 55410, 55419, 55429, 55438, 55447, 55457, 55466, 55476, 55485, 55495, 55504, 55514, 55523, 55533, 55542, 55551,
                        55561, 55570, 55580, 55589, 55599, 55608, 55617, 55627, 55636, 55646, 55655, 55665, 55674, 55683, 55693, 55702, 55712, 55721, 55730, 55740, 55749, 55759, 55768, 55777, 55787, 55796, 55806, 55815, 55824, 55834, 55843, 55853,
                        55862, 55871, 55881, 55890, 55900, 55909, 55918, 55928, 55937, 55946, 55956, 55965, 55975, 55984, 55993, 56003, 56012, 56021, 56031, 56040, 56049, 56059, 56068, 56077, 56087, 56096, 56105, 56115, 56124, 56134, 56143, 56152,
                        56162, 56171, 56180, 56190, 56199, 56208, 56218, 56227, 56236, 56245, 56255, 56264, 56273, 56283, 56292, 56301, 56311, 56320, 56329, 56339, 56348, 56357, 56367, 56376, 56385, 56394, 56404, 56413, 56422, 56432, 56441, 56450,
                        56459, 56469, 56478, 56487, 56497, 56506, 56515, 56524, 56534, 56543, 56552, 56562, 56571, 56580, 56589, 56599, 56608, 56617, 56626, 56636, 56645, 56654, 56663, 56673, 56682, 56691, 56700, 56710, 56719, 56728, 56737, 56747,
                        56756, 56765, 56774, 56784, 56793, 56802, 56811, 56820, 56830, 56839, 56848, 56857, 56867, 56876, 56885, 56894, 56903, 56913, 56922, 56931, 56940, 56950, 56959, 56968, 56977, 56986, 56996, 57005, 57014, 57023, 57032, 57041,
                        57051, 57060, 57069, 57078, 57087, 57097, 57106, 57115, 57124, 57133, 57143, 57152, 57161, 57170, 57179, 57188, 57198, 57207, 57216, 57225, 57234, 57243, 57252, 57262, 57271, 57280, 57289, 57298, 57307, 57317, 57326, 57335,
                        57344, 57353, 57362, 57371, 57381, 57390, 57399, 57408, 57417, 57426, 57435, 57444, 57454, 57463, 57472, 57481, 57490, 57499, 57508, 57517, 57527, 57536, 57545, 57554, 57563, 57572, 57581, 57590, 57599, 57609, 57618, 57627,
                        57636, 57645, 57654, 57663, 57672, 57681, 57690, 57699, 57709, 57718, 57727, 57736, 57745, 57754, 57763, 57772, 57781, 57790, 57799, 57808, 57817, 57827, 57836, 57845, 57854, 57863, 57872, 57881, 57890, 57899, 57908, 57917,
                        57926, 57935, 57944, 57953, 57962, 57971, 57980, 57990, 57999, 58008, 58017, 58026, 58035, 58044, 58053, 58062, 58071, 58080, 58089, 58098, 58107, 58116, 58125, 58134, 58143, 58152, 58161, 58170, 58179, 58188, 58197, 58206,
                        58215, 58224, 58233, 58242, 58251, 58260, 58269, 58278, 58287, 58296, 58305, 58314, 58323, 58332, 58341, 58350, 58359, 58368, 58377, 58386, 58395, 58404, 58413, 58422, 58431, 58440, 58449, 58458, 58467, 58476, 58485, 58494,
                        58503, 58512, 58521, 58529, 58538, 58547, 58556, 58565, 58574, 58583, 58592, 58601, 58610, 58619, 58628, 58637, 58646, 58655, 58664, 58673, 58682, 58690, 58699, 58708, 58717, 58726, 58735, 58744, 58753, 58762, 58771, 58780,
                        58789, 58798, 58806, 58815, 58824, 58833, 58842, 58851, 58860, 58869, 58878, 58887, 58896, 58904, 58913, 58922, 58931, 58940, 58949, 58958, 58967, 58976, 58985, 58993, 59002, 59011, 59020, 59029, 59038, 59047, 59056, 59064,
                        59073, 59082, 59091, 59100, 59109, 59118, 59127, 59135, 59144, 59153, 59162, 59171, 59180, 59189, 59197, 59206, 59215, 59224, 59233, 59242, 59251, 59259, 59268, 59277, 59286, 59295, 59304, 59312, 59321, 59330, 59339, 59348,
                        59357, 59366, 59374, 59383, 59392, 59401, 59410, 59418, 59427, 59436, 59445, 59454, 59463, 59471, 59480, 59489, 59498, 59507, 59515, 59524, 59533, 59542, 59551, 59559, 59568, 59577, 59586, 59595, 59603, 59612, 59621, 59630,
                        59639, 59647, 59656, 59665, 59674, 59683, 59691, 59700, 59709, 59718, 59727, 59735, 59744, 59753, 59762, 59770, 59779, 59788, 59797, 59805, 59814, 59823, 59832, 59841, 59849, 59858, 59867, 59876, 59884, 59893, 59902, 59911,
                        59919, 59928, 59937, 59946, 59954, 59963, 59972, 59981, 59989, 59998, 60007, 60015, 60024, 60033, 60042, 60050, 60059, 60068, 60077, 60085, 60094, 60103, 60112, 60120, 60129, 60138, 60146, 60155, 60164, 60173, 60181, 60190,
                        60199, 60207, 60216, 60225, 60233, 60242, 60251, 60260, 60268, 60277, 60286, 60294, 60303, 60312, 60320, 60329, 60338, 60347, 60355, 60364, 60373, 60381, 60390, 60399, 60407, 60416, 60425, 60433, 60442, 60451, 60459, 60468,
                        60477, 60485, 60494, 60503, 60511, 60520, 60529, 60537, 60546, 60555, 60563, 60572, 60581, 60589, 60598, 60607, 60615, 60624, 60633, 60641, 60650, 60658, 60667, 60676, 60684, 60693, 60702, 60710, 60719, 60728, 60736, 60745,
                        60753, 60762, 60771, 60779, 60788, 60797, 60805, 60814, 60822, 60831, 60840, 60848, 60857, 60866, 60874, 60883, 60891, 60900, 60909, 60917, 60926, 60934, 60943, 60952, 60960, 60969, 60977, 60986, 60995, 61003, 61012, 61020,
                        61029, 61038, 61046, 61055, 61063, 61072, 61081, 61089, 61098, 61106, 61115, 61123, 61132, 61141, 61149, 61158, 61166, 61175, 61183, 61192, 61201, 61209, 61218, 61226, 61235, 61243, 61252, 61261, 61269, 61278, 61286, 61295,
                        61303, 61312, 61320, 61329, 61338, 61346, 61355, 61363, 61372, 61380, 61389, 61397, 61406, 61414, 61423, 61431, 61440, 61449, 61457, 61466, 61474, 61483, 61491, 61500, 61508, 61517, 61525, 61534, 61542, 61551, 61559, 61568,
                        61576, 61585, 61593, 61602, 61610, 61619, 61627, 61636, 61644, 61653, 61661, 61670, 61678, 61687, 61695, 61704, 61712, 61721, 61729, 61738, 61746, 61755, 61763, 61772, 61780, 61789, 61797, 61806, 61814, 61823, 61831, 61840,
                        61848, 61857, 61865, 61874, 61882, 61891, 61899, 61908, 61916, 61924, 61933, 61941, 61950, 61958, 61967, 61975, 61984, 61992, 62001, 62009, 62018, 62026, 62034, 62043, 62051, 62060, 62068, 62077, 62085, 62094, 62102, 62110,
                        62119, 62127, 62136, 62144, 62153, 62161, 62170, 62178, 62186, 62195, 62203, 62212, 62220, 62229, 62237, 62245, 62254, 62262, 62271, 62279, 62287, 62296, 62304, 62313, 62321, 62330, 62338, 62346, 62355, 62363, 62372, 62380,
                        62388, 62397, 62405, 62414, 62422, 62430, 62439, 62447, 62456, 62464, 62472, 62481, 62489, 62498, 62506, 62514, 62523, 62531, 62539, 62548, 62556, 62565, 62573, 62581, 62590, 62598, 62607, 62615, 62623, 62632, 62640, 62648,
                        62657, 62665, 62673, 62682, 62690, 62699, 62707, 62715, 62724, 62732, 62740, 62749, 62757, 62765, 62774, 62782, 62790, 62799, 62807, 62816, 62824, 62832, 62841, 62849, 62857, 62866, 62874, 62882, 62891, 62899, 62907, 62916,
                        62924, 62932, 62941, 62949, 62957, 62966, 62974, 62982, 62991, 62999, 63007, 63016, 63024, 63032, 63040, 63049, 63057, 63065, 63074, 63082, 63090, 63099, 63107, 63115, 63124, 63132, 63140, 63149, 63157, 63165, 63173, 63182,
                        63190, 63198, 63207, 63215, 63223, 63231, 63240, 63248, 63256, 63265, 63273, 63281, 63289, 63298, 63306, 63314, 63323, 63331, 63339, 63347, 63356, 63364, 63372, 63381, 63389, 63397, 63405, 63414, 63422, 63430, 63438, 63447,
                        63455, 63463, 63471, 63480, 63488, 63496, 63505, 63513, 63521, 63529, 63538, 63546, 63554, 63562, 63571, 63579, 63587, 63595, 63604, 63612, 63620, 63628, 63636, 63645, 63653, 63661, 63669, 63678, 63686, 63694, 63702, 63711,
                        63719, 63727, 63735, 63743, 63752, 63760, 63768, 63776, 63785, 63793, 63801, 63809, 63817, 63826, 63834, 63842, 63850, 63859, 63867, 63875, 63883, 63891, 63900, 63908, 63916, 63924, 63932, 63941, 63949, 63957, 63965, 63973,
                        63982, 63990, 63998, 64006, 64014, 64023, 64031, 64039, 64047, 64055, 64063, 64072, 64080, 64088, 64096, 64104, 64113, 64121, 64129, 64137, 64145, 64153, 64162, 64170, 64178, 64186, 64194, 64202, 64211, 64219, 64227, 64235,
                        64243, 64251, 64260, 64268, 64276, 64284, 64292, 64300, 64309, 64317, 64325, 64333, 64341, 64349, 64357, 64366, 64374, 64382, 64390, 64398, 64406, 64414, 64423, 64431, 64439, 64447, 64455, 64463, 64471, 64479, 64488, 64496,
                        64504, 64512, 64520, 64528, 64536, 64544, 64553, 64561, 64569, 64577, 64585, 64593, 64601, 64609, 64618, 64626, 64634, 64642, 64650, 64658, 64666, 64674, 64682, 64691, 64699, 64707, 64715, 64723, 64731, 64739, 64747, 64755,
                        64763, 64772, 64780, 64788, 64796, 64804, 64812, 64820, 64828, 64836, 64844, 64852, 64861, 64869, 64877, 64885, 64893, 64901, 64909, 64917, 64925, 64933, 64941, 64949, 64957, 64966, 64974, 64982, 64990, 64998, 65006, 65014,
                        65022, 65030, 65038, 65046, 65054, 65062, 65070, 65078, 65086, 65095, 65103, 65111, 65119, 65127, 65135, 65143, 65151, 65159, 65167, 65175, 65183, 65191, 65199, 65207, 65215, 65223, 65231, 65239, 65247, 65255, 65263, 65271,
                        65279, 65288, 65296, 65304, 65312, 65320, 65328, 65336, 65344, 65352, 65360, 65368, 65376, 65384, 65392, 65400, 65408, 65416, 65424, 65432, 65440, 65448, 65456, 65464, 65472, 65480, 65488, 65496, 65504, 65512, 65520, 65528,
                        65536};//16.10;

double PSKTable2[50] = {-17.3945482822602,-16.3976380863221,-15.3950195168880,-14.4007762419357,-13.3831826663873,
                        -12.3767076164324,-11.4085955766138,-10.4015551529943,-9.38890186988488,-8.38778120752433,
                        -7.39218859129456,-6.39709880958969,-5.39486820298353,-4.39404255938965,-3.38759104970184,
                        -2.39180982113118,-1.37907208917066,-0.392129594334399,0.617538577138788,1.59903859566268,
                        2.60877077449104,3.60494435074558,4.61135937388143,5.61053108175587,6.60433802863654,
                        7.62060525807621,8.61645738182327,9.61816966499719,10.5929050172755,11.6182866222179,
                        12.6106571681113,13.6112381431479,14.6040377554175,15.6059991774945,16.6123395972270,
                        17.6064636013194,18.5885387129797,19.6055443257921,20.5989891619641,21.6179609113884,
                        22.6144916826656,23.6083051467427,24.6061435736380,25.6218748763550,26.6021405275683,
                        27.6246643555273,28.5995115363202,29.6092007926904,30.6132889666215,31.6156166108765};
int PSKTable2_fxp[50] = {   -17812, -16791, -15764, -14746, -13704, -12674,
                            -11682, -10651, -9614 , -8589 , -7570 , -6551,
                            -5524,  -4499 , -3469 , -2449 , -1412 , -402,
                            632   ,  1637  ,  2671 ,  3691 , 4722  ,  5745,
                            6763  ,  7803  , 8823  ,  9849 , 10847 ,  11897,
                            12913 ,  13938 , 14955 , 15981 , 17011 ,  18029,
                            19035 ,  20076 , 21093 , 22137 , 23157 ,  24175,
                            25197 ,  26237 , 27241 , 28288 , 29286 ,  30320,
                            31348 ,  32374 };
double PSKTable3[50] =  {2.00215329580935,1.99048968880195,1.99958662495499,1.99974709165788,1.99246970549059,
                        2.00101252993813,1.99538342939687,1.99574804706337,1.99069802691786,1.98205072079350,
                        1.98019663168878,1.97227545931537,1.96092729849497,1.95491650334325,1.92900219428986,
                        1.91097541155931,1.87551313262582,1.84369761665078,1.80579564539052,1.76168224792938,
                        1.71316794042407,1.67045889540474,1.62632895819072,1.58207930089782,1.54068278945583,
                        1.49790313936442,1.46352794747559,1.42966039020490,1.40221339193184,1.37683873154820,
                        1.35458508143333,1.33592182099955,1.31978477763654,1.30512769932950,1.29198485558999,
                        1.28098152768237,1.27126305374246,1.26175869728187,1.25347644845097,1.24546899415982,
                        1.23793964632756,1.23093364485508,1.22429902076970,1.21757300984002,1.21133484373866,
                        1.20515159480469,1.19901717552784,1.19309953618963,1.18729495266099,1.18143708707548};
double PSKTable3_fxp[50] = {2050,2038,2048,2048,2040,
                            2049,2043,2044,2038,2030,
                            2028,2020,2008,2002,1975,
                            1957,1921,1888,1849,1804,
                            1754,1711,1665,1620,1578,
                            1534,1499,1464,1436,1410,
                            1387,1368,1351,1336,1323,
                            1312,1302,1292,1284,1275,
                            1268,1260,1254,1247,1240,
                            1234,1228,1222,1216,1210};
int snr_table[50] = {   37,46,57,72,90,
                        112,138,171,211,259,
                        316,382,459,546,644,
                        749,863,978,1097,1210,
                        1323,1426,1522,1607,1681,
                        1746,1800,1846,1884,1916,
                        1942,1963,1979,1993,2004,
                        2013,2020,2026,2030,2034,
                        2037,2039,2041,2042,2044,
                        2044,2045,2046,2046,2047};

double snr_table_double[50] = {0.0178938423103594,0.0224075326500589,0.0280631298379426,0.0350296925754614,0.0438729984173428,
                                0.0546894723041863,0.0674254714384614,0.0835511999615037,0.103226793474193,0.126600346552928,
                                0.154189323721459,0.186489114775157,0.224050788174543,0.266634444682648,0.314315216729513,
                                0.365694001338499,0.421274633654724,0.477442528932425,0.535488602868632,0.591022088578625,
                                0.645814533391540,0.696369543060041,0.743035281472935,0.784463964527047,0.820642276291645,
                                0.852545786638964,0.879108261461809,0.901558760925282,0.919761173114643,0.935547859210101,
                                0.948029586261586,0.958277762047460,0.966518389039047,0.973231967680436,0.978650213544952,
                                0.982943798160469,0.986348619561521,0.989167827930507,0.991363574294585,0.993157386876426,
                                0.994552733199903,0.995662080854414,0.996549480312872,0.997267098346046,0.997818087554843,
                                0.998275021172895,0.998621363732682,0.998907038173264,0.999132450691556,0.999311127160214};
template <class T>
class Cls_BFSelect
{
	public:
		Cls_BFSelect() {
            m_in                = NULL;
            m_ah_in             = NULL;
            m_p0_in             = NULL;
            m_e0_in             = NULL;

            m_snrtable          = NULL;
            m_snrEtable         = NULL;

            m_SumdataAbs        = NULL;
            m_dataAbs           = NULL;
            m_dataNormal        = NULL;
            m_TmpSumDataAbs     = NULL;
            m_TmpP              = NULL;
            m_SNRSelect1        = NULL;
            m_PdB               = NULL;
            m_select            = NULL;
            m_Pmsk              = NULL;
            m_diff              = NULL;
            m_RxBeamforming_out = NULL;
            m_out               = NULL;

            m_DataIn            = NULL;
            m_AHIn              = NULL;
		}
		~Cls_BFSelect() {
            free(m_in)                  ;
            free(m_ah_in)               ;
            free(m_p0_in)               ;
            free(m_e0_in)               ;

            free(m_snrtable)            ;
            free(m_snrEtable)           ;
            free(m_SumdataAbs)          ;
            free(m_dataAbs)             ;
            free(m_dataNormal)          ;

            free(m_TmpSumDataAbs)       ;
            free(m_TmpP)                ;
            free(m_SNRSelect1)          ;
            free(m_PdB)                 ;
            free(m_select)              ;
            free(m_Pmsk)                ;
            free(m_diff)                ;
            free(m_RxBeamforming_out)   ;
            free(m_out)                 ;
            free(m_DataIn)              ;
            free(m_AHIn)                ;
#ifdef DUMP_MATLAB_BFSelect
            bf_mat_out1.close();
            bf_mat_out2.close();
            bf_mat_out3.close();
            bf_mat_out4.close();
#endif
		}
		void			init(char * module_name);
		void			go();
		void 			reset();
		void			configure(BFSelect_CFG cfg);

        int             m_bf_o_index;
        double          m_bf_o_P[16];
        double          m_bf_o_E[16];
        T               *m_RxBeamforming_out;

        int             m_shift;
        Complex<double> **m_DataIn;
        Complex<T>      * m_AHIn;
        Complex<T>      *m_out;
	private:
        void    BFSelectionModule();
        double  sqrt_t(double in);
        BFSelect_CFG        m_cfg;
        Complex<double>     **m_in;
        Complex<T>          m_in_fxp;
        Complex<double>     *m_ah_in;
        Complex<T>          m_ah_in_fxp;
//        Complex<T>          *m_out;
        T                   m_p0_fxp;
        T                   m_e0_fxp;
        T                   m_bf_matrix_in_fxp;
        T                   m_ahnorm_fxp;
        T                   m_ahnorm_sum_fxp;
        T                   m_sqrt_fft_frac;
        T                   m_dataAbs_fxp;
        T                   m_TmpSumDataAbs_fxp;
        T                   m_TmpSumDataAbs_frac_fxp;
        T                   m_sqrt_dataabs_in_fxp;
        T                   m_dataNormal_fxp;
        T                   m_dataNormal_tmp_fxp;
        T                   m_bf_o_E_fxp;
        T                   m_bf_o_P_fxp;
        T                   m_diff_fxp;
        T                   m_PdB_fxp;
        T                   m_select_fxp;
        T                   m_sqrt_dsss;
        T                   m_dataNormal_fxp_in;
        T                   m_SNRSelect1_fxp;
        T                   m_PdB_tmp;
        T                   m_log_in_fxp;
        double              *m_p0_in;
        double              *m_e0_in;

        int                 m_ant_num     ;
        int                 m_fft_len     ;
        int                 m_ifft_len    ;
        int                 m_rx_d_win_i  ;
        int                 m_gw_rxbf_mode;

        int                 m_snrtable_len;
        T                   *m_snrtable;
        T                   *m_snrEtable;
        T                   m_powExp;
        T                   m_gwrx_bfselect_the;
        T                   m_gwrx_bfselect_alpha;
        int                 m_slideDataNum;
        int                 m_slideSectionlen;
        T                   m_alpha;
        double              *m_SumdataAbs;
        double              **m_dataAbs;
        double              **m_dataNormal;

        double              *m_TmpSumDataAbs;
        double              *m_TmpP;
        T                   m_TmpP_fxp;
        double              *m_SNRSelect1;
        double              *m_PdB;
        double              *m_select;
        double              *m_Pmsk;
        double              *m_diff;
        int                 m_DSSS;
        void                shift_cal();
        Cls_log10<T>        m_log10;
#ifdef DUMP_MATLAB_BFSelect
        //ofstram       m_file_in;
        std::ofstream           bf_mat_out1;
        std::ofstream           bf_mat_out2;
        std::ofstream           bf_mat_out3;
        std::ofstream           bf_mat_out4;
#endif
};

template <class T>
void Cls_BFSelect<T>::configure(BFSelect_CFG cfg)
{
	m_cfg = cfg;

    m_ant_num      = m_cfg.ant_num;
    m_fft_len      = m_cfg.fft_len;
    m_ifft_len     = m_cfg.ifft_len;
    m_rx_d_win_i   = m_cfg.rx_d_win_i;
    m_gw_rxbf_mode = m_cfg.gw_rxbf_mode;
    m_DSSS         = m_cfg.DSSS;
}

template <class T>
void Cls_BFSelect<T>::init(char * module_name)
{
    m_slideDataNum = DOUBLE_POW2[64 + m_cfg.slidedatanum]; //0:1, 1:2, 2:4, 3:8, 4:16

    m_slideSectionlen  = m_ifft_len / m_slideDataNum;

    m_snrtable_len = 50;

    m_in                = safe_new(m_in, m_ant_num, m_ifft_len);
    m_DataIn            = safe_new(m_DataIn, m_ant_num, m_ifft_len);
    m_ah_in             = safe_new(m_ah_in, m_ant_num);
    m_AHIn              = safe_new(m_AHIn, m_ant_num);
    m_p0_in             = safe_new(m_p0_in, m_ant_num);
    m_e0_in             = safe_new(m_e0_in, m_ant_num);

    m_dataNormal        = safe_new(m_dataNormal, m_ant_num, m_ifft_len);
    m_TmpSumDataAbs     = safe_new(m_TmpSumDataAbs, m_ant_num);
    m_TmpP              = safe_new(m_TmpP, m_ant_num);
    m_SNRSelect1        = safe_new(m_SNRSelect1, m_ant_num);
    m_PdB               = safe_new(m_PdB, m_ant_num);
    m_select            = safe_new(m_select, m_ant_num);
    m_Pmsk              = safe_new(m_Pmsk, m_ant_num);

    m_snrtable          = safe_new(m_snrtable, m_snrtable_len);
    m_snrEtable         = safe_new(m_snrEtable, m_snrtable_len);
    m_diff              = safe_new(m_diff, m_snrtable_len);
    m_SumdataAbs        = safe_new(m_SumdataAbs, m_ant_num);
    m_dataAbs           = safe_new(m_dataAbs, m_ant_num, m_slideSectionlen);
    m_RxBeamforming_out = safe_new(m_RxBeamforming_out, m_ifft_len * 2);
    m_out               = safe_new(m_out, m_ifft_len);

    fix_set(m_in_fxp, 20, 9, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_in_fxp, "m_in_fxp", 1, 100);//18.9
    fix_set(m_ah_in_fxp, 21, 14, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_ah_in_fxp, "m_in_fxp", 1, 100);//
    fix_set(m_ahnorm_fxp, 21, 15, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_ahnorm_fxp, "m_ahnorm_fxp", 1, 10);
    fix_set(m_ahnorm_sum_fxp, 34, 28, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_ahnorm_sum_fxp, "m_ahnorm_sum_fxp", 1, 1);
    fix_set(m_p0_fxp, 18, 14, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_ah_in_fxp, "m_in_fxp", 1, 100);//
    fix_set(m_e0_fxp, 18, 7, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_ah_in_fxp, "m_in_fxp", 1, 100);
    fix_set(m_bf_matrix_in_fxp, 11, 1, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_ah_in_fxp, "m_in_fxp", 1, 100);
    fix_set(m_sqrt_fft_frac, 16, -2, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_ah_in_fxp, "m_in_fxp", 1, 100);//
    fix_set(m_powExp, 16, 8, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_powExp, "m_powExp", 1, 2);33.25
    fix_set(m_dataAbs_fxp, 28, 16, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_dataAbs_fxp, "m_dataAbs_fxp", 1, 2);
    fix_set(m_TmpSumDataAbs_fxp, 30, 24, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_TmpSumDataAbs_fxp, "m_TmpSumDataAbs_fxp", 1, 2);//
    fix_set(m_TmpSumDataAbs_frac_fxp, 10, -17, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_TmpSumDataAbs_frac_fxp, "m_TmpSumDataAbs_frac_fxp", 1, 2);
    fix_set(m_sqrt_dataabs_in_fxp, 22, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sqrt_dataabs_in_fxp, "m_TmpSumDataAbs_frac_fxp", 1, 2);
    fix_set(m_dataNormal_fxp, 20, 8, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_dataNormal_fxp, "m_dataNormal_fxp", 1, 2);
    fix_set(m_dataNormal_tmp_fxp, 24, 15, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_dataNormal_tmp_fxp, "m_dataNormal_fxp", 1, 2);//
    fix_set(m_bf_o_E_fxp, 18, 3, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_bf_o_E_fxp, "m_bf_o_E_fxp", 1, 2);
    fix_set(m_bf_o_P_fxp, 24, 14, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_bf_o_P_fxp, "m_bf_o_P_fxp", 1, 2);
    fix_set_group(m_snrEtable, m_snrtable_len, 15, 3, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_snrEtable, "m_snrEtable", 1, 2);
    fix_set_group(m_snrtable, m_snrtable_len, 18, 7, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_snrEtable, "m_snrEtable", 1, 2);
    fix_set(m_diff_fxp, 18, 2, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_bf_o_E_fxp, "m_bf_o_E_fxp", 1, 2);
    fix_set(m_PdB_fxp, 30, 20, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_PdB_fxp, "m_PdB_fxp", 1, 2);
    fix_set(m_alpha, 11, 1, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_PdB_fxp, "m_PdB_fxp", 1, 2);
    fix_set(m_PdB_tmp, 12, 6, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_PdB_tmp, "m_PdB_tmp", 1, 2);
    fix_set(m_select_fxp, 24, 15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_select_fxp, "m_select_fxp", 1, 2);
    fix_set(m_gwrx_bfselect_the, 3, 3, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_select_fxp, "m_select_fxp", 1, 2);//
    fix_set(m_gwrx_bfselect_alpha, 11, 1, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_select_fxp, "m_select_fxp", 1, 2);
    fix_set(m_TmpP_fxp, 24, 0, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_TmpP_fxp, "m_TmpP_fxp", 1, 1);
    fix_set(m_dataNormal_fxp_in, 16, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_dataNormal_fxp_in, "m_dataNormal_fxp_in", 1, 1);
    fix_set(m_SNRSelect1_fxp, 18, 6, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_SNRSelect1_fxp, "m_SNRSelect1_fxp", 1, 2);//
    fix_set(m_log_in_fxp, 20, 8, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(m_log_in_fxp, "m_log_in_fxp", 1, 2);
    fix_set_group(m_RxBeamforming_out, m_ifft_len * 2, 20, 9, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_select_fxp, "m_select_fxp", 1, 2);//17.8
    fix_set_group(m_out, m_ifft_len, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_select_fxp, "m_select_fxp", 1, 2);

    //double a = sqrt_t(15.6);
    if(m_fft_len == 512)
        m_sqrt_fft_frac = 11585 * DOUBLE_POW2[64 - 18];
    else if(m_fft_len == 1024)
        m_sqrt_fft_frac = 8192 * DOUBLE_POW2[64 - 18];
    else if(m_fft_len == 2048)
        m_sqrt_fft_frac = 5793 * DOUBLE_POW2[64 - 18];
    else if(m_fft_len == 4096)
        m_sqrt_fft_frac = 4096 * DOUBLE_POW2[64 - 18];
    else if(m_fft_len == 8192)
        m_sqrt_fft_frac = 2896 * DOUBLE_POW2[64 - 18];

    m_gwrx_bfselect_the = m_cfg.bf_th;
    m_gwrx_bfselect_alpha = m_cfg.bf_alpha;
    if(m_DSSS == 1)
        m_sqrt_dsss = 1;
    else if(m_DSSS == 2)
        m_sqrt_dsss = 1448 * DOUBLE_POW2[64 - 10];
    else if(m_DSSS == 4)
        m_sqrt_dsss = 2;
    /*       fix_set(m_p0_in, 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_e0_in, 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_BeamformerMatrix, 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_ahnorm, 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_dataNormal, 64, 32, FXP_US, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_TmpSumDataAbs, 64, 32, FXP_US, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_TmpP, 64, 32, FXP_US, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_SNRSelect1, 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_PdB, 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_select, 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_Pmsk, 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_snrtable, 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_snrEtable, 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_diff, 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_SumdataAbs, 64, 32, FXP_US, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_dataAbs, 64, 32, FXP_US, FXP_TRN, FXP_SAT_SYM);*/


    //m_rs_o_bit = safe_new(m_rs_o_bit, m_len_rs_o_bit);

    //mkdir Folder
#ifdef DUMP_MATLAB_BFSelect
    /*struct stat statExist;
    int statMd = 0;
    if (stat(module_name, &statExist) != 0) {
            statMd = mkdir(module_name,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }*/
    char filename_mat[300] = {};
    strcpy(filename_mat, module_name); strcat(filename_mat, "o_sig1.txt");        bf_mat_out1.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "o_sig2.txt");        bf_mat_out2.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "o_sig3.txt");        bf_mat_out3.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "o_sig4.txt");        bf_mat_out4.open(filename_mat);

    //strcpy(filename_mat, module_name); strcat(filename_mat, "cc_in.txt");       fp_mat_cc_in.open(filename_mat);
#endif
	reset();
}

template <class T>
void Cls_BFSelect<T>::reset()
{
    //zeros(m_rs_o_bit, m_len_rs_o_bit);
    zeros(m_in, m_ant_num, m_ifft_len)            ;
    zeros(m_DataIn, m_ant_num, m_ifft_len)        ;
    zeros(m_ah_in, m_ant_num)                     ;
    zeros(m_AHIn, m_ant_num)                      ;
    zeros(m_p0_in, m_ant_num)                     ;
    zeros(m_e0_in, m_ant_num)                     ;

    zeros(m_snrtable, m_snrtable_len)             ;
    zeros(m_snrEtable, m_snrtable_len)            ;

    zeros(m_SumdataAbs, m_ant_num)                ;
    zeros(m_dataAbs, m_ant_num, m_slideSectionlen);
    zeros(m_dataNormal, m_ant_num, m_ifft_len)    ;
    zeros(m_TmpSumDataAbs, m_ant_num)             ;
    zeros(m_TmpP, m_ant_num)                      ;
    zeros(m_SNRSelect1, m_ant_num)                ;
    zeros(m_PdB, m_ant_num)                       ;
    zeros(m_select, m_ant_num)                    ;
    zeros(m_Pmsk, m_ant_num)                      ;
    zeros(m_diff, m_snrtable_len)                 ;

    for(int ck1 = 0; ck1 < m_snrtable_len;ck1++){
        m_snrtable[ck1] = pow(10,PSKTable2[ck1] / 10) / (pow(10,PSKTable2[ck1] / 10) + 1);
        //LOG2DEV<<m_snrtable[ck1] <<endl;
    }

}

template <class T>
void Cls_BFSelect<T>::go()
{
    int i, j;
    for(i = 0; i < m_ant_num; i++) {
        for(j = 0; j < m_ifft_len; j++) {
            m_in_fxp = m_DataIn[i][j];//24.16
            m_in[i][j] = m_in_fxp;
        }
        m_ah_in_fxp = m_AHIn[i];
        m_ah_in[i] = m_ah_in_fxp;
    }
    BFSelectionModule();
}

template <class T>
void Cls_BFSelect<T>::BFSelectionModule()
{
    int i, j;
    double m_ahnorm_sum = 0.0;
    int m_slideSectionlenstart,idxS, idxE;
    double dataNormal_tmp;
    int minidx = 0;
    double minvalue_tmp = 0;
    double maxvalue_tmp = 0;
    double matrix_I = 0;
    double matrix_Q = 0;
    m_bf_o_index = 0;
    for(i = 0; i < m_ant_num; i++){
        m_bf_o_P[i] = 0;
        m_bf_o_E[i] = 0;
    }

    if(m_gw_rxbf_mode == 0) {
        //m_gwrx_bfselect_the = 2.0;
        //m_gwrx_bfselect_alpha = 0.9;
        m_ahnorm_sum_fxp = 0;
        for(i = 0; i < m_ant_num; i++){
            /*m_ahnorm_fxp = m_ah_in[i].power();
            m_ahnorm_sum += m_ahnorm_fxp;*/
            m_ahnorm_sum_fxp += m_ah_in[i].power();//21.14
        }
        //m_ahnorm_sum_fxp = m_ahnorm_sum;
        //m_powExp = m_ahnorm_sum_fxp * m_sqrt_fft_frac * (8192.0 / m_fft_len);
        //m_powExp = m_ahnorm_sum_fxp * m_sqrt_dsss;//sqrt(m_DSSS) ;34.28
        m_powExp = (m_ahnorm_sum_fxp * m_sqrt_dsss) / (m_ifft_len * m_ifft_len);//sqrt(m_DSSS) ;
        m_alpha = m_gwrx_bfselect_alpha;

        zeros(m_SumdataAbs,m_ant_num);
        for(int ij = 0; ij < m_slideDataNum; ij++){
            if(ij == 0) {
                m_slideSectionlenstart = m_slideSectionlen / m_slideDataNum;
            }
            else if(ij == m_slideDataNum - 1) {
                m_slideSectionlenstart = m_slideSectionlen / m_slideDataNum * (-1);
            }
            else{
                m_slideSectionlenstart = 0;
            }
            idxS = ij * m_slideSectionlen + m_slideSectionlenstart;
            idxE = (ij + 1) * m_slideSectionlen + m_slideSectionlenstart;
            for(i = 0; i < m_ant_num; i++){
                for(j = idxS; j < idxE; j++){
                    m_dataAbs_fxp = m_in[i][j].power();//in[i][j].I * in[i][j].I + in[i][j].Q * in[i][j].Q;//16.8
                    m_dataAbs[i][j - idxS] = m_dataAbs_fxp;//28.16
                }
            }

            for(i = 0; i < m_ant_num; i++){
                m_TmpSumDataAbs[i] = 0;
                for(j = 0; j < m_slideSectionlen; j++){
                    m_TmpSumDataAbs[i] += m_dataAbs[i][j];//28.16
                }
                m_TmpSumDataAbs_fxp = m_TmpSumDataAbs[i];//30.24
                m_TmpP_fxp = m_slideSectionlen * 1.0 / m_TmpSumDataAbs_fxp;
                m_TmpP[i] = m_TmpP_fxp;//24.0
                m_SumdataAbs[i] +=  m_TmpSumDataAbs_fxp;
            }
            for(int k = 0; k <  m_ant_num; k++){
                for(int kk = 0;kk < m_slideSectionlen; kk++){
                    m_dataNormal_fxp_in = m_dataAbs[k][kk] * m_TmpP[k];//16.4
                    m_dataNormal_fxp = m_dataNormal_fxp_in * m_dataNormal_fxp_in;
                    m_dataNormal[k][ij * m_slideSectionlen + kk] = m_dataNormal_fxp;//20.8
                }
            }
        }

        for(i = 0; i < m_ant_num; i++){
            m_bf_o_P_fxp = m_SumdataAbs[i] / m_ifft_len;//30.24
            m_bf_o_P[i] = m_bf_o_P_fxp;//24.14
            dataNormal_tmp = 0;
            for(j = 0; j < m_ifft_len; j++) {
                dataNormal_tmp += m_dataNormal[i][j];//20.8
            }
            m_dataNormal_tmp_fxp = dataNormal_tmp;
            m_bf_o_E_fxp = m_dataNormal_tmp_fxp * 1.0 / m_ifft_len;
            m_bf_o_E[i] = m_bf_o_E_fxp;
        }


        for(int ll = 0; ll < m_ant_num; ll++) {
            for(i = 0; i < m_snrtable_len; i++) {
                #ifdef Table_FIX
                m_diff_fxp = fabs(m_bf_o_E[ll] - PSKTable3[i]);
                #else
                m_diff_fxp = fabs(m_bf_o_E[ll] - PSKTable3_fxp[i] * DOUBLE_POW2[64 - 10]);
                #endif // Table_FIX
                m_diff[i] = m_diff_fxp;
                if(i == 0) {
                    minidx =  i;
                    minvalue_tmp = m_diff[i];
                }
                else {
                    if(m_diff[i] < minvalue_tmp) {
                        minidx =  i;
                        minvalue_tmp = m_diff[i];
                    }
                }
            }

            #ifdef Table_FIX
            m_SNRSelect1_fxp = PSKTable2[minidx];
            #else
            m_SNRSelect1_fxp = PSKTable2_fxp[minidx] * DOUBLE_POW2[64 - 10] - (ll + 1) * 6554 * DOUBLE_POW2[64 - 15];
            #endif // Table_FIX
            m_SNRSelect1[ll] = m_SNRSelect1_fxp;

            #ifdef Table_FIX
            m_Pmsk[ll] = m_bf_o_P[ll] * snr_table_double[minidx];
            #else
            m_Pmsk[ll] = m_bf_o_P[ll] * snr_table[minidx] * DOUBLE_POW2[64 - 11];
            #endif // Table_FIX

            m_log_in_fxp = m_Pmsk[ll] * 1.0 / m_powExp;

            m_PdB_tmp = m_log10.run(m_log_in_fxp)  - 1.5;

            if(m_PdB_tmp > 0) {
                if(m_gwrx_bfselect_the == 0) {
                    m_PdB_fxp = 1;
                } else if (m_gwrx_bfselect_the == 2) {
                    m_PdB_fxp = m_PdB_tmp * m_PdB_tmp;
                } else if (m_gwrx_bfselect_the == 4) {
                    m_PdB_fxp = m_PdB_tmp * m_PdB_tmp;
                    m_PdB_fxp = m_PdB_fxp * m_PdB_fxp;
                } else if (m_gwrx_bfselect_the == 8) {
                    m_PdB_fxp = m_PdB_tmp * m_PdB_tmp;
                    m_PdB_fxp = m_PdB_fxp * m_PdB_fxp;
                    m_PdB_fxp = m_PdB_fxp * m_PdB_fxp;
                }
            } else {
                if(m_gwrx_bfselect_the == 2 || m_gwrx_bfselect_the == 0) {
                    m_PdB_fxp = abs(m_PdB_tmp);
                } else if (m_gwrx_bfselect_the == 4) {
                    m_PdB_fxp = m_PdB_tmp * m_PdB_tmp;
                } else if (m_gwrx_bfselect_the == 8) {
                    m_PdB_fxp = m_PdB_tmp * m_PdB_tmp;
                    m_PdB_fxp = m_PdB_fxp * m_PdB_fxp;
                }
            }
            m_PdB[ll] = m_PdB_fxp;
            m_select_fxp = m_alpha * m_SNRSelect1[ll] - (1 - m_alpha * 1.0) * m_PdB[ll];
            m_select[ll] = m_select_fxp;
        }

        for(i = 0; i < m_ant_num; i++) {
            if(i == 0) {
                maxvalue_tmp = m_select[i];
                m_bf_o_index = i;
            }
            else {
                if(m_select[i] > maxvalue_tmp) {
                    maxvalue_tmp = m_select[i];
                    m_bf_o_index = i;
                }
            }
        }
        //m_bf_o_index = 1;

        for(i = 0; i < m_ifft_len; i++) {
            m_RxBeamforming_out[2 * i]     = m_in[m_bf_o_index][i].I;
            m_RxBeamforming_out[2 * i + 1] = m_in[m_bf_o_index][i].Q;
            #ifdef DUMP_MATLAB_BFSelect
                bf_mat_out1 <<fixed<<setprecision(30)<< m_RxBeamforming_out[2 * i] <<endl;
                bf_mat_out1 <<fixed<<setprecision(30)<< m_RxBeamforming_out[2 * i + 1] <<endl;
            #endif
        }
    }
    else if(m_gw_rxbf_mode == 1) {
        for(i = 0; i < m_ifft_len; i++) {
            m_RxBeamforming_out[2 * i]     = m_in[0][i].I;
            m_RxBeamforming_out[2 * i + 1] = m_in[0][i].Q;
            #ifdef DUMP_MATLAB_BFSelect
                bf_mat_out1 <<fixed<<setprecision(30)<< m_RxBeamforming_out[2 * i] <<endl;
                bf_mat_out1 <<fixed<<setprecision(30)<< m_RxBeamforming_out[2 * i + 1] <<endl;
            #endif
        }
    }
    else{

    }
    shift_cal();
    #ifdef DUMP_MATLAB_BFSelect
        for(i = 0; i < m_ant_num; i++){
            bf_mat_out2 <<fixed<<setprecision(30)<< m_bf_o_P[i] <<endl;
            bf_mat_out3 <<fixed<<setprecision(30)<< m_bf_o_E[i] <<endl;
        }
        bf_mat_out4 <<fixed<<setprecision(30)<< m_bf_o_index <<endl;
    #endif
    // Output Temporary
    for(int ck0 = 0; ck0 < m_ifft_len; ck0++) {
        m_out[ck0].I = m_RxBeamforming_out[2 * ck0] * DOUBLE_POW2[64 - (m_shift + 1)];
        m_out[ck0].Q = m_RxBeamforming_out[2 * ck0 + 1] * DOUBLE_POW2[64 - (m_shift + 1)];
    }
}

template <class T>
double Cls_BFSelect<T>::sqrt_t(double in) {
    int times = 0;
    double a;
    int times_int;

    a = in;
    if(in >= 2048 && in <= 4096) {
        times = 0;
    } else if(a < 2048) {
        while(a < 2048) {
            times = times + 1;
            a = in * DOUBLE_POW2[64 + times];
        }
    } else if(a > 4096) {
        while(a > 4096) {
            times = times - 1;
            a = in * DOUBLE_POW2[64 + times];
        }
    }
    a = round(a);
    double root;
    root = sqrt_table[(int)a - 2048] * DOUBLE_POW2[64 - 10];
    if(times > 0) {
        times_int = (int)(times / 2);
        root = root * DOUBLE_POW2[64 - times_int];
        if((times - 2 * times_int) == 1)
            root = root * 46341 * DOUBLE_POW2[64 - 16];// sqrt(2);
    } else if(times < 0) {
        times = times * -1;
        times_int = (int)(times / 2);
        root = root * DOUBLE_POW2[64 + times_int];
        if((times - 2 * times_int) == 1)
            root = root * 92682 * DOUBLE_POW2[64 - 16]; //sqrt(2);
    }
    return root;
}

template <class T>
void Cls_BFSelect<T>::shift_cal() {
    double max_val = 0.000001;
    for (int ck0 = 0; ck0 < m_ifft_len; ck0++){
        if (max_val < fabs(m_RxBeamforming_out[2 * ck0]))
            max_val = fabs(m_RxBeamforming_out[2 * ck0]);
        if (max_val < fabs(m_RxBeamforming_out[2 * ck0 + 1]))
            max_val = fabs(m_RxBeamforming_out[2 * ck0 + 1]);
    }
    m_shift = ceil(log2(max_val));
    //LOG2DEV  << max_val << endl;
}
#endif
