#ifndef DEBUG_H
#define DEBUG_H
#endif

#define detailabort() {  printf("Crapout: Line %d in source file %s\n",__LINE__,__FILE__); abort(); }
