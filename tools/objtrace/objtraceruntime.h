#ifndef LLVM_CORELAB_OBJTRACE_RUNTIME_H
#define LLVM_CORELAB_OBJTRACE_RUNTIME_H

//#include <vector>
#include <map>
#include <algorithm>
#include <set>

  #define DEBUG(fmt, ...) fprintf(stderr, "DEBUG: %s(): " fmt, \
      __func__, ##__VA_ARGS__)

typedef uint64_t FullID;

//typedef std::set<std::pair<const uint64_t, size_t>> AllocInfo;
//typedef std::set<std::pair<const uint64_t, FullID>> LoadStoreInfo;
typedef std::set<size_t> AllocInfo;
typedef std::set<FullID> LoadStoreInfo;
typedef std::map<FullID, AllocInfo> AllocMap;
typedef std::map<FullID, LoadStoreInfo> LoadStoreMap;

#endif
