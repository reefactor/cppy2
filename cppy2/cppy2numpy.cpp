
#define IMCLUDED_FROM_IMPORTNUMPY_CPP
#include "cppy2numpy.hpp"
#undef IMCLUDED_FROM_IMPORTNUMPY_CPP


namespace cppy2 {

void importNumpy() {
  static bool imported = false;
  if (!imported) {
    // @todo use double-lock-singleton pattern against multithreaded race condition
    imported = true;
    import_array();
  }
}


NPY_TYPES toNumpyDType(double) {
  return NPY_DOUBLE;
}


NPY_TYPES toNumpyDType(int) {
  return NPY_INT;
}


}
