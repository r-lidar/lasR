#ifdef UNORDERED
#include <unordered_map>
using namespace std;
typedef std::unordered_map<I32, LASintervalStartCell*> my_cell_hash;
#elif defined(LZ_WIN32_VC6)
#include <hash_map>
using namespace std;
typedef hash_map<I32, LASintervalStartCell*> my_cell_hash;
#else
#include <unordered_map>
using namespace std;
typedef std::unordered_map<I32, LASintervalStartCell*> my_cell_hash;
#endif 