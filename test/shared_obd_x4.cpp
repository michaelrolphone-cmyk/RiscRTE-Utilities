// Extend exact owning fixture with selected-runtime validation of the new SD ELF.
#define main cohort_main
#include "x4.cpp"
#undef main
int main(int argc,char**argv){
 if(argc!=4)return 2;
 int result=cohort_main(3,argv);if(result)return result;
 std::ifstream input(argv[3],std::ios::binary);if(!input)return 5;
 std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(input),{});
 if(bytes.empty()||!CohortElf::admitElf(bytes.data(),bytes.size()))return 5;
 bytes[0]=0;if(CohortElf::admitElf(bytes.data(),bytes.size()))return 6;
 fprintf(stderr,"OBD_SD_ELF_ADMITTED\n");return 0;
}
