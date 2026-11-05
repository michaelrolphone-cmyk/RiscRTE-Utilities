/* Static selected Runtime loader admission; never target instructions. */
#include "bootstrap/Runtime.h"
#include "cohort_elf_admission.h"
#include <cassert>
int main(int argc,char**argv){
 assert(argc==2);std::ifstream input(argv[1],std::ios::binary);
 std::vector<uint8_t>b(std::istreambuf_iterator<char>(input),{});
 assert(!b.empty()&&CohortElf::admitElf(b.data(),b.size()));
 CohortElf::missing="risc_runtime_get_api";assert(!CohortElf::admitElf(b.data(),b.size()));
 CohortElf::missing=nullptr;CohortElf::safe=false;assert(!CohortElf::admitElf(b.data(),b.size()));
 CohortElf::safe=true;b[0]=0;assert(!CohortElf::admitElf(b.data(),b.size()));
 puts("shared SDR ELF: admitted; missing Runtime import, unsafe state and corrupt header refused");
}
